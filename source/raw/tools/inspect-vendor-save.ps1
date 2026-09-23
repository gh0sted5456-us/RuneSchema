param([Parameter(Mandatory=$true)][string]$Path)
$ErrorActionPreference = 'Stop'
# Read-only, narrow SPUD chunk inspection. No save serialization or repair.
# Based on sinbad/SPUD SpudData.cpp with observed Dragonwilds level extensions.
$saveBytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Path).Path)
function Chunk([long]$Offset, [long]$Limit) {
    if ($Offset -lt 0 -or $Offset + 8 -gt $Limit) { throw 'Truncated chunk header' }
    $tag = [Text.Encoding]::ASCII.GetString($saveBytes, $Offset, 4)
    $size = [BitConverter]::ToUInt32($saveBytes, $Offset + 4)
    $end = $Offset + 8 + $size
    if ($tag -notmatch '^[A-Z]{4}$' -or $end -gt $Limit) { throw "Invalid chunk at $Offset" }
    [pscustomobject]@{Tag=$tag;Start=$Offset;Data=$Offset+8;End=$end}
}
function Children([long]$Offset, [long]$Limit) {
    while ($Offset -lt $Limit) { $c = Chunk $Offset $Limit; $c; $Offset = $c.End }
}
function ReadString([ref]$Offset, [long]$Limit) {
    if ($Offset.Value + 4 -gt $Limit) { throw 'Truncated string length' }
    $count = [BitConverter]::ToInt32($saveBytes, $Offset.Value)
    $Offset.Value += 4
    if ($count -eq 0) { return '' }
    $size = if ($count -gt 0) { [long]$count } else { -2L * $count }
    if ($size -gt 1048576 -or $Offset.Value + $size -gt $Limit) { throw 'Invalid string length' }
    $value = if ($count -gt 0) {
        [Text.Encoding]::UTF8.GetString($saveBytes, $Offset.Value, $size)
    } else { [Text.Encoding]::Unicode.GetString($saveBytes, $Offset.Value, $size) }
    $Offset.Value += $size
    $value.TrimEnd([char]0)
}
$root = Chunk 0 $saveBytes.Length
if ($root.Tag -ne 'SAVE' -or $root.End -ne $saveBytes.Length) { throw 'Not a complete SPUD SAVE chunk' }
$levels = @(Children $root.Data $root.End | Where-Object Tag -EQ 'LVLS')
if ($levels.Count -ne 1) { throw 'Expected one LVLS container' }
$vendorRecords = @()
$levelCount = 0
$spawnCount = 0
foreach ($level in (Children $levels[0].Data $levels[0].End)) {
    if ($level.Tag -ne 'LEVL') { continue }
    ++$levelCount
    $p = [long]$level.Data
    $name = ReadString ([ref]$p) $level.End
    # Dragonwilds stores eight extension bytes before META; do not interpret them.
    if ([Text.Encoding]::ASCII.GetString($saveBytes, $p, 4) -ne 'META') { $p += 8 }
    $parts = @(Children $p $level.End)
    $meta = @($parts | Where-Object Tag -EQ 'META')
    if ($meta.Count -ne 1) { throw 'Missing level metadata' }
    $cnix = @(Children $meta[0].Data $meta[0].End | Where-Object Tag -EQ 'CNIX')
    if ($cnix.Count -ne 1) { throw 'Missing class name index' }
    $p = [long]$cnix[0].Data
    $count = [BitConverter]::ToUInt32($saveBytes, $p); $p += 4
    if ($count -gt 100000) { throw 'Invalid class count' }
    $classes = @()
    for ($i = 0; $i -lt $count; ++$i) { $classes += ReadString ([ref]$p) $cnix[0].End }
    if ($p -ne $cnix[0].End) { throw 'Unsupported class index layout' }
    foreach ($list in ($parts | Where-Object Tag -EQ 'SATS')) {
        foreach ($actor in (Children $list.Data $list.End)) {
            if ($actor.Tag -ne 'SPWN' -or $actor.End - $actor.Data -lt 20) { throw 'Unsupported spawned actor record' }
            ++$spawnCount
            $id = [BitConverter]::ToUInt32($saveBytes, $actor.Data)
            if ($id -ge $classes.Count) { throw 'Actor class index outside metadata' }
            if ($classes[$id] -notlike '*/BP_BaseInteractableNPC.BP_BaseInteractableNPC_C') { continue }
            $words = @(0..3 | ForEach-Object { [BitConverter]::ToUInt32($saveBytes, $actor.Data + 4 + 4 * $_) })
            $vendorRecords += [ordered]@{
                Level=$name; Class=$classes[$id]; RecordOffset=$actor.Start
                GuidWords=$words; GuidHex=($words | ForEach-Object { '{0:X8}' -f $_ }) -join ''
                RecordBytes=$actor.End-$actor.Start
            }
        }
    }
}
[ordered]@{
    SaveSHA256=(Get-FileHash -LiteralPath $Path).Hash
    Levels=$levelCount; SpawnedActorRecords=$spawnCount; BaseNPCRecords=$vendorRecords
    Limitations='Reads class/GUID from chunk structure only. Does not establish RuneSchema ownership or decode mesh/UI/merchant properties.'
} | ConvertTo-Json -Depth 6
