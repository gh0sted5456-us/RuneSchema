param([string]$Script)
$ErrorActionPreference='Stop'
. $Script
$test=Join-Path ([IO.Path]::GetTempPath()) ('RS-hotfix-test-'+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($test)|Out-Null
function Check($value,[string]$message){if(!$value){throw $message}}
function Write-Fixture([string]$path,[string]$text){[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))|Out-Null;[IO.File]::WriteAllText($path,$text)}
function Refused([scriptblock]$body,[string]$message){$blocked=$false;try{& $body | Out-Null}catch{$blocked=$true};Check $blocked $message}
function Get-CimInstance {return $script:Processes}
function Get-RepairRelease([string]$version){return [pscustomobject]@{id=1;tag_name=$version;draft=$false;assets=@($script:Asset)}}
function Receive-RepairPackage($asset,[string]$path){$script:Downloads++;[IO.File]::Copy($script:Zip,$path)}
function New-Job {
    $script:Job=Join-Path $test ([Guid]::NewGuid().ToString('N'));[IO.Directory]::CreateDirectory($script:Job)|Out-Null
    $script:Console=Join-Path $script:Job 'console.log';$script:ProgressFile=Join-Path $script:Job 'progress.json';$script:ControlFile=''
}
try {
    $script:Processes=@();$script:Downloads=0
    $root=Join-Path $test 'game/Binaries/Win64/ue4ss/Mods/RuneSchema'
    Write-Fixture (Join-Path $root 'dlls/main.dll') 'original-core'
    Write-Fixture (Join-Path $root 'scripts/main.lua') 'original-loader'
    Write-Fixture (Join-Path $root 'settings/settings.ini') 'user-preferences'
    Write-Fixture (Join-Path $root 'mods/Example/ID.txt') 'user-mod'
    $payload=Join-Path $test 'payload/RuneSchema'
    Write-Fixture (Join-Path $payload 'scripts/main.lua') 'updated-loader'
    $manifest=@{Format=1;Version='0.8.1.0hf';FromVersions=@('0.8.1.0','0.8.1.0hf');Platform='Win64';Files=@(@{Path='scripts/main.lua';SHA256=(Get-RepairHash (Join-Path $payload 'scripts/main.lua'));BeforeSHA256=(Get-RepairHash (Join-Path $root 'scripts/main.lua'))})}
    Write-Fixture (Join-Path $payload 'hotfix.json') ($manifest | ConvertTo-Json -Depth 8)
    $script:Zip=Join-Path $test 'hotfix.zip';[IO.Compression.ZipFile]::CreateFromDirectory((Split-Path $payload),$script:Zip)
    $script:Asset=[pscustomobject]@{id=2;state='uploaded';name='RuneSchema-0.8.1.0hf-Win64-hotfix.zip';size=(Get-Item $script:Zip).Length;digest=('sha256:'+(Get-RepairHash $script:Zip).ToLowerInvariant());browser_download_url='https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.8.1.0hf/RuneSchema-0.8.1.0hf-Win64-hotfix.zip'}
    $data=[pscustomobject]@{Root=$root;Version='0.8.1.0';Hotfix=[pscustomobject]@{id=1;tag_name='0.8.1.0hf';assets=@($script:Asset)};HotfixIdentity=('hotfix:1:2:'+$script:Asset.digest);HotfixReceipt=(Join-Path $test 'appdata/hotfix-applied.json')}
    # No game, DLL loader, network connection, or user installation is exercised.
    $script:Processes=@([pscustomobject]@{ExecutablePath=(Join-Path (Split-Path (Split-Path (Split-Path $root))) 'RSDragonwilds.exe')})
    New-Job;Refused {Invoke-RuneSchemaRepair $data} 'Hotfix downloaded while its game was running';Check ($script:Downloads -eq 0) 'Game-closed guard ran after downloading'
    $script:Processes=@();New-Job;$entries=@(Get-RepairEntries $script:Zip Win64 (Join-Path $script:Job 'stage') -Partial)
    Check ($entries.Count -eq 1) 'Partial hotfix required unrelated core/plugin files'
    Read-HotfixManifest $script:Zip '0.8.1.0hf' '0.8.1.0' Win64 $entries
    Refused {Read-HotfixManifest $script:Zip '0.8.1.0hf' '0.8.0.3' Win64 $entries} 'Wrong installed version accepted'
    Refused {Read-HotfixManifest $script:Zip '0.8.1.0hf' '0.8.1.0' WinGDK $entries} 'Wrong platform accepted'
    $entries[0].Hash='0'*64;Refused {Read-HotfixManifest $script:Zip '0.8.1.0hf' '0.8.1.0' Win64 $entries} 'Incorrect payload checksum accepted'
    # The real exit handshake is tested with a tiny console owner, not Helpy UI.
    $ownerExe=Join-Path $root 'plugins/RuneSchema.Helpy/dll/Helpy.exe';[IO.Directory]::CreateDirectory((Split-Path $ownerExe))|Out-Null
    Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Threading;
public static class HotfixOwnerFixture {
 public static void Main(string[] args) {
  var until=DateTime.UtcNow.AddSeconds(15);
  while(DateTime.UtcNow<until) {
   try {if(File.Exists(args[1])&&File.ReadAllText(args[1]).Contains("AwaitingExit")) {
    var pending=args[0]+".pending";File.WriteAllText(pending,"{\"Action\":\"apply-hotfix\",\"OwnerPid\":"+System.Diagnostics.Process.GetCurrentProcess().Id+"}");
    File.Replace(pending,args[0],null);Thread.Sleep(100);return;
   }} catch(IOException) {} Thread.Sleep(20);
  }
 }
}
'@ -OutputAssembly $ownerExe -OutputType ConsoleApplication
    New-Job;$script:ControlFile=Join-Path $script:Job 'control.json'
    $owner=Start-Process -FilePath $ownerExe -ArgumentList ('"'+$script:ControlFile+'" "'+$script:ProgressFile+'"') -PassThru -WindowStyle Hidden
    Write-Fixture $script:ControlFile ('{"Action":"run","OwnerPid":'+$owner.Id+'}')
    Wait-HotfixApply $root;Check ($script:HotfixHandedOff -and $owner.HasExited) 'Owner exit handshake did not complete'
    $script:RestartRoot=$null;$script:ControlFile=''
    function Wait-HotfixApply([string]$root){$script:ApplyWaits++}
    $script:ApplyWaits=0;New-Job;Invoke-RuneSchemaRepair $data
    Check ((Get-Content -LiteralPath (Join-Path $root 'scripts/main.lua') -Raw) -eq 'updated-loader') 'Declared file was not replaced'
    Check ((Get-Content -LiteralPath (Join-Path $root 'dlls/main.dll') -Raw) -eq 'original-core') 'Unlisted core changed'
    Check ((Get-Content -LiteralPath (Join-Path $root 'settings/settings.ini') -Raw) -eq 'user-preferences') 'User settings changed'
    Check ((Get-Content -LiteralPath (Join-Path $root 'mods/Example/ID.txt') -Raw) -eq 'user-mod') 'User mod changed'
    Check ((Get-Content -LiteralPath $data.HotfixReceipt -Raw | ConvertFrom-Json).Identity -eq $data.HotfixIdentity) 'Applied receipt missing'
    $data.Version='0.8.1.0hf';New-Job;Invoke-RuneSchemaRepair $data;Check ($script:ApplyWaits -eq 1) 'Already applied hotfix requested another exit';$data.Version='0.8.1.0'
    Write-Fixture (Join-Path $root 'scripts/main.lua') 'different-private-build'
    New-Job;Refused {Invoke-RuneSchemaRepair $data} 'Unexpected installed file was overwritten'
    Check ((Get-Content -LiteralPath (Join-Path $root 'scripts/main.lua') -Raw) -eq 'different-private-build') 'Mismatched original was changed'
    $originalDigest=$script:Asset.digest;$script:Asset.digest='sha256:'+('0'*64);$data.HotfixIdentity='hotfix:1:2:'+$script:Asset.digest
    New-Job;Refused {Invoke-RuneSchemaRepair $data} 'Bad archive digest accepted';$script:Asset.digest=$originalDigest
    $protected=Join-Path $test 'protected/RuneSchema'
    Write-Fixture (Join-Path $protected 'settings/settings.ini') 'replace-user-settings'
    $blockedManifest=$manifest.Clone();$blockedManifest.Files=@(@{Path='settings/settings.ini';SHA256=(Get-RepairHash (Join-Path $protected 'settings/settings.ini'));BeforeSHA256=''})
    Write-Fixture (Join-Path $protected 'hotfix.json') ($blockedManifest | ConvertTo-Json -Depth 8)
    $protectedZip=Join-Path $test 'protected.zip';[IO.Compression.ZipFile]::CreateFromDirectory((Split-Path $protected),$protectedZip)
    New-Job;$protectedEntries=@(Get-RepairEntries $protectedZip Win64 (Join-Path $script:Job 'stage') -Partial)
    Refused {Read-HotfixManifest $protectedZip '0.8.1.0hf' '0.8.1.0' Win64 $protectedEntries} 'Protected user settings were eligible for hotfixing'
    # Exercise the shared replacement/rollback transaction through the hotfix branch.
    Write-Fixture (Join-Path $root 'scripts/main.lua') 'original-loader'
    Write-Fixture (Join-Path $payload 'dlls/main.dll') 'updated-core'
    $manifest.Files+=@{Path='dlls/main.dll';SHA256=(Get-RepairHash (Join-Path $payload 'dlls/main.dll'));BeforeSHA256=(Get-RepairHash (Join-Path $root 'dlls/main.dll'))}
    Write-Fixture (Join-Path $payload 'hotfix.json') ($manifest | ConvertTo-Json -Depth 8)
    $script:Zip=Join-Path $test 'rollback.zip';[IO.Compression.ZipFile]::CreateFromDirectory((Split-Path $payload),$script:Zip)
    $script:Asset.size=(Get-Item $script:Zip).Length;$script:Asset.digest='sha256:'+(Get-RepairHash $script:Zip).ToLowerInvariant();$data.HotfixIdentity='hotfix:1:2:'+$script:Asset.digest
    [IO.File]::Delete($data.HotfixReceipt)
    $script:RealInstall=${function:Install-RepairFile};$script:FailReplacement=$true
    function Install-RepairFile([string]$stage,[string]$target){
        if($script:FailReplacement -and $target.EndsWith('main.lua')){$script:FailReplacement=$false;throw 'Injected hotfix replacement failure'}
        & $script:RealInstall $stage $target
    }
    New-Job;Refused {Invoke-RuneSchemaRepair $data} 'Replacement failure was ignored'
    Check ((Get-Content -LiteralPath (Join-Path $root 'dlls/main.dll') -Raw) -eq 'original-core') 'Hotfix rollback lost the original core'
    Check ((Get-Content -LiteralPath (Join-Path $root 'scripts/main.lua') -Raw) -eq 'original-loader') 'Hotfix rollback changed an unapplied loader'
    Check (!(Test-Path -LiteralPath $data.HotfixReceipt)) 'Failed hotfix wrote an applied receipt'
    'Hotfix checks passed: game-closed download guard, platform/version/hash validation, real owner-exit handoff, selective replacement, applied receipt, idempotence, protected settings, private-build preservation, and rollback.'
} finally {
    if($owner -and !$owner.HasExited){$owner.WaitForExit(16000)|Out-Null}
    $resolved=[IO.Path]::GetFullPath($test);$parent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
    if([IO.Path]::GetDirectoryName($resolved) -eq $parent -and [IO.Path]::GetFileName($resolved) -match '^RS-hotfix-test-[0-9a-f]{32}$'){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
