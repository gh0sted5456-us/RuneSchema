param([Parameter(Mandatory)][string]$SchemaDirectory)
$ErrorActionPreference = 'Stop'
$checks = 0
function Check-Schema([string]$Name, [string]$Json, [bool]$Expected) {
    $valid = Test-Json -Json $Json -SchemaFile (Join-Path $SchemaDirectory "$Name.schema.json") -ErrorAction SilentlyContinue
    if ($valid -ne $Expected) { throw "Schema check failed: $Name, expected $Expected; $Json" }
    $script:checks++
}
foreach ($name in 'assets','blueprints','buildings','courses','enums','journal','raw','recipes','strings') {
    Check-Schema $name '{}' $true
    Check-Schema $name '42' $false
}
foreach ($name in 'players','spawns') {
    Check-Schema $name '[]' $true
    Check-Schema $name '{}' $false
}
Check-Schema vendors '[]' $true
Check-Schema vendors '{}' $false
Check-Schema vendors '42' $false
Check-Schema equipment '{"SurgeEvadeLegs":{}}' $true
Check-Schema equipment '{"Unknown":{}}' $false
Check-Schema equipment '{"SurgeEvadeLegs":{"Boots":"yes"}}' $false
Check-Schema assets '{"/Game/Test.Test":{"$WearableBridge":true}}' $true
Check-Schema assets '{"/Game/Test.Test":{"$WearableBridge":"true"}}' $false
Check-Schema assets '{"/Game/Scroll.Scroll":{"$QuestScroll":{"QuestID":"Dye:rat_work","PersistenceID":"VT39acMY4k62LnArwSMkEQ"}}}' $true
Check-Schema assets '{"/Game/Scroll.Scroll":{"$QuestScroll":{"QuestID":"Dye:rat_work","PersistenceID":"bad"}}}' $false
Check-Schema assets '{"/Game/Scroll.Scroll":{"$QuestScroll":{"QuestID":"Dye:rat_work","PersistenceID":"VT39acMY4k62LnArwSMkEQ","Action":"reward"}}}' $false
Check-Schema registry '{"SchemaVersion":1,"Entries":[],"NativeRegistries":{"Items":[{"Path":"/Game/Items/Example.Example","Source":"Schema"}]}}' $true
Check-Schema registry '{"SchemaVersion":1,"Entries":[],"NativeRegistries":{"Recipes":["/Mod/Recipes/Example.Example"]}}' $true
Check-Schema registry '{"SchemaVersion":1,"Entries":[],"NativeRegistries":{"Items":[{"Path":"/Game/Items/Example.Example","Source":"Unknown"}]}}' $false
Check-Schema registry '{"SchemaVersion":1,"Entries":[],"NativeRegistries":{"Unknown":["/Game/Test.Test"]}}' $false
Check-Schema registry '{"SchemaVersion":1,"Entries":[],"NativeRegistries":{"Recipes":[42]}}' $false
# Repair behavior belongs to the cooked item, not an intercepting runtime action.
Check-Schema registry '{"SchemaVersion":1,"Entries":[{"Id":"Kit","Kind":"GameplayAuthority","Authority":{"Action":"RepairPersonalEquipment","GraphClass":"/Example/BP_Kit.BP_Kit_C","DataAsset":"/Example/Kit.Kit","Function":"UseItem"}}]}' $false
foreach($lane in 'ContainerContents','FarmPlants','Trees','TreeGrowth','FixedTextNotifications','FTUENotifications','PlayerHooks','WorldHooks'){
    Check-Schema registry ('{"SchemaVersion":1,"Entries":[],"NativeRegistries":{"'+$lane+'":["/SharedPlugin/Data/Asset.Asset"]}}') $true
    Check-Schema registry ('{"SchemaVersion":1,"Entries":[],"NativeRegistries":{"'+$lane+'":[{"Path":"/SharedPlugin/Data/Asset.Asset","Source":"Schema"}]}}') $false
    Check-Schema registry ('{"SchemaVersion":1,"Entries":[],"NativeRegistries":{"'+$lane+'":["/Script/Dominion.Type"]}}') $false
}
Check-Schema registry '{"SchemaVersion":1,"Entries":[],"NativeRegistries":{"Difficulty":["/Example/Data.Asset"]}}' $false
Check-Schema registry '{"SchemaVersion":1,"Entries":[],"NativeRegistries":{"Entitlements":["/Example/Data.Asset"]}}' $false
foreach($lane in 'Skills','SkillPerks','Enums') {
    Check-Schema registry ('{"SchemaVersion":1,"Entries":[],"NativeRegistries":{"'+$lane+'":["/Game/Mods/Example/Content/Asset.Asset"]}}') $true
    Check-Schema registry ('{"SchemaVersion":1,"Entries":[],"NativeRegistries":{"'+$lane+'":[{"Path":"/Example/Content/Asset.Asset","Source":"Schema"}]}}') $false
    Check-Schema registry ('{"SchemaVersion":1,"Entries":[],"NativeRegistries":{"'+$lane+'":["/Script/Dominion.ESkill"]}}') $false
}
Check-Schema raw '[{"$Patch":"DT_Test:Row","$Target":{"Amount":2}}]' $true
Check-Schema players '[{"Archetype":{"Name":"Mage","Icon":"/Game/Test.Test"}}]' $true
Check-Schema players '[{"Archetype":{"Name":"Mage"}}]' $false
Check-Schema players '[{"Nameplate":{"Events":[{"Function":"/Script/Test.Class:Fire","State":"Magic","Parameters":[{"Path":["Id"],"Equals":1}]}]}}]' $true
Check-Schema players '[{"Nameplate":{"Events":[{"Function":"/Script/Test.Class:Fire","State":"Magic","Parameters":[{"Path":["Id"],"Equals":"1"}]}]}}]' $false
$exampleRoot=Join-Path $SchemaDirectory '../examples/RSv16/ExampleMods/RuneSchema2VendorTest'
foreach($pair in @(@('npc','npc/50-QuantumBanker.jsonc'),@('dialogue','dialogue/50-QuantumBanker.jsonc'),@('dialogue','dialogue/40-Grumble.jsonc'),@('npc','npc/60-Mortimer.jsonc'),@('dialogue','dialogue/60-Mortimer.jsonc'),@('spawns','spawns/60-MortimerZombies.json'),@('spawns','spawns/62-NightFirepit.jsonc'),@('spawns','spawns/90-GrumbleGoblins.json'),@('events','events/60-MortimerEncounters.json'),@('events','events/90-GrumbleGoblinWaves.json'),@('quests','quests/60-MortimerDebtors.json'),@('quests','quests/60-GraniteGoblinPatrol.jsonc'))) {
    $definition=Get-Content -LiteralPath (Join-Path $exampleRoot $pair[1]) -Raw | ConvertFrom-Json -AsHashtable -NoEnumerate
    if ($pair[0] -eq 'spawns' -and $definition -is [hashtable]) { $definition = ,$definition }
    Check-Schema $pair[0] (ConvertTo-Json -InputObject $definition -Depth 100) $true
}
Check-Schema spawns '[{"Id":"ghost","Type":"AISpawnPoint","EventOnly":true,"AIClass":"/Game/Test.Test_C","VisualEffect":{"Type":"Ghost Glow"}}]' $true
Check-Schema spawns '[{"Id":"furnace","Type":"BuildingPiece","Building":"/Game/Test.Test","Location":[1,2,"$"],"DuplicateRadius":100,"AllowDeconstruction":true}]' $true
Check-Schema spawns '[{"Id":"furnace","Type":"BuildingPiece","Building":"/Game/Test.Test","Location":[1,2,"$"],"DuplicateRadius":0}]' $false
Check-Schema spawns '[{"Id":"furnace","Type":"BuildingPiece","Building":"/Game/Test.Test","Location":[1,2,"$"],"AllowDeconstruction":false}]' $false
Check-Schema spawns '[{"Id":"ghost","Type":"AISpawnPoint","EventOnly":true,"AIClass":"/Game/Test.Test_C","VisualEffect":{"Type":"Niagara","System":"/Game/Test.Test"}}]' $false
Check-Schema spawns '[{"Type":"AISpawnPoint","Grid":{"Rows":2,"Columns":3,"SpacingMeters":5},"AdditionalDrops":[{"Item":"/Game/Test.Test","Min":1,"Max":3,"ChancePercent":50}]}]' $true
Check-Schema spawns '[{"Grid":{"Rows":0,"Columns":3,"SpacingMeters":5}}]' $false
Check-Schema spawns '[{"AdditionalDrops":[{"Item":"relative","Min":1,"Max":3,"ChancePercent":50}]}]' $false
Check-Schema spawns '[{"AdditionalDrops":[{"Item":"/Game/Test.Test","Min":1,"Max":3,"ChancePercent":101}]}]' $false
Check-Schema npc '[{"Id":"night_vendor","Type":"Prop","Mesh":"/Game/Test.Test","Location":[0,0,0],"TimeOfDay":"Night"}]' $true
Check-Schema npc '[{"Id":"night_vendor","Type":"Prop","Mesh":"/Game/Test.Test","Location":[0,0,0],"TimeOfDay":"Dusk"}]' $false
Check-Schema npc '[{"Id":"lore_keeper","Type":"Prop","Mesh":"/Game/Test.Test","Location":[0,0,0],"LoreID":"History:founding","QuestID":"welcome"}]' $true
Check-Schema npc '[{"Id":"ambiguous_lore","Type":"Prop","Mesh":"/Game/Test.Test","Location":[0,0,0],"LoreID":"history","LoreEntry":"legacy"}]' $false
Check-Schema npc '[{"Id":"journal_vendor","Type":"Prop","Mesh":"/Game/Test.Test","Location":[0,0,0],"VendorID":"shop","JournalEntries":["history","OtherMod:person","/Mod/Journal/Entry.Entry"]}]' $true
Check-Schema npc '[{"Id":"journal_vendor","Type":"Prop","Mesh":"/Game/Test.Test","Location":[0,0,0],"JournalEntries":[]}]' $false
Check-Schema npc '[{"Id":"journal_vendor","Type":"Prop","Mesh":"/Game/Test.Test","Location":[0,0,0],"JournalEntries":["history","history"]}]' $false
Check-Schema npc '[{"Id":"journal_vendor","Type":"Prop","Mesh":"/Game/Test.Test","Location":[0,0,0],"JournalEntries":[42]}]' $false
$alternative=@{Id='work';PersistenceID='VT39acMY4k62LnArwSMkEQ';Title='Work';Description='Either route';Reward=@{Item='/Test/Reward.Reward';Count=1};Stages=@(@{Id='work';Completion='Any';Objectives=@(@{Id='a';Text='A';Item='/Test/A.A';Count=50},@{Id='b';Text='B';Type='Kill';AIClasses=@('/Test/Rat.Rat_C');Count=50})})}
Check-Schema quests (ConvertTo-Json $alternative -Depth 30) $true
$alternative.Stages[0].Objectives[0].Optional=$true
Check-Schema quests (ConvertTo-Json $alternative -Depth 30) $false
$alternative.Stages[0].Objectives[0].Remove('Optional')
$alternative.Stages[0].Objectives=@($alternative.Stages[0].Objectives[0])
Check-Schema quests (ConvertTo-Json $alternative -Depth 30) $false
$human=@{Id='keeper';DisplayName='Keeper';Type='Human';Location=@(0,0,0);Appearance=@{BodyType='A';Head='A';HairPreset='A';FacialHairPreset='A';HairColor='A';SkinTone='A';EyeColor='A';EyebrowColor='A'};EquipmentWhen=@(@{Id='helmet';QuestCompleted='Test:final';Policy='WorldEverCompleted';Equipment=@{Head='/Test/Helmet.Helmet'}})}
Check-Schema npc (ConvertTo-Json $human -Depth 30) $true
$human.EquipmentWhen[0].Policy='PerPlayer'
Check-Schema npc (ConvertTo-Json $human -Depth 30) $false
$human.EquipmentWhen[0].Policy='WorldEverCompleted'
$human.EquipmentWhen[0].Equipment=@{MainHand='/Test/Sword.Sword'}
Check-Schema npc (ConvertTo-Json $human -Depth 30) $false
Write-Output "Loader schemas: $checks positive/negative checks passed."
