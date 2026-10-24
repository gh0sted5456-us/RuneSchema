param([string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference='Stop'
$root=Join-Path ([IO.Path]::GetTempPath()) ('RuneSchema-MappingPackageTest-'+[guid]::NewGuid().ToString('N'))
$payload=Join-Path $root 'input/RuneSchema'
try{
    foreach($relative in @('dlls/main.dll','enabled.txt','scripts/main.lua','plugins/plugins.txt','settings/settings.ini','dlls/mappings/Mappings.usmap','dlls/mappings/README.txt')){
        $file=Join-Path $payload $relative;New-Item -ItemType Directory -Force -Path (Split-Path -Parent $file) | Out-Null
        [IO.File]::WriteAllText($file,'disposable mapping-package fixture')
    }
    $before=(Get-FileHash -LiteralPath (Join-Path $payload 'dlls/main.dll')).Hash
    $archive=Join-Path $root 'input.zip';$output=Join-Path $root 'clean.zip'
    Compress-Archive -LiteralPath $payload -DestinationPath $archive
    & (Join-Path $PSScriptRoot 'package-runtime.ps1') -SourceArchive $archive -DestinationArchive $output -RepositoryRoot $RepositoryRoot | Out-Null
    $zip=[IO.Compression.ZipFile]::OpenRead($output)
    try{
        if(@($zip.Entries | Where-Object {$_.FullName.Replace('\','/') -match '^RuneSchema/dlls/mappings(?:/|$)'}).Count){throw 'Legacy mapping directory entered package'}
        $entry=@($zip.Entries | Where-Object {$_.FullName.Replace('\','/') -eq 'RuneSchema/dlls/main.dll'})
        if($entry.Count -ne 1){throw 'Core DLL was lost'}
        $sha=[Security.Cryptography.SHA256]::Create();$stream=$entry[0].Open()
        try{if([BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','') -ne $before){throw 'Core DLL bytes changed'}}finally{$stream.Dispose();$sha.Dispose()}
    }finally{$zip.Dispose()}
    $build=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'build.ps1'))
    if($build -match 'Ensure-Mappings|Update-MappingsLock|\$Mappings\.Path'){throw 'Build still requires a packaged mapping'}
    if([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'build.bat')) -match '-UpdateMappings'){throw 'Launcher still requests retired mapping download'}
    Write-Output 'PASS: no packaged mappings or mapping download dependency; core bytes preserved.'
}finally{
    $temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/')
    if(-not [IO.Path]::GetFullPath($root).StartsWith($temp+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($root) -notmatch '^RuneSchema-MappingPackageTest-[a-f0-9]{32}$'){throw 'Fixture cleanup escaped unique temp root'}
    if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
}
