param(
    [Parameter(Mandatory=$true)][string]$CoreDll,
    [Parameter(Mandatory=$true)][string]$PackedDll,
    [Parameter(Mandatory=$true)][string]$UpxPath
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$root=Join-Path ([IO.Path]::GetTempPath()) ('RuneSchema-Wine-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root|Out-Null
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Add-Text($Archive,$Name,$Text){$entry=$Archive.CreateEntry($Name);$writer=[IO.StreamWriter]::new($entry.Open());try{$writer.Write($Text)}finally{$writer.Dispose()}}
function Bytes($Entry){$read=$Entry.Open();$memory=[IO.MemoryStream]::new();try{$read.CopyTo($memory);return ,$memory.ToArray()}finally{$memory.Dispose();$read.Dispose()}}
try{
    $source=Join-Path $root 'source.zip';$output=Join-Path $root 'wine.zip'
    $zip=[IO.Compression.ZipFile]::Open($source,[IO.Compression.ZipArchiveMode]::Create)
    try{
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$CoreDll,'Binaries/Win64/ue4ss/mods/runeschema/DLLS/MAIN.DLL')|Out-Null
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$PackedDll,'Binaries/Win64/ue4ss/mods/runeschema/plugins/Probe/dll/Probe.dll')|Out-Null
        Add-Text $zip 'Binaries/Win64/ue4ss/mods/runeschema/settings/licenses/RuneSchema-Licenses.txt' 'Synthetic package fixture; not distributable'
        foreach($extension in @('pak','utoc','ucas')){Add-Text $zip ('Binaries/Win64/ue4ss/mods/runeschema/plugins/Probe/paks/Probe.'+$extension) ('Original-content-'+$extension)}
        Add-Text $zip 'Content/Paks/~mods/runeschema.txt' 'Keep the original content entry'
    }finally{$zip.Dispose()}
    $sourceHash=(Get-FileHash -LiteralPath $source).Hash
    & (Join-Path $repo 'build/package-wine-compat.ps1') -SourceArchive $source -DestinationArchive $output -UpxPath $UpxPath
    if((Get-FileHash -LiteralPath $source).Hash -ne $sourceHash){throw 'Source ZIP changed'}
    $original=[IO.Compression.ZipFile]::OpenRead($source);$result=[IO.Compression.ZipFile]::OpenRead($output)
    try{
        $core=$result.GetEntry('Binaries/Win64/ue4ss/Mods/RuneSchema/dlls/main.dll')
        if(-not $core -or $core.Length -ne (Get-Item -LiteralPath $CoreDll).Length){throw 'Canonical core path or bytes differ'}
        $metaEntry=$result.GetEntry('Binaries/Win64/ue4ss/Mods/RuneSchema/settings/WINE-COMPATIBILITY.json')
        $metadata=[Text.Encoding]::UTF8.GetString((Bytes $metaEntry))|ConvertFrom-Json
        if(@($metadata.NativeDLLs).Count -ne 2 -or @($metadata.NativeDLLs|Where-Object {$_.WasPacked}).Count -ne 1){throw 'Native DLL accounting differs'}
        foreach($old in $original.Entries|Where-Object {$_.Name -notlike '*.dll'}){
            $name=$old.FullName.Replace('/ue4ss/mods/runeschema/','/ue4ss/Mods/RuneSchema/')
            $fresh=$result.GetEntry($name)
            if(-not $fresh -or [Convert]::ToBase64String((Bytes $old)) -ne [Convert]::ToBase64String((Bytes $fresh))){throw 'Non-DLL entry was lost or changed'}
        }
        if(-not $result.GetEntry('Binaries/Win64/ue4ss/Mods/RuneSchema/tools/proton/check-ue4ss-abi.py')){throw 'API checker missing'}
        foreach($row in $metadata.NativeDLLs){
            $entry=$result.GetEntry($row.File);$hash=[Security.Cryptography.SHA256]::Create()
            try{$actual=[BitConverter]::ToString($hash.ComputeHash((Bytes $entry))).Replace('-','')}finally{$hash.Dispose()}
            if($actual -ne $row.SHA256){throw 'Packaged DLL differs from its receipt'}
        }
    }finally{$original.Dispose();$result.Dispose()}
    $unsafe=Join-Path $root 'unsafe.zip';$zip=[IO.Compression.ZipFile]::Open($unsafe,[IO.Compression.ZipArchiveMode]::Create)
    try{
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$CoreDll,'RuneSchema/dlls/main.dll')|Out-Null
        Add-Text $zip 'RuneSchema/settings/licenses/RuneSchema-Licenses.txt' 'Fixture'
        Add-Text $zip '../escape.txt' 'Do not publish this entry'
    }finally{$zip.Dispose()}
    $rejected=$false;try{& (Join-Path $repo 'build/package-wine-compat.ps1') -SourceArchive $unsafe -DestinationArchive (Join-Path $root 'unsafe-output.zip') -UpxPath $UpxPath}catch{$rejected=$true}
    if(-not $rejected -or (Test-Path -LiteralPath (Join-Path $root 'unsafe-output.zip'))){throw 'Unsafe archive was published'}
    Write-Output 'Wine package: source preserved, all content retained, DLL hashes verified, UPX removed, paths canonical, unsafe input rejected.'
}finally{
    $path=[IO.Path]::GetFullPath($root);$parent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([char[]]@('\','/'))+[IO.Path]::DirectorySeparatorChar
    if(-not $path.StartsWith($parent,[StringComparison]::OrdinalIgnoreCase) -or (Get-Item -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Unsafe test cleanup boundary'}
    Remove-Item -LiteralPath $path -Recurse -Force
}
