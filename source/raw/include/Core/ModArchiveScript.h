#pragma once
#include <string_view>
namespace PS::ModManager {
// Fixed, first-party extractor. Paths are data in input.json, never interpolated
// into a shell command. No archive entry is executed. New folders only.
inline constexpr std::string_view ArchiveScript=R"RSPS(
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskInput=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'input.json') -Raw | ConvertFrom-Json
$taskResult=Join-Path $PSScriptRoot 'result.json'
try {
  $archivePath=[IO.Path]::GetFullPath($taskInput.Archive)
  if([IO.Path]::GetExtension($archivePath) -ine '.zip'){throw 'Choose a ZIP mod archive.'}
  if((Get-Item -LiteralPath $archivePath).Length -gt 2147483648){throw 'Archive exceeds the 2 GiB import limit.'}
  $zip=[IO.Compression.ZipFile]::OpenRead($archivePath)
  try {
    if($zip.Entries.Count -gt 16000){throw 'Archive has too many entries.'}
    $files=[Collections.Generic.List[object]]::new()
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    [long]$total=0
    foreach($entry in $zip.Entries){
      $name=$entry.FullName.Replace('\','/')
      if($name.Length -gt 220 -or $name.StartsWith('/') -or $name.Contains(':') -or $name.Contains([char]0)){throw 'Archive has an unsafe path.'}
      $parts=$name.TrimEnd('/').Split('/')
      foreach($part in $parts){
        if(!$part -or $part -eq '.' -or $part -eq '..' -or $part.EndsWith('.') -or $part.EndsWith(' ') -or $part -match '[\x00-\x1f<>"|?*]' -or $part -match '^(?i:con|prn|aux|nul|com[1-9]|lpt[1-9])(\.|$)'){throw 'Archive has an unsafe Windows path.'}
      }
      $mode=($entry.ExternalAttributes -shr 16) -band 0xf000
      if($mode -eq 0xa000 -or ($entry.ExternalAttributes -band 0x400)){throw 'Archive links/reparse entries are not supported.'}
      if(!$seen.Add($name.TrimEnd('/'))){throw 'Archive has duplicate or case-colliding paths.'}
      if($name.EndsWith('/')){continue}
      if($entry.Length -gt 1073741824){throw 'An archive entry exceeds 1 GiB.'}
      $total+=$entry.Length
      if($total -gt 4294967296 -or ($entry.Length -gt 1048576 -and $entry.Length -gt [Math]::Max(1,$entry.CompressedLength)*400)){throw 'Archive exceeds expansion limits.'}
      if([IO.Path]::GetExtension($name) -match '^(?i:\.exe|\.com|\.bat|\.cmd|\.ps1|\.msi|\.lnk|\.url)$'){throw 'Installer programs and shortcuts are not mod payloads.'}
      $files.Add([pscustomobject]@{Name=$name;Parts=$parts;Entry=$entry})
    }
    if(!$files.Count){throw 'Archive contains no files.'}
    $prefixes=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($file in $files){
      for($i=0;$i+3 -lt $file.Parts.Length;$i++){
        if($file.Parts[$i] -ieq 'RuneSchema' -and $file.Parts[$i+1] -ieq 'mods'){
          [void]$prefixes.Add(($file.Parts[0..($i+2)] -join '/')+'/')
        }
      }
    }
    $owner='';$prefix=''
    if($prefixes.Count -eq 1){$prefix=@($prefixes)[0];$owner=$prefix.TrimEnd('/').Split('/')[-1]}
    elseif($prefixes.Count -gt 1){throw 'Archive contains multiple mods; import one owner per ZIP.'}
    else {
      $manifests=@($files | Where-Object { [IO.Path]::GetFileName($_.Name) -ieq 'mod.json' })
      if($manifests.Count -ne 1){throw 'Use one mod.json manifest, or a RuneSchema/mods/ModName package.'}
      $manifest=$manifests[0]
      if($manifest.Entry.Length -gt 131072){throw 'mod.json is too large.'}
      $reader=[IO.StreamReader]::new($manifest.Entry.Open())
      try {$text=$reader.ReadToEnd()} finally {$reader.Dispose()}
      if($text.Length -gt 131072){throw 'mod.json is too large.'}
      $meta=$text|ConvertFrom-Json
      $owner=[string]$meta.id
      $prefix=$manifest.Name.Substring(0,$manifest.Name.Length-'mod.json'.Length)
    }
    if($owner -notmatch '^[A-Za-z0-9][A-Za-z0-9_ -]{0,95}$' -or $owner.EndsWith(' ') -or $owner -match '^(?i:con|prn|aux|nul|com[1-9]|lpt[1-9])$'){throw 'Invalid mod folder id.'}
    $stage=Join-Path $PSScriptRoot $owner
    if(Test-Path -LiteralPath $stage){throw 'Import staging directory already exists.'}
    [IO.Directory]::CreateDirectory($stage)|Out-Null
    $stagePrefix=[IO.Path]::GetFullPath($stage)+[IO.Path]::DirectorySeparatorChar
    foreach($file in $files){
      if($file.Name -ieq 'Content/Paks/~mods/runeschema.txt'){if($file.Entry.Length -gt 4096){throw 'RuneSchema deployment marker is too large.'};continue}
      if(!$file.Name.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){
        if($file.Name -match '^(?i)Binaries/(Win64|WinGDK)/ue4ss/Mods/'){throw 'This package also contains a native UE4SS mod. Install the complete Binaries/Content package through your mod manager or manually; Helpy will not discard its native companion.'}
        throw 'Archive contains files outside its single mod folder.'
      }
      $relative=$file.Name.Substring($prefix.Length)
      if(!$relative){throw 'Empty mod payload path.'}
      $target=[IO.Path]::GetFullPath((Join-Path $stage $relative))
      if(!$target.StartsWith($stagePrefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Archive path escaped the mod directory.'}
      [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))|Out-Null
      $source=$file.Entry.Open();$output=[IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
      try {
        $buffer=New-Object byte[] 65536;[long]$written=0
        while(($n=$source.Read($buffer,0,$buffer.Length)) -gt 0){$written+=$n;if($written -gt $file.Entry.Length){throw 'Entry expanded beyond its declared length.'};$output.Write($buffer,0,$n)}
        if($written -ne $file.Entry.Length){throw 'Archive entry is incomplete.'}
      }finally{$output.Dispose();$source.Dispose()}
    }
    @{Ok=$true;Owner=$owner;Files=$files.Count;Bytes=$total}|ConvertTo-Json -Compress|Set-Content -LiteralPath $taskResult -Encoding UTF8
  }finally{$zip.Dispose()}
}catch{
  @{Ok=$false;Status=$_.Exception.Message}|ConvertTo-Json -Compress|Set-Content -LiteralPath $taskResult -Encoding UTF8
  exit 1
}
)RSPS";
}
