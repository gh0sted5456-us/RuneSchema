[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SourceArchive,
    [Parameter(Mandatory=$true)][string]$DestinationArchive,
    [string]$UpxPath,
    [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot)
)
# Packaging only: retain every source entry, remove UPX from native DLLs.
$ErrorActionPreference='Stop'
$SourceArchive=[IO.Path]::GetFullPath($SourceArchive)
$DestinationArchive=[IO.Path]::GetFullPath($DestinationArchive)
if($SourceArchive -eq $DestinationArchive){throw 'Wine output must be separate from the normal package'}
if(Test-Path -LiteralPath $DestinationArchive){throw 'Wine output already exists; preserve or remove it explicitly'}
if(-not $UpxPath){$UpxPath=Join-Path $RepositoryRoot '.cache/dependencies/tools/upx/upx.exe'}
$checker=Join-Path $RepositoryRoot 'build/check-ue4ss-abi.py'
if(-not(Test-Path -LiteralPath $checker -PathType Leaf)){throw 'Read-only UE4SS checker is missing'}

function Canonical-Entry([string]$Name){
    $path=$Name.Replace('\','/')
    if($path.StartsWith('/') -or $path.Contains(':') -or $path.Contains([char]0)){throw 'Archive contains an absolute or invalid path'}
    $parts=$path.Split('/')
    if(@($parts|Where-Object {$_ -eq '.' -or $_ -eq '..'}).Count){throw 'Archive contains a relative traversal'}
    for($i=0;$i -lt $parts.Length-1;$i++){
        if($parts[$i] -ieq 'ue4ss' -and $parts[$i+1] -ieq 'mods'){$parts[$i]='ue4ss';$parts[$i+1]='Mods'}
    }
    for($i=0;$i -lt $parts.Length;$i++){
        if($parts[$i] -ieq 'RuneSchema'){
            $parts[$i]='RuneSchema'
            if($parts.Length -gt $i+1 -and $parts[$i+1] -ieq 'dlls'){
                $parts[$i+1]='dlls';if($parts.Length -eq $i+3 -and $parts[$i+2] -ieq 'main.dll'){$parts[$i+2]='main.dll'}
            }
            break
        }
    }
    return $parts -join '/'
}
function Is-Packed([string]$File){
    if((Get-Item -LiteralPath $File).Length -gt 256MB){throw 'Unpacked DLL exceeds the bounded inspection size'}
    $bytes=[IO.File]::ReadAllBytes($File)
    if($bytes.Length -lt 64 -or $bytes[0] -ne 77 -or $bytes[1] -ne 90){throw 'Native DLL has an invalid DOS header'}
    $pe=[BitConverter]::ToInt32($bytes,60)
    if($pe -lt 0 -or $pe -gt $bytes.Length-24 -or [BitConverter]::ToUInt32($bytes,$pe) -ne 17744){throw 'Native DLL has an invalid PE header'}
    $count=[BitConverter]::ToUInt16($bytes,$pe+6);$table=$pe+24+[BitConverter]::ToUInt16($bytes,$pe+20)
    if($count -lt 1 -or $count -gt 96 -or $table -gt $bytes.Length-$count*40){throw 'Native DLL section table is invalid'}
    for($i=0;$i -lt $count;$i++){if([Text.Encoding]::ASCII.GetString($bytes,$table+$i*40,8).StartsWith('UPX')){return $true}}
    return $false
}
function Copy-EntryBytes($Archive,[string]$Name,[IO.Stream]$Stream){
    $entry=$Archive.CreateEntry($Name,[IO.Compression.CompressionLevel]::Optimal)
    $write=$entry.Open();try{$Stream.CopyTo($write)}finally{$write.Dispose()}
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$scratch=Join-Path ([IO.Path]::GetTempPath()) ('RuneSchema-Wine-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch|Out-Null
$temporary=$DestinationArchive+'.'+[guid]::NewGuid().ToString('N')+'.tmp'
$source=$null;$target=$null
try{
    $source=[IO.Compression.ZipFile]::OpenRead($SourceArchive)
    $core=@($source.Entries|Where-Object {$_.FullName.Replace('\','/') -match '(^|/)RuneSchema/dlls/main\.dll$'})
    if($core.Count -ne 1){throw 'Source package must contain one RuneSchema/dlls/main.dll'}
    $modRoot=Canonical-Entry ($core[0].FullName.Replace('\','/').Substring(0,$core[0].FullName.Replace('\','/').Length-'dlls/main.dll'.Length))
    if(-not $source.GetEntry(($modRoot+'settings/licenses/RuneSchema-Licenses.txt'))){
        $license=@($source.Entries|Where-Object {(Canonical-Entry $_.FullName) -eq ($modRoot+'settings/licenses/RuneSchema-Licenses.txt')})
        if($license.Count -ne 1){throw 'Finalize source package licenses before creating the Wine variant'}
    }
    $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $reserved=@(($modRoot+'settings/WINE-COMPATIBILITY.json'),($modRoot+'tools/proton/check-ue4ss-abi.py'),($modRoot+'tools/proton/README.txt'))
    foreach($entry in $source.Entries){$name=Canonical-Entry $entry.FullName;if(-not $names.Add($name)){throw 'Archive paths collide after case normalization'};if($name -in $reserved){throw 'Source is already a compatibility package'}}
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($DestinationArchive)) -Force|Out-Null
    $target=[IO.Compression.ZipFile]::Open($temporary,[IO.Compression.ZipArchiveMode]::Create)
    $records=@();$index=0
    foreach($entry in $source.Entries){
        $name=Canonical-Entry $entry.FullName
        if($name -match '(^|/)RuneSchema/(dlls|plugins)/.*\.dll$'){
            if($entry.Length -gt 256MB){throw 'Native DLL exceeds the bounded inspection size'}
            $file=Join-Path $scratch ((++$index).ToString()+'.dll')
            $read=$entry.Open();try{$write=[IO.File]::Open($file,'CreateNew','Write','None');try{$read.CopyTo($write)}finally{$write.Dispose()}}finally{$read.Dispose()}
            $before=(Get-FileHash -LiteralPath $file).Hash;$packed=Is-Packed $file
            if($packed){
                if(-not(Test-Path -LiteralPath $UpxPath -PathType Leaf)){throw 'UPX is required to unpack this source package'}
                & $UpxPath -t $file *> (Join-Path $scratch 'upx-test.log');if($LASTEXITCODE){throw 'Packed source DLL failed integrity verification'}
                & $UpxPath -d $file *> (Join-Path $scratch 'upx-unpack.log');if($LASTEXITCODE -or (Is-Packed $file)){throw 'Native DLL could not be unpacked'}
            }
            $records+=[ordered]@{File=$name;WasPacked=$packed;OriginalSHA256=$before;SHA256=(Get-FileHash -LiteralPath $file).Hash;Bytes=(Get-Item -LiteralPath $file).Length}
            $read=[IO.File]::OpenRead($file);try{Copy-EntryBytes $target $name $read}finally{$read.Dispose()}
        }else{$read=$entry.Open();try{Copy-EntryBytes $target $name $read}finally{$read.Dispose()}}
    }
    $metadata=[ordered]@{Variant='Wine compatibility';SourceArchiveSHA256=(Get-FileHash -LiteralPath $SourceArchive).Hash;Compression='ZIP only; native DLLs have no UPX loader';NativeDLLs=$records;RuntimeVerifiedOnProton=$false;APIRequirement='Matching UE4SS exported APIs; version labels alone are insufficient'}|ConvertTo-Json -Depth 7
    $entry=$target.CreateEntry($reserved[0]);$writer=[IO.StreamWriter]::new($entry.Open());try{$writer.Write($metadata)}finally{$writer.Dispose()}
    [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($target,$checker,$reserved[1],[IO.Compression.CompressionLevel]::Optimal)|Out-Null
    $readme=@'
This Wine variant retains the source package's content and license notices.
Native DLLs use ZIP compression, without a runtime UPX decompressor. No game,
UE4SS, mod settings or saves are modified by this packaging tool.

Before launching, use Python 3 to check exported APIs without loading any DLL:
  python3 check-ue4ss-abi.py /path/to/RuneSchema/dlls/main.dll /path/to/ue4ss/UE4SS.dll
Missing export names mean the host cannot load this core. Export parity does not
prove C++ layout compatibility, Wine system libraries or gameplay support.

Keep the exact ue4ss/Mods path spelling. Record the chosen Proton version and
fresh UE4SS startup version/commit. PROTON_LOG=1 enables a Proton debug log.
This variant requires a real Wine/Proton acceptance test before support is claimed.
'@
    $entry=$target.CreateEntry($reserved[2]);$writer=[IO.StreamWriter]::new($entry.Open());try{$writer.Write($readme)}finally{$writer.Dispose()}
    $target.Dispose();$target=$null;$source.Dispose();$source=$null
    Move-Item -LiteralPath $temporary -Destination $DestinationArchive
    Write-Output ('Created Wine compatibility package: '+$DestinationArchive)
}finally{
    if($target){$target.Dispose()};if($source){$source.Dispose()}
    if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary}
    $boundary=[IO.Path]::GetFullPath($scratch)
    $parent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([char[]]@('\','/'))+[IO.Path]::DirectorySeparatorChar
    if(-not $boundary.StartsWith($parent,[StringComparison]::OrdinalIgnoreCase) -or (Get-Item -LiteralPath $boundary).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Unsafe temporary cleanup boundary'}
    Remove-Item -LiteralPath $boundary -Recurse -Force
}
