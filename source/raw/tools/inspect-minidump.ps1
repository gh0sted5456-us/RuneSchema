param([Parameter(Mandatory=$true)][string]$Path)
$bytes=[IO.File]::ReadAllBytes($Path)
function Read32([long]$offset) { [BitConverter]::ToUInt32($bytes,[int]$offset) }
function Read64([long]$offset) { [BitConverter]::ToUInt64($bytes,[int]$offset) }
$streams=@{}
$directory=Read32 12
for($i=0;$i -lt (Read32 8);$i++) {
    $offset=$directory+12*$i
    $streams[(Read32 $offset)]=(Read32 ($offset+8))
}
$exception=$streams[[uint32]6]
if(!$exception) { throw 'No exception stream' }
$threadId=Read32 $exception
$address=Read64 ($exception+24)
$context=Read32 ($exception+164)
$stackPointer=Read64 ($context+152)
'Exception=0x{0:X} Thread={1} RIP=0x{2:X} RCX=0x{3:X} RDX=0x{4:X}' -f (Read32 ($exception+8)),$threadId,$address,(Read64 ($context+128)),(Read64 ($context+136))
$parameterCount=[Math]::Min([int](Read32 ($exception+32)),15)
if($parameterCount) {
    $parameters=for($i=0;$i -lt $parameterCount;$i++) { '0x{0:X}' -f (Read64 ($exception+40+8*$i)) }
    'Exception parameters: '+($parameters -join ', ')
}
$modules=@()
$moduleStream=$streams[[uint32]4]
for($i=0;$i -lt (Read32 $moduleStream);$i++) {
    $offset=$moduleStream+4+108*$i
    $nameOffset=Read32 ($offset+20)
    $name=[Text.Encoding]::Unicode.GetString($bytes,$nameOffset+4,(Read32 $nameOffset))
    $modules += [pscustomobject]@{Base=(Read64 $offset);Size=(Read32 ($offset+8));Name=$name}
}
function Describe([uint64]$value) {
    foreach($module in $modules) {
        if($value -ge $module.Base -and ($value-$module.Base) -lt $module.Size) {
            return ('{0}+0x{1:X}' -f [IO.Path]::GetFileName($module.Name),($value-$module.Base))
        }
    }
    return $null
}
'Fault: '+(Describe $address)
$threadStream=$streams[[uint32]3]
for($i=0;$i -lt (Read32 $threadStream);$i++) {
    $offset=$threadStream+4+48*$i
    if((Read32 $offset) -ne $threadId) { continue }
    $start=Read64 ($offset+24)
    $size=Read32 ($offset+32)
    $rva=Read32 ($offset+36)
    'Stack candidates (raw scan, not an unwound call stack):'
    $preferred='RuneSchema|UE4SS|RSDragonwilds|server|Shipping'
    $preferredRows=@()
    $otherRows=@()
    for($delta=[long]($stackPointer-$start);$delta -lt $size;$delta+=8) {
        $value=Read64 ($rva+$delta)
        $description=Describe $value
        if(!$description) { continue }
        $row='SP+0x{0:X}: {1}' -f ($start+$delta-$stackPointer),$description
        if($description -match $preferred) { $preferredRows += $row }
        elseif($otherRows.Count -lt 80) { $otherRows += $row }
    }
    if($preferredRows.Count) { 'Preferred module candidates:'; $preferredRows | Select-Object -First 160 }
    'Other module candidates (first 80):'; $otherRows
}

'Preferred candidates on all captured threads (raw scan):'
for($i=0;$i -lt (Read32 $threadStream);$i++) {
    $offset=$threadStream+4+48*$i
    $candidateThread=Read32 $offset
    $candidateContextRva=Read32 ($offset+44)
    if(!$candidateContextRva) { continue }
    $candidateStackPointer=Read64 ($candidateContextRva+152)
    $candidateStart=Read64 ($offset+24)
    $candidateSize=Read32 ($offset+32)
    $candidateRva=Read32 ($offset+36)
    $rows=@()
    $begin=[long]($candidateStackPointer-$candidateStart)
    if($begin -lt 0 -or $begin -ge $candidateSize) { continue }
    for($delta=$begin;$delta -lt $candidateSize;$delta+=8) {
        $description=Describe (Read64 ($candidateRva+$delta))
        if($description -and $description -match 'RuneSchema|UE4SS|RSDragonwilds|Shipping') {
            $rows += ('SP+0x{0:X}: {1}' -f ($candidateStart+$delta-$candidateStackPointer),$description)
            if($rows.Count -ge 48) { break }
        }
    }
    if($rows.Count) { "Thread ${candidateThread}:"; $rows }
}
