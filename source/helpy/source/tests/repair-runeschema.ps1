param([string]$Script)
$ErrorActionPreference='Stop'
. $Script
$test=Join-Path ([IO.Path]::GetTempPath()) ('RS-repair-test-'+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($test)|Out-Null
function Check($Value,[string]$Message){if(!$Value){throw $Message}}
function Write-Fixture([string]$Path,[string]$Text){[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path))|Out-Null;[IO.File]::WriteAllText($Path,$Text)}
$script:Processes=@()
function Get-CimInstance {return $script:Processes}
function Get-RepairRelease([string]$Version){return [pscustomobject]@{tag_name=$Version;draft=$false;assets=@($script:Asset)}}
function Receive-RepairPackage($Asset,[string]$Path){[IO.File]::Copy($script:Zip,$Path)}
function New-RepairJob {
    $script:Job=Join-Path $test ([Guid]::NewGuid().ToString('N'));[IO.Directory]::CreateDirectory($script:Job)|Out-Null
    $script:Console=Join-Path $script:Job 'console.log';$script:ProgressFile=Join-Path $script:Job 'progress.json';$script:ControlFile=''
}
try {
    $payload=Join-Path $test 'payload/RuneSchema'
    Write-Fixture (Join-Path $payload 'dlls/main.dll') 'expected-main'
    Write-Fixture (Join-Path $payload 'scripts/main.lua') 'expected-script'
    Write-Fixture (Join-Path $payload 'modmenu.json') 'expected-settings-page'
    Write-Fixture (Join-Path $payload 'plugins/RuneSchema.Helpy/RuneSchema.Helpy.dll') 'expected-bridge'
    Write-Fixture (Join-Path $payload 'plugins/RuneSchema.Helpy/Helpy.exe') 'expected-app'
    foreach($plugin in $script:RepairPlugins){Write-Fixture (Join-Path $payload ('plugins/'+$plugin+'/plugin.json')) ('{"Id":"'+$plugin+'","Version":"0.8.1.0"}')}
    foreach($ext in @('pak','utoc','ucas')){Write-Fixture (Join-Path $payload ('plugins/RSNetworking/paks/Bridge.'+$ext)) 'expected-pak'}
    Write-Fixture (Join-Path $payload 'settings/settings.ini') 'must-not-install'
    Write-Fixture (Join-Path $payload 'mods/OtherMod/ID.txt') 'must-not-install'
    $script:Zip=Join-Path $test 'base.zip';[IO.Compression.ZipFile]::CreateFromDirectory((Split-Path $payload),$script:Zip)
    $script:Asset=[pscustomobject]@{name='RuneSchema-0.8.1.0.zip';size=(Get-Item $script:Zip).Length;digest=('sha256:'+(Get-RepairHash $script:Zip));browser_download_url='https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.8.1.0/RuneSchema-0.8.1.0.zip'}
    $root=Join-Path $test 'game/Binaries/Win64/ue4ss/Mods/RuneSchema'
    Write-Fixture (Join-Path $root 'dlls/main.dll') 'broken-main'
    Write-Fixture (Join-Path $root 'plugins/Custom/plugin.json') 'custom-plugin'
    Write-Fixture (Join-Path $root 'settings/settings.ini') 'user-settings'
    Write-Fixture (Join-Path $root 'mods/OtherMod/ID.txt') 'user-mod'
    Write-Fixture (Join-Path $root 'RuneSchema.log') '[warning] Incomplete triplet'
    $data=@{Root=$root;Version='0.8.1.0'}
    $script:Processes=@([pscustomobject]@{ExecutablePath=(Join-Path ([IO.Directory]::GetParent([IO.Directory]::GetParent([IO.Directory]::GetParent($root).FullName).FullName).FullName) 'RSDragonwilds.exe')})
    $refused=$false;try{Assert-RepairClosed $root}catch{$refused=$true};Check $refused 'Active assigned game accepted'
    $script:Processes=@([pscustomobject]@{ExecutablePath=$null})
    $refused=$false;try{Assert-RepairClosed $root}catch{$refused=$true};Check $refused 'Unidentified game process accepted'
    $script:Processes=@()
    New-RepairJob;Invoke-RuneSchemaRepair $data
    Check ((Get-Content (Join-Path $root 'dlls/main.dll') -Raw) -eq 'expected-main') 'Corrupt main DLL not repaired'
    Check ((Get-Content (Join-Path $root 'scripts/main.lua') -Raw) -eq 'expected-script') 'Bundled Lua loader not repaired'
    Check ((Get-Content (Join-Path $root 'settings/settings.ini') -Raw) -eq 'user-settings') 'Settings overwritten'
    Check ((Get-Content (Join-Path $root 'mods/OtherMod/ID.txt') -Raw) -eq 'user-mod') 'User mod overwritten'
    Check ((Get-Content (Join-Path $root 'plugins/Custom/plugin.json') -Raw) -eq 'custom-plugin') 'Third-party plugin overwritten'
    Check ((Get-Content (Join-Path $script:Job 'backup/dlls/main.dll') -Raw) -eq 'broken-main') 'Backup missing'
    Check ((Get-Content $script:Console -Raw).Contains('Incomplete triplet')) 'Log context missing'
    $before=(Get-Item (Join-Path $root 'dlls/main.dll')).LastWriteTimeUtc
    New-RepairJob;Invoke-RuneSchemaRepair $data
    Check ((Get-Item (Join-Path $root 'dlls/main.dll')).LastWriteTimeUtc -eq $before) 'Valid file was unnecessarily replaced'
    # A package checksum failure must occur before the first installed file changes.
    $digest=$script:Asset.digest;$script:Asset.digest='sha256:'+('0'*64);New-RepairJob;$refused=$false
    try{Invoke-RuneSchemaRepair $data}catch{$refused=$_.Exception.Message -match 'checksum'}
    Check $refused 'Bad checksum accepted';$script:Asset.digest=$digest
    $handle=[IO.File]::Open((Join-Path $root 'dlls/main.dll'),'Open','Read','None')
    try{New-RepairJob;$refused=$false;try{Invoke-RuneSchemaRepair $data}catch{$refused=$true};Check $refused 'Loaded/locked main DLL accepted'}finally{$handle.Dispose()}
    # Roll back all previously replaced files if a later atomic replacement fails.
    Write-Fixture (Join-Path $root 'dlls/main.dll') 'rollback-main'
    Write-Fixture (Join-Path $root 'plugins/RuneSchema.Helpy/Helpy.exe') 'rollback-app'
    $script:ReplacementCount=0
    function Install-RepairFile([string]$Stage,[string]$Target){$script:ReplacementCount++;if($script:ReplacementCount -eq 2){throw 'Injected replacement failure'};if(Test-Path -LiteralPath $Target){[IO.File]::Replace($Stage,$Target,[NullString]::Value)}else{[IO.File]::Move($Stage,$Target)}}
    New-RepairJob;$refused=$false;try{Invoke-RuneSchemaRepair $data}catch{$refused=$_.Exception.Message -match 'Injected'}
    Check $refused 'Replacement failure not reported'
    Check ((Get-Content (Join-Path $root 'dlls/main.dll') -Raw) -eq 'rollback-main') 'Main DLL rollback failed'
    Check ((Get-Content (Join-Path $root 'plugins/RuneSchema.Helpy/Helpy.exe') -Raw) -eq 'rollback-app') 'App rollback failed'
    Check (@(Get-ChildItem -LiteralPath $root -Recurse -Filter '*.repair-*').Count -eq 0) 'Temporary replacement file leaked'
    # Archive traversal and incomplete bundled plugin sets are rejected before writing.
    $unsafe=Join-Path $test 'unsafe.zip';$zip=[IO.Compression.ZipFile]::Open($unsafe,'Create');$entry=$zip.CreateEntry('../escape.dll');$zip.Dispose()
    New-RepairJob;$refused=$false;try{Get-RepairEntries $unsafe 'Win64' (Join-Path $script:Job 'stage')}catch{$refused=$true};Check $refused 'Archive traversal accepted'
    Remove-Item -LiteralPath (Join-Path $payload 'plugins/RSNetworking/paks/Bridge.utoc')
    $incomplete=Join-Path $test 'incomplete.zip';[IO.Compression.ZipFile]::CreateFromDirectory((Split-Path $payload),$incomplete)
    New-RepairJob;$refused=$false;try{Get-RepairEntries $incomplete 'Win64' (Join-Path $script:Job 'stage') | Out-Null}catch{$refused=$_.Exception.Message -match 'triplet'};Check $refused 'Incomplete base triplet accepted'
    $script:ControlFile=Join-Path $script:Job 'control.json';Write-Fixture $script:ControlFile '{"Action":"stop"}'
    $refused=$false;try{Test-RepairControl}catch{$refused=$true};Check $refused 'Stop request ignored'
    'Repair checks passed: replacement, unchanged files, log context, backups, checksum refusal, lock refusal, rollback, traversal and cancellation.'
} finally {
    $resolved=[IO.Path]::GetFullPath($test);$parent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
    if([IO.Path]::GetDirectoryName($resolved) -eq $parent -and [IO.Path]::GetFileName($resolved) -match '^RS-repair-test-[0-9a-f]{32}$'){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
