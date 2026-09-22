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
    for($delta=[long]($stackPointer-$start);$delta -lt [Math]::Min($size,($stackPointer-$start)+4096);$delta+=8) {
        $value=Read64 ($rva+$delta)
        $description=Describe $value
        if($description) { 'SP+0x{0:X}: {1}' -f ($start+$delta-$stackPointer),$description }
    }
}
