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
Check-Schema vendors '{}' $true
Check-Schema vendors '42' $false
Check-Schema equipment '{"SurgeEvadeLegs":{}}' $true
Check-Schema equipment '{"Unknown":{}}' $false
Check-Schema equipment '{"SurgeEvadeLegs":{"Boots":"yes"}}' $false
Check-Schema raw '[{"$Patch":"DT_Test:Row","$Target":{"Amount":2}}]' $true
Check-Schema players '[{"Archetype":{"Name":"Mage","Icon":"/Game/Test.Test"}}]' $true
Check-Schema players '[{"Archetype":{"Name":"Mage"}}]' $false
Check-Schema players '[{"Nameplate":{"Events":[{"Function":"/Script/Test.Class:Fire","State":"Magic","Parameters":[{"Path":["Id"],"Equals":1}]}]}}]' $true
Check-Schema players '[{"Nameplate":{"Events":[{"Function":"/Script/Test.Class:Fire","State":"Magic","Parameters":[{"Path":["Id"],"Equals":"1"}]}]}}]' $false
$exampleRoot=Join-Path $SchemaDirectory '../examples/QuantumBanker/RuneSchema2VendorTest'
foreach($pair in @(@('npc','npc/50-QuantumBanker.jsonc'),@('dialogue','dialogue/50-QuantumBanker.jsonc'),@('dialogue','dialogue/40-Grumble.jsonc'),@('npc','npc/60-Mortimer.jsonc'),@('dialogue','dialogue/60-Mortimer.jsonc'),@('spawns','spawns/60-MortimerZombies.json'),@('spawns','spawns/62-NightFirepit.jsonc'),@('spawns','spawns/90-GrumbleGoblins.json'),@('events','events/60-MortimerEncounters.json'),@('events','events/90-GrumbleGoblinWaves.json'),@('quests','quests/60-MortimerDebtors.json'),@('quests','quests/60-GraniteGoblinPatrol.jsonc'))) {
    $definition=Get-Content -LiteralPath (Join-Path $exampleRoot $pair[1]) -Raw | ConvertFrom-Json -AsHashtable -NoEnumerate
    if ($pair[0] -eq 'spawns' -and $definition -is [hashtable]) { $definition = ,$definition }
    Check-Schema $pair[0] (ConvertTo-Json -InputObject $definition -Depth 100) $true
}
Check-Schema spawns '[{"Id":"ghost","Type":"AISpawnPoint","EventOnly":true,"AIClass":"/Game/Test.Test_C","VisualEffect":{"Type":"Ghost Glow"}}]' $true
Check-Schema spawns '[{"Id":"ghost","Type":"AISpawnPoint","EventOnly":true,"AIClass":"/Game/Test.Test_C","VisualEffect":{"Type":"Niagara","System":"/Game/Test.Test"}}]' $false
Check-Schema spawns '[{"Type":"AISpawnPoint","Grid":{"Rows":2,"Columns":3,"SpacingMeters":5},"AdditionalDrops":[{"Item":"/Game/Test.Test","Min":1,"Max":3,"ChancePercent":50}]}]' $true
Check-Schema spawns '[{"Grid":{"Rows":0,"Columns":3,"SpacingMeters":5}}]' $false
Check-Schema spawns '[{"AdditionalDrops":[{"Item":"relative","Min":1,"Max":3,"ChancePercent":50}]}]' $false
Check-Schema spawns '[{"AdditionalDrops":[{"Item":"/Game/Test.Test","Min":1,"Max":3,"ChancePercent":101}]}]' $false
Check-Schema npc '[{"Id":"night_vendor","Type":"Prop","Mesh":"/Game/Test.Test","Location":[0,0,0],"TimeOfDay":"Night"}]' $true
Check-Schema npc '[{"Id":"night_vendor","Type":"Prop","Mesh":"/Game/Test.Test","Location":[0,0,0],"TimeOfDay":"Dusk"}]' $false
Check-Schema npc '[{"Id":"lore_keeper","Type":"Prop","Mesh":"/Game/Test.Test","Location":[0,0,0],"LoreID":"History:founding","QuestID":"welcome"}]' $true
Check-Schema npc '[{"Id":"ambiguous_lore","Type":"Prop","Mesh":"/Game/Test.Test","Location":[0,0,0],"LoreID":"history","LoreEntry":"legacy"}]' $false
Write-Output "Loader schemas: $checks positive/negative checks passed."
