[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RuneSchemaRoot,
    [Parameter(Mandatory=$true)][string]$OutputPath,
    [ValidateSet('Steam','GamePass')][string]$Platform='Steam'
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression
$root=[IO.Path]::GetFullPath((Resolve-Path -LiteralPath $RuneSchemaRoot).Path).TrimEnd('\','/')
$output=[IO.Path]::GetFullPath($OutputPath)
if($output.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Output must be outside the framework input'}
if(Test-Path -LiteralPath $output){throw 'Output archive already exists'}
if(!(Test-Path -LiteralPath (Join-Path $root 'dlls/main.dll') -PathType Leaf) -or !(Test-Path -LiteralPath (Join-Path $root 'plugins') -PathType Container)){throw 'Use a clean full-framework release payload'}
$files=New-Object 'System.Collections.Generic.List[object]'
$queue=New-Object 'System.Collections.Generic.Stack[string]';$queue.Push($root)
$names=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$triplets=@{}
$canonical=Test-Path -LiteralPath (Join-Path $root 'plugins/RuneSchema.RegistryBridge/paks/RegistryBridge') -PathType Container
while($queue.Count){
    $directory=$queue.Pop()
    if((Get-Item -LiteralPath $directory -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse directories are not release inputs'}
    foreach($item in Get-ChildItem -LiteralPath $directory -Force){
        if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Reparse entry is not a release input: $($item.Name)"}
        $relative=$item.FullName.Substring($root.Length+1).Replace('\','/')
        if($canonical -and ($relative -ieq 'plugins/RSNetworking/paks/RegistryBridge' -or $relative.StartsWith('plugins/RSNetworking/paks/RegistryBridge/',[StringComparison]::OrdinalIgnoreCase))){continue}
        if($item.PSIsContainer){$queue.Push($item.FullName);continue}
        if($relative -match '^(?i:cache|profiles|backups)/' -or $item.Extension -in @('.log','.dmp','.sav','.jsonl') -or $relative.StartsWith('mods/',[StringComparison]::OrdinalIgnoreCase)){throw "Use a clean release payload; runtime/user file found: $relative"}
        if($item.Name -eq '__folder_managed_by_vortex'){continue}
        if(!$names.Add($relative)){throw "Case-colliding release path: $relative"}
        if($relative -match '^(?i:plugins)/.+/(?i:paks)/.+\.(?i:pak|ucas|utoc)$'){
            $key=$relative.Substring(0,$relative.LastIndexOf('.')).ToLowerInvariant()
            $bit=switch($item.Extension.ToLowerInvariant()){'.pak'{1};'.ucas'{2};'.utoc'{4}}
            $triplets[$key]=([int]$triplets[$key] -bor $bit)
        }
        $files.Add([pscustomobject]@{Path=$item.FullName;Relative=$relative})
    }
}
foreach($key in $triplets.Keys){if($triplets[$key] -ne 7){throw "Incomplete release container: $key"}}
if($canonical -and !($triplets.Keys | Where-Object {$_ -like 'plugins/runeschema.registrybridge/paks/registrybridge/*'})){throw 'Canonical shared registry container is missing'}
$binary=if($Platform -eq 'GamePass'){'WinGDK'}else{'Win64'}
$destination="RSDragonwilds/Binaries/$binary/ue4ss/Mods/RuneSchema"
$instructions=New-Object 'System.Collections.Generic.List[object]'
$instructions.Add(@{type='setmodtype';value='dinput'})
$parent=Split-Path -Parent $output;New-Item -ItemType Directory -Path $parent -Force | Out-Null
$temporary=Join-Path $parent ('.vortex-'+[Guid]::NewGuid().ToString('N')+'.zip')
$utf8=New-Object Text.UTF8Encoding($false)
$stream=$null;$archive=$null
try{
    $stream=[IO.File]::Open($temporary,[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    $archive=New-Object IO.Compression.ZipArchive($stream,[IO.Compression.ZipArchiveMode]::Create,$false)
    foreach($file in $files | Sort-Object Relative){
        $source='RuneSchema/'+$file.Relative
        $entry=$archive.CreateEntry($source,[IO.Compression.CompressionLevel]::Optimal)
        $write=$entry.Open();$read=[IO.File]::OpenRead($file.Path)
        try{$read.CopyTo($write)}finally{$read.Dispose();$write.Dispose()}
        $instructions.Add(@{type='copy';source=$source;destination=$destination+'/'+$file.Relative})
    }
    $texts=@{
        'RuneSchema/mods/.keep'='';
        'RuneSchema-Paks-README.txt'="RuneSchema framework location: $destination`r`n`r`nThis note does not load content. Keep framework plugin .pak/.ucas/.utoc files together inside RuneSchema/plugins. Install RuneSchema add-ons inside RuneSchema/mods.`r`nHelp: https://gh0sted5456-us.github.io/RuneSchema/INSTALLATION/`r`n";
        'INSTALL.txt'="RuneSchema Vortex package for $Platform.`r`nInstall with Vortex 1.14 or newer, then deploy with the game closed. The override uses the Engine Injector/game-root mod type and preserves every relative path. UE4SS must already be installed separately.`r`n`r`nFor manual installation, copy the RuneSchema folder to $destination. Copy RuneSchema-Paks-README.txt to RSDragonwilds/Content/Paks/~mods/RuneSchema-README.txt if desired.`r`n`r`nVortex collections do not apply file override instructions. Verify the collection's mod type and install mapping before deployment; the supplied overrides are not a collection installer.`r`n"
    }
    $instructions.Add(@{type='copy';source='RuneSchema/mods/.keep';destination=$destination+'/mods/.keep'})
    $instructions.Add(@{type='copy';source='RuneSchema-Paks-README.txt';destination='RSDragonwilds/Content/Paks/~mods/RuneSchema-README.txt'})
    $texts['vortex_override_instructions.json']=ConvertTo-Json -InputObject @($instructions.ToArray()) -Depth 4
    foreach($name in $texts.Keys){$entry=$archive.CreateEntry($name);$write=$entry.Open();try{$bytes=$utf8.GetBytes($texts[$name]);$write.Write($bytes,0,$bytes.Length)}finally{$write.Dispose()}}
    $archive.Dispose();$archive=$null;$stream.Dispose();$stream=$null
    Move-Item -LiteralPath $temporary -Destination $output
    [pscustomobject]@{Archive=$output;Platform=$Platform;FrameworkFiles=$files.Count;Destination=$destination}
}finally{
    if($archive){$archive.Dispose()};if($stream){$stream.Dispose()}
    if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary}
}
