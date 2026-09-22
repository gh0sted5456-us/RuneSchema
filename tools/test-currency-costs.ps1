param([string]$ExportRoot = 'C:\Users\user\Desktop\Main Game')
$ErrorActionPreference = 'Stop'
$root = Join-Path $PSScriptRoot '../examples/Currency'
function Check($condition, $message) { if (!$condition) { throw $message } }
function Read-Data($path) { Get-Content (Join-Path $root $path) -Raw | ConvertFrom-Json -AsHashtable }
function Export-File($path) {
    $package = ($path -split '\.')[0]
    if ($package.StartsWith('/Game/')) { return Join-Path $ExportRoot ($package.Replace('/Game/','Content/') + '.json') }
    $parts = $package.TrimStart('/').Split('/',2)
    return Join-Path $ExportRoot ("Plugins/GameFeatures/$($parts[0])/Content/$($parts[1]).json")
}
$coins = Read-Data 'assets/00-Coins.jsonc'
$buildings = Read-Data 'assets/40-BuildingCosts.jsonc'
$merchants = Read-Data 'assets/50-MerchantCosts.jsonc'
Check ($buildings.Count -eq 779 -and $merchants.Count -eq 118) 'Cost coverage changed; review exported targets.'
foreach ($group in @(@($buildings,'BuildingPieceData','Requirements','Amount'),@($merchants,'RecipeData','ItemsConsumed','Count'))) {
    foreach ($entry in $group[0].GetEnumerator()) {
        $objects = Get-Content (Export-File $entry.Key) -Raw | ConvertFrom-Json -AsHashtable -NoEnumerate
        $source = @($objects | Where-Object { ($_.Package + '.' + $_.Name) -eq $entry.Key })
        Check ($source.Count -eq 1 -and $source[0].Type -eq $group[1]) "Incorrect target: $($entry.Key)"
        # FModel omits fields equal to class defaults (including empty costs).
        # The exact native class is checked above; never infer fields from filenames.
        if ($group[1] -eq 'RecipeData') {
            Check ($source[0].Properties.ContainsKey('ItemsConsumed')) "Purchase has no exported input: $($entry.Key)"
        }
        Check ($entry.Value.Count -eq 1 -and $entry.Value.ContainsKey('$Append')) 'Only append directives are allowed.'
        Check ($entry.Value.'$Append'.Count -eq 1) 'Unexpected cost mutation.'
        $added = $entry.Value.'$Append'[$group[2]]
        Check ($added.Count -eq 1 -and $coins.ContainsKey($added[0].ItemData)) 'Expected one additional coin requirement.'
        Check ($added[0][$group[3]] -gt 0) 'Currency fee must be positive.'
    }
}
$down = Read-Data 'recipes/11-DownConvert.jsonc'
$up = Read-Data 'recipes/10-Exchange.jsonc'
Check ($down.Count -eq 2) 'Expected two reverse exchanges.'
foreach ($pair in @(@('silver','copper'),@('gold','silver'))) {
    $reverse = $down['RECIPE_rs_currency_break_' + $pair[0]].Properties
    $forward = $up['RECIPE_rs_currency_' + $pair[0]].Properties
    Check ($reverse.ItemsConsumed[0].ItemData -eq $forward.ItemsCreated[0].ItemData) 'Wrong reverse input.'
    Check ($reverse.ItemsCreated[0].ItemData -eq $forward.ItemsConsumed[0].ItemData) 'Wrong reverse output.'
    Check ($reverse.ItemsConsumed[0].Count -eq 1 -and $reverse.ItemsCreated[0].Count -eq 100) 'Reverse exchange must be 1:100.'
    Check ($reverse.SkillXPAwardedOnCraft -eq 0 -and $forward.SkillXPAwardedOnCraft -eq 0) 'Currency conversion must not award XP.'
}
Write-Output 'Economy checks passed: 779 building definitions, 118 vendor recipes, append-only costs, and exact reverse exchanges.'
$mint = Read-Data 'recipes/12-Mint.jsonc'
Check ($mint.Count -eq 3) 'Expected three mint recipes.'
foreach ($metal in 'copper','silver','gold') {
    $recipe=$mint['RECIPE_rs_currency_mint_'+$metal]
    Check ($recipe.AddTo[0].Row -eq 'JewelersBench' -and $recipe.AddTo[0].Category -eq 'Mint') 'Wrong mint placement.'
    $properties=$recipe.Properties
    Check ($properties.ItemsConsumed.Count -eq 2) 'Mint requires a bar and essence.'
    Check ($properties.ItemsConsumed[0].ItemData.ToLower().Contains($metal+'bar')) 'Mismatched bar.'
    Check ($properties.ItemsConsumed[1].ItemData -eq '/Game/Gameplay/Items/Resources/Magic/ITEM_Rune_Essence.ITEM_Rune_Essence') 'Mint requires rune essence.'
    foreach ($inputItem in $properties.ItemsConsumed) {
        Check ($inputItem.Count -gt 0 -and (Test-Path (Export-File $inputItem.ItemData))) 'Invalid mint input.'
    }
    Check ($coins.ContainsKey($properties.ItemsCreated[0].ItemData)) 'Unknown minted coin.'
    Check ($properties.ItemsCreated[0].Count -le $coins[$properties.ItemsCreated[0].ItemData].MaxStackSize) 'Mint batch exceeds stack limit.'
    Check ($properties.MinProcessingTime -gt 0 -and $properties.MinProcessingTime -eq $properties.MaxProcessingTime) 'Invalid mint timing.'
    Check ($properties.SkillXPAwardedOnCraft -gt 0) 'Mint XP missing.'
}
Write-Output 'Minting data checks passed; runtime duration, XP, and item consumption require in-game validation.'
