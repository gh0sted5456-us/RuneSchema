param([string]$ExportRoot = 'C:\Users\user\Desktop\Main Game')
$ErrorActionPreference = 'Stop'
$root = Join-Path $PSScriptRoot '../examples/Currency'
function Read-Data($name) { Get-Content (Join-Path $root $name) -Raw | ConvertFrom-Json -AsHashtable }
function Check($condition, $message) { if (!$condition) { throw $message } }
$assets = Read-Data 'assets/00-Coins.jsonc'
$recipes = Read-Data 'recipes/10-Exchange.jsonc'
$journal = Read-Data 'journal/20-Currency.jsonc'
$drops = (Read-Data 'raw/30-CoinDrops.jsonc').DT_LootDropTable
$ids = @{}
Check ($assets.Count -eq 3) 'Expected three coin assets.'
foreach ($entry in $assets.GetEnumerator()) {
    $coin = $entry.Value
    Check ($coin.PersistenceID -cmatch '^[A-Za-z0-9_-]{21}[AQgw]$') 'Noncanonical save identity.'
    Check (!$ids.ContainsKey($coin.PersistenceID)) 'Duplicate save identity.'
    $ids[$coin.PersistenceID] = $true
    Check ($coin.Weight -eq 0 -and !$coin.bDropOnDeath) 'Coin inventory policy changed.'
    Check ($coin.ItemFilterTags.GameplayTags[0].TagName -eq 'ItemFilter.Type.Equipment.Ammo.Rune') 'Missing rune routing tag.'
    $expectedStack = if ($coin.InternalName.EndsWith('gold')) {99} else {9999}
    Check ($coin.MaxStackSize -eq $expectedStack) 'Incorrect stack size.'
    $metal = (Get-Culture).TextInfo.ToTitleCase($coin.InternalName.Replace('rs_currency_', ''))
    Check ($coin.Icon.AssetPathName -ceq "/Game/Mods/ColorsofMoney/Icons/T_Icon_${metal}_Coin.T_Icon_${metal}_Coin") 'Incorrect custom icon path.'
    foreach ($ref in @($coin.'$Clone')) {
        $relative = ($ref -split '\.')[0].Replace('/DowdunReach/', 'Plugins/GameFeatures/DowdunReach/Content/') + '.json'
        Check (Test-Path (Join-Path $ExportRoot $relative)) "Missing exported coin dependency: $ref"
    }
}
Check ($recipes.Count -eq 2) 'Expected two exchange recipes.'
foreach ($pair in @(@('copper','silver'), @('silver','gold'))) {
    $recipe = $recipes['RECIPE_rs_currency_' + $pair[1]]
    Check ($recipe.AddTo[0].Row -eq 'JewelersBench') 'Wrong exchange station.'
    $inputItem = $recipe.Properties.ItemsConsumed[0]
    $outputItem = $recipe.Properties.ItemsCreated[0]
    Check ($inputItem.Count -eq 100 -and $outputItem.Count -eq 1) 'Exchange must be 100:1.'
    Check ($assets[$inputItem.ItemData].InternalName -eq "rs_currency_$($pair[0])") 'Wrong exchange input.'
    Check ($assets[$outputItem.ItemData].InternalName -eq "rs_currency_$($pair[1])") 'Wrong exchange output.'
}
$stock = (Get-Content (Join-Path $ExportRoot 'Content/Gameplay/Items/LootDropTables/DT_LootDropTable.json') -Raw | ConvertFrom-Json -AsHashtable -NoEnumerate)[0].Rows
foreach ($row in $drops.GetEnumerator()) {
    Check ($stock.ContainsKey($row.Key)) "Unknown loot row: $($row.Key)"
    Check ($row.Value.Count -eq 1 -and $row.Value.ContainsKey('$Append')) 'Loot must append, never replace.'
    foreach ($reward in $row.Value.'$Append'.Resources) {
        Check ($assets.ContainsKey($reward.SpawnedItemData)) 'Unknown coin reward.'
        Check ($reward.MinimumDropAmount -gt 0 -and $reward.MaximumDropAmount -ge $reward.MinimumDropAmount) 'Invalid reward range.'
        Check ($reward.DropChance -gt 0 -and $reward.DropChance -le 100) 'Invalid drop chance.'
    }
}
Check ($journal.Count -eq 3) 'Expected three journal entries.'
foreach ($page in $journal.Values) {
    Check ($assets.ContainsKey($page.ItemData)) 'Journal item missing.'
    if ($page.ContainsKey('RecipeData')) { Check ($recipes.ContainsKey($page.RecipeData)) 'Journal recipe missing.' }
}
$purses = Read-Data 'assets/05-CoinPurse.jsonc'
Check ($purses.Count -eq 1) 'Expected one consumable purse.'
$pursePath = '/Game/RuneSchema/Currency/Items/rs_currency_purse.rs_currency_purse'
$purse = $purses[$pursePath]
Check ($purse.PersistenceID -cmatch '^[A-Za-z0-9_-]{21}[AQgw]$' -and !$ids.ContainsKey($purse.PersistenceID)) 'Invalid purse save identity.'
Check ($purse.'$Clone' -eq '/Game/Gameplay/Items/Consumables/Misc/ITEM_Consumable_ZombiePack.ITEM_Consumable_ZombiePack') 'Purse must inherit native consumable emitter behavior.'
Check ($purse.Icon.AssetPathName -ceq '/Game/Mods/ColorsofMoney/Icons/T_Icon_Purse_Coin.T_Icon_Purse_Coin') 'Incorrect purse icon path.'
Check ($purse.'Items to Drop'.Count -eq 3) 'Expected three independent coin rolls.'
$index = 0
foreach ($reward in $purse.'Items to Drop') {
    Check ($assets.ContainsKey($reward.ItemDataClass.AssetPathName)) 'Unknown purse coin reference.'
    Check ($reward.ProbabilityOfDrop -eq @(1.0,0.25,0.01)[$index]) 'Incorrect default purse probability.'
    Check ($reward.MinToDrop -eq @(25,1,1)[$index] -and $reward.MaxToDrop -eq @(75,3,1)[$index]) 'Incorrect purse quantities.'
    Check ($reward.MaximumGrouping -ge $reward.MaxToDrop) 'Purse grouping smaller than reward.'
    $index++
}
foreach ($row in (Read-Data 'raw/35-PurseDrops.jsonc').DT_LootDropTable.GetEnumerator()) {
    Check ($stock.ContainsKey($row.Key)) 'Unknown purse loot row.'
    Check ($row.Value.ContainsKey('$Append')) 'Purse drops must append.'
    $reward = $row.Value.'$Append'.Resources[0]
    Check ($reward.SpawnedItemData -eq $pursePath -and $reward.DropChance -eq 15) 'Incorrect purse loot reference or chance.'
}
foreach ($page in $journal.Values) {
    Check ($page.Image -eq $assets[$page.ItemData].Icon.AssetPathName) 'Journal icon differs from coin icon.'
}
Write-Output 'Currency checks passed: coins, exchanges, journal, native purse rewards, custom icon paths, and stock loot rows. Cooked custom textures require in-game validation.'
