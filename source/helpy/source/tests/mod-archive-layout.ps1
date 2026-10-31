param([string]$Header)
$ErrorActionPreference='Stop';Add-Type -AssemblyName System.IO.Compression.FileSystem
$source=[IO.File]::ReadAllText($Header);$extractor=($source -split 'R"RSPS\(')[1] -split '\)RSPS"' | Select-Object -First 1
if(!$extractor){throw 'Archive extractor is missing'}
$root=Join-Path ([IO.Path]::GetTempPath()) ('RS-import-layout-'+[Guid]::NewGuid().ToString('N'));[IO.Directory]::CreateDirectory($root)|Out-Null
try {
    foreach($native in @($false,$true)){
        $job=Join-Path $root ([Guid]::NewGuid().ToString('N'));$payload=Join-Path $job 'payload'
        $owner=Join-Path $payload 'Binaries/WinGDK/ue4ss/Mods/RuneSchema/mods/Example'
        $marker=Join-Path $payload 'Content/Paks/~mods'
        [IO.Directory]::CreateDirectory($owner)|Out-Null;[IO.Directory]::CreateDirectory($marker)|Out-Null
        [IO.File]::WriteAllText((Join-Path $owner 'ID.txt'),'{"ModID":"Example","Author":"Fixture"}')
        [IO.File]::WriteAllText((Join-Path $marker 'runeschema.txt'),'RuneSchema')
        if($native){$folder=Join-Path $payload 'Binaries/WinGDK/ue4ss/Mods/Native/dlls';[IO.Directory]::CreateDirectory($folder)|Out-Null;[IO.File]::WriteAllText((Join-Path $folder 'main.dll'),'inert')}
        $archive=Join-Path $job 'mod.zip';[IO.Compression.ZipFile]::CreateFromDirectory($payload,$archive)
        [IO.File]::WriteAllText((Join-Path $job 'input.json'),(@{Archive=$archive}|ConvertTo-Json))
        $script=Join-Path $job 'import.ps1';[IO.File]::WriteAllText($script,$extractor)
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script
        $result=Get-Content -LiteralPath (Join-Path $job 'result.json') -Raw | ConvertFrom-Json
        if($native){if($result.Ok -or $result.Status -notmatch 'native UE4SS mod'){throw 'Native companion must not be silently discarded'}}
        elseif(!$result.Ok -or !(Test-Path -LiteralPath (Join-Path $job 'Example/ID.txt'))){throw 'Binaries/Content marker package must import'}
    }
    'Archive layout checks passed: deployment marker accepted; native companion clearly refused.'
}finally{
    $absolute=[IO.Path]::GetFullPath($root);if([IO.Path]::GetDirectoryName($absolute) -eq [IO.Path]::GetTempPath().TrimEnd('\') -and [IO.Path]::GetFileName($absolute) -match '^RS-import-layout-[0-9a-f]{32}$'){Remove-Item -LiteralPath $absolute -Recurse -Force}
}
