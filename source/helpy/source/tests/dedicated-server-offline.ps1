param([Parameter(Mandatory)][string]$Helper)
$ErrorActionPreference='Stop'
$taskDirectory=Join-Path ([IO.Path]::GetTempPath()) ('rsmk-offline-'+[Guid]::NewGuid().ToString('N'))
$taskDirectory=[IO.Path]::GetFullPath($taskDirectory)
$taskConfig=Join-Path $taskDirectory 'WindowsServer'
New-Item -ItemType Directory -Path $taskConfig | Out-Null
$taskIni=Join-Path $taskConfig 'DedicatedServer.ini'
$taskChanges=Join-Path $taskConfig 'DedicatedServer.ini.helpy-changes-test.json'
$taskEncoding=[Text.UTF8Encoding]::new($false)
$global:RsmkOfflineFixtureRunning=$false
# The helper is exercised only on disposable files. No real process is stopped
# and no production INI is used. The running-server branch is tested explicitly.
function Get-CimInstance {if($global:RsmkOfflineFixtureRunning){[pscustomobject]@{Name='RSDragonwildsServer-Win64-Shipping.exe'}}}
function Assert-Value([bool]$value,[string]$message){if(-not $value){throw $message}}
function Write-Request([string]$key='ServerName',[string]$expected='Example',[string]$value='Reviewed'){
    $request=@{Version=1;Target=$taskIni;Changes=@(@{Section='/Script/Dominion.DedicatedServerSettings';Key=$key;Expected=$expected;Value=$value})}
    [IO.File]::WriteAllText($taskChanges,($request|ConvertTo-Json -Depth 5),$taskEncoding)
}
function Assert-Rejected([string]$label) {
    $before=[IO.File]::ReadAllText($taskIni,$taskEncoding);$rejected=$false
    try{& $Helper -Changes $taskChanges | Out-Null}catch{$rejected=$true}
    Assert-Value $rejected ('Unsafe offline apply was accepted: '+$label)
    Assert-Value ([IO.File]::ReadAllText($taskIni,$taskEncoding) -ceq $before) 'Rejected apply changed the INI'
}
try{
    $source=[char]0xFEFF+"; retain comment`r`n[/Script/Dominion.DedicatedServerSettings]`r`nServerName=Example`r`nKnownPlayerList=(UserId=fixture,Privileges=(PrivilegeMask=14),LastAdminPassword=not-a-real-password)`r`n+Unknown=one`r`n+Unknown=two"
    [IO.File]::WriteAllText($taskIni,$source,$taskEncoding)
    Write-Request
    $global:RsmkOfflineFixtureRunning=$true;Assert-Rejected 'running server';$global:RsmkOfflineFixtureRunning=$false
    Write-Request -key KnownPlayerList;Assert-Rejected 'managed record'
    Write-Request -expected Stale;Assert-Rejected 'stale value'
    Write-Request
    # The game's native bookkeeping can change during shutdown. Only reviewed
    # scalar baselines are compared; the latest managed records are preserved.
    $current=$source.Replace('UserId=fixture','UserId=updated-fixture')
    [IO.File]::WriteAllText($taskIni,$current,$taskEncoding)
    & $Helper -Changes $taskChanges | Out-Null
    Assert-Value ([IO.File]::ReadAllText($taskIni,$taskEncoding) -ceq $current.Replace('ServerName=Example','ServerName=Reviewed')) 'Scalar apply damaged bookkeeping or formatting'
    $backups=@(Get-ChildItem -LiteralPath $taskConfig -Filter '*.bak' -File)
    Assert-Value ($backups.Count -eq 1) 'Expected one backup'
    Assert-Value ([IO.File]::ReadAllText($backups[0].FullName,$taskEncoding) -ceq $current) 'Backup does not preserve the prior INI'
    Assert-Rejected 'repeat apply'
    [IO.File]::WriteAllText($taskIni,$source+"`r`nServerName=Duplicate",$taskEncoding)
    Assert-Rejected 'duplicate scalar'
    Write-Output 'Offline server configuration apply verified: stop gate, supported-key restriction, stale baselines, duplicate rejection, bookkeeping/BOM/line-ending preservation and backup integrity.'
}finally{
    foreach($taskFile in Get-ChildItem -LiteralPath $taskConfig -File){if(-not $taskFile.FullName.StartsWith($taskDirectory+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Fixture cleanup escaped private directory'};Remove-Item -LiteralPath $taskFile.FullName -Force}
    Remove-Item -LiteralPath $taskConfig
    Remove-Item -LiteralPath $taskDirectory
    Remove-Variable RsmkOfflineFixtureRunning -Scope Global -ErrorAction SilentlyContinue
}
