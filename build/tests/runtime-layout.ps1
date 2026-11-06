param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $RepositoryRoot 'build/runtime-layout.ps1')
$test=Join-Path ([IO.Path]::GetTempPath()) ('RS-layout-'+[Guid]::NewGuid().ToString('N'))
function Write-Layout([string]$path,[string]$text){New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force|Out-Null;[IO.File]::WriteAllText($path,$text)}
function Check-Layout([bool]$value,[string]$reason){if(!$value){throw $reason}}
try{
 $root=Join-Path $test 'RuneSchema'
 $files=@{
  'dlls/main.dll'='core';'plugins/RuneSchema.Helpy/dll/RuneSchema.Helpy.dll'='bridge';'plugins/RuneSchema.Helpy/dll/Helpy.exe'='app';'plugins/RuneSchema.Helpy/dll/SDL3.dll'='controller';
  'plugins/RuneSchema.ModMenu2/plugin.json'='{"Id":"RuneSchema.ModMenu2","Version":"0.8.1.0","Host":"helpy","EntryPoint":"RuneSchema.ModMenu2.dll"}';'plugins/RuneSchema.ModMenu2/dll/RuneSchema.ModMenu2.dll'='menu';'plugins/RuneSchema.ModMenu2/ModMenu-MIT.txt'='notice';
  'plugins/RuneSchema.DeveloperTools/plugin.json'='{"Id":"RuneSchema.DeveloperTools","Version":"0.8.1.0","Host":"helpy","EntryPoint":"RuneSchema.DeveloperTools.dll"}';'plugins/RuneSchema.DeveloperTools/dll/RuneSchema.DeveloperTools.dll'='developer';
  'plugins/RuneSchema.Helpy/modmenu2.key'='key';'plugins/RuneSchema.Helpy/settings/ui.jsonc'='user-theme';'plugins/RuneSchema.HUD/plugin.json'='{"Id":"RuneSchema.HUD","Version":"0.8.1.0"}';'plugins/RuneSchema.HUD/dll/RuneSchema.HUD.dll'='hud';
  'plugins/RuneSchema.Helpy/plugin.json'='{"Id":"RuneSchema.Helpy","Version":"0.8.1.0","EntryPoint":"RuneSchema.Helpy.dll","NativeHudPlugin":"RuneSchema.HUD"}';'plugins/plugins.txt'='RSNetworking:0';'settings/settings.ini'='user-settings';'mods/Keep/ID.txt'='user-mod'
 }
 foreach($id in @('RSNetworking','RuneSchema.RegistryBridge','RuneSchema.QuestItems')){$files['plugins/'+$id+'/plugin.json']='{"Id":"'+$id+'","Version":"0.8.1.0"}'}
 foreach($pair in $files.GetEnumerator()){Write-Layout (Join-Path $root $pair.Key) $pair.Value}
 Set-RuneSchemaRuntimeLayout $root
 Set-RuneSchemaRuntimeLayout $root
 foreach($pair in $files.GetEnumerator()){
  if($pair.Key -eq 'plugins/RuneSchema.Helpy/plugin.json'){continue}
  $relative=$pair.Key.Replace('plugins/RuneSchema.Helpy/dll/RuneSchema.Helpy.dll','Helpy/Helpy.dll').Replace('plugins/RuneSchema.Helpy/dll/','Helpy/').Replace('plugins/RuneSchema.Helpy/','Helpy/').Replace('plugins/RuneSchema.HUD/','Helpy/plugins/RuneSchema.HUD/').Replace('plugins/RuneSchema.ModMenu2/','Helpy/plugins/RuneSchema.ModMenu2/').Replace('plugins/RuneSchema.DeveloperTools/','Helpy/plugins/RuneSchema.DeveloperTools/')
  if($relative -eq 'plugins/plugins.txt'){$relative='settings/plugins.txt'}elseif($relative.StartsWith('plugins/')){$relative='dlls/'+$relative}
  Check-Layout ([IO.File]::ReadAllText((Join-Path $root $relative)) -eq $pair.Value) ('Changed staged bytes: '+$relative)
 }
 Check-Layout (-not(Test-Path -LiteralPath (Join-Path $root 'plugins'))) 'Legacy root remains'
 Check-Layout ((Get-Content -LiteralPath (Join-Path $root 'Helpy/plugin.json') -Raw|ConvertFrom-Json).EntryPoint -eq 'Helpy.dll') 'Flat bridge entry differs'
 . (Join-Path $RepositoryRoot 'resources/tools/repair-runeschema.ps1')
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 $zip=Join-Path $test 'new.zip';[IO.Compression.ZipFile]::CreateFromDirectory($root,$zip)
 $script:Job=Join-Path $test 'repair-job';New-Item -ItemType Directory -Path $script:Job|Out-Null
 $entries=Get-RepairEntries $zip 'Win64' (Join-Path $script:Job 'stage')
 foreach($required in @('dlls/main.dll','Helpy/Helpy.dll','Helpy/Helpy.exe','Helpy/plugins/RuneSchema.HUD/dll/RuneSchema.HUD.dll','Helpy/plugins/RuneSchema.ModMenu2/dll/RuneSchema.ModMenu2.dll','Helpy/plugins/RuneSchema.ModMenu2/ModMenu-MIT.txt','Helpy/plugins/RuneSchema.DeveloperTools/dll/RuneSchema.DeveloperTools.dll','dlls/plugins/RSNetworking/plugin.json')){Check-Layout ($required -in $entries.Relative) ('Repair omitted '+$required)}
 Check-Layout (-not @($entries|Where-Object {$_.Relative -match '(^|/)(settings|mods)/'}).Count) 'Repair included user data'
 'Runtime staging/repair layout checks passed.'
}finally{
 $resolved=[IO.Path]::GetFullPath($test);$temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/')
 if([IO.Path]::GetDirectoryName($resolved) -eq $temp -and [IO.Path]::GetFileName($resolved) -match '^RS-layout-[a-f0-9]{32}$'){if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}}
}
