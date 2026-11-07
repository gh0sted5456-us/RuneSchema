param([Parameter(Mandatory=$true)][string]$Binary,[Parameter(Mandatory=$true)][string]$Root)
$ErrorActionPreference='Stop'
$base=[IO.Path]::GetFullPath($Root)
if($base -notmatch '^[DF]:[\\/]'){throw 'Redirect fixtures must stay on D: or F:'}
$fixture=Join-Path $base ([guid]::NewGuid().ToString('N'))
$links=[Collections.Generic.List[string]]::new()
function Directory([string]$path){New-Item -ItemType Directory -Path $path -Force|Out-Null}
function Hash([string]$path){$sha=[Security.Cryptography.SHA256]::Create();$stream=[IO.File]::OpenRead($path);try{return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')}finally{$stream.Dispose();$sha.Dispose()}}
function Junction([string]$path,[string]$target){
 Directory (Split-Path -Parent $path)
 New-Item -ItemType Junction -Path $path -Target $target|Out-Null
 if(!((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Junction fixture was not created'}
 $links.Add($path)
}
function RunPaths([string]$local){
 $info=New-Object Diagnostics.ProcessStartInfo
 $info.FileName=[IO.Path]::GetFullPath($Binary);$info.Arguments='--paths';$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.WindowStyle='Hidden'
 $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 $info.StandardOutputEncoding=[Text.Encoding]::UTF8;$info.StandardErrorEncoding=[Text.Encoding]::UTF8
 $info.EnvironmentVariables['LOCALAPPDATA']=$local;$info.EnvironmentVariables['TEMP']=(Join-Path $fixture 'temp');$info.EnvironmentVariables['TMP']=(Join-Path $fixture 'temp')
 $process=[Diagnostics.Process]::Start($info)
 try{
  $out=$process.StandardOutput.ReadToEndAsync();$err=$process.StandardError.ReadToEndAsync()
  if(!$process.WaitForExit(10000)){throw 'Path fixture exceeded 10 seconds'}
  return [pscustomobject]@{Code=$process.ExitCode;Output=$out.Result;Error=$err.Result}
 }finally{$process.Dispose()}
}
function CheckPaths([string]$local,[string]$expected){
 $result=RunPaths $local
 if($result.Code -ne 0){throw $result.Error}
 $actual=@($result.Output -split '\r?\n'|Where-Object {$_})
 $prefix=$expected.Replace('\','/').TrimEnd('/')
 $wanted=@($prefix)+@('steam','gamepass','epic','server'|ForEach-Object {$prefix+'/'+$_+'/cache/mappings'})
 if($actual.Count -ne $wanted.Count){throw 'Wrong number of data paths'}
 for($i=0;$i -lt $wanted.Count;$i++){if(![string]::Equals($actual[$i],$wanted[$i],[StringComparison]::OrdinalIgnoreCase)){throw ('Unexpected physical data path: '+$actual[$i]+' expected '+$wanted[$i])}}
}
try{
 Directory (Join-Path $fixture 'temp')
 $physical1=Join-Path $fixture 'physical-local';Directory $physical1
 $logical1=Join-Path $fixture 'alias-local';Junction $logical1 $physical1
 CheckPaths $logical1 (Join-Path $physical1 'RSDragonwilds/RuneSchema')
 $local2=Join-Path $fixture 'local2';$physical2=Join-Path $fixture 'physical-game';Directory $physical2
 Junction (Join-Path $local2 'RSDragonwilds') $physical2
 CheckPaths $local2 (Join-Path $physical2 'RuneSchema')
 $local3=Join-Path $fixture 'local3';$physical3=Join-Path $fixture 'physical-schema-Ω';Directory $physical3
 Junction (Join-Path $local3 'RSDragonwilds/RuneSchema') $physical3
 $marker=Join-Path $physical3 'profile-marker.json';[IO.File]::WriteAllText($marker,'{"keep":"profile"}',[Text.Encoding]::UTF8)
 $hash=(Hash $marker)
 CheckPaths $local3 $physical3
 if((Hash $marker) -ne $hash){throw 'Existing profile bytes changed'}
 $outside=Join-Path $fixture 'outside';Directory $outside
 $child=Join-Path $physical3 'steam/cache/mappings';Junction $child $outside
 $blocked=RunPaths $local3
 if($blocked.Code -eq 0 -or !$blocked.Error.Contains($child.Replace('\','/'))){throw 'Redirected cache child was not rejected with its exact path'}
 $invalid=RunPaths 'relative-local'
 if($invalid.Code -eq 0 -or !$invalid.Error.Contains('LOCALAPPDATA')){throw 'Relative LocalAppData was not rejected'}
 'PASS: redirected LocalAppData/game/data roots, Unicode, preserved profile, rejected child link and relative root'
}finally{
 for($i=$links.Count-1;$i -ge 0;$i--){$link=$links[$i];if(Test-Path -LiteralPath $link){if(!((Get-Item -LiteralPath $link -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Fixture link changed; cleanup stopped'};[IO.Directory]::Delete($link)}}
 if(Test-Path -LiteralPath $fixture){
  $resolved=[IO.Path]::GetFullPath($fixture)
  if(!$resolved.StartsWith($base.TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup boundary mismatch'}
  $queue=[Collections.Generic.Queue[string]]::new();$queue.Enqueue($resolved)
  while($queue.Count){$current=$queue.Dequeue();foreach($item in Get-ChildItem -LiteralPath $current -Force){if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Unexpected fixture link; cleanup stopped'};if($item.PSIsContainer){$queue.Enqueue($item.FullName)}}}
  Remove-Item -LiteralPath $resolved -Recurse -Force
 }
}
