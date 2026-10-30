$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$temporaryBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$fixture=[IO.Path]::GetFullPath((Join-Path $temporaryBase ('RuneSchemaVortexContract-'+[Guid]::NewGuid().ToString('N'))))
$payload=Join-Path $fixture 'RuneSchema'
function Need([bool]$condition,[string]$message){if(!$condition){throw $message}}
function Put([string]$relative,[string]$value='fixture'){$path=Join-Path $payload $relative;New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null;[IO.File]::WriteAllText($path,$value)}
try{
    Put 'dlls/main.dll';Put 'plugins/RuneSchema.Helpy/dll/RuneSchema.Helpy.dll'
    Put 'plugins/RuneSchema.Helpy/dll/Helpy.exe' 'optional desktop fixture'
    Put 'plugins/RSNetworking/plugin.json' '{}';Put 'plugins/RuneSchema.RegistryBridge/plugin.json' '{}'
    foreach($extension in @('pak','ucas','utoc')){Put "plugins/RSNetworking/paks/Networking/Networking_P.$extension";Put "plugins/RuneSchema.RegistryBridge/paks/RegistryBridge/RegistryBridge_P.$extension"}
    Put 'plugins/RSNetworking/paks/RegistryBridge/RegistryBridge_P.pak' 'retired partial duplicate'
    foreach($platform in @('Steam','GamePass')){
        $output=Join-Path $fixture "$platform.zip"
        & (Join-Path $repo 'build/package-vortex.ps1') -RuneSchemaRoot $payload -OutputPath $output -Platform $platform | Out-Null
        $stream=[IO.File]::OpenRead($output);$zip=New-Object IO.Compression.ZipArchive($stream,[IO.Compression.ZipArchiveMode]::Read,$false)
        try{
            $reader=New-Object IO.StreamReader($zip.GetEntry('vortex_override_instructions.json').Open());try{$instructions=ConvertFrom-Json $reader.ReadToEnd()}finally{$reader.Dispose()}
            Need ($instructions[0].type -eq 'setmodtype' -and $instructions[0].value -eq 'dinput') 'Vortex must target game root'
            $binary=if($platform -eq 'GamePass'){'WinGDK'}else{'Win64'}
            $copies=@($instructions|Where-Object {$_.type -eq 'copy'})
            Need (@($copies|Where-Object {$_.source -eq 'RuneSchema/dlls/main.dll' -and $_.destination -eq "RSDragonwilds/Binaries/$binary/ue4ss/Mods/RuneSchema/dlls/main.dll"}).Count -eq 1) 'Core DLL destination differs'
            foreach($copy in $copies){Need ($null -ne $zip.GetEntry($copy.source)) 'Override references absent source';Need (!$copy.destination.Contains('..') -and !$copy.destination.StartsWith('/')) 'Unsafe destination'}
            Need (@($copies|Where-Object {$_.destination.StartsWith('RSDragonwilds/Content/Paks/~mods/')}).Count -eq 1) 'Only the explanatory note belongs in ordinary PAK mods'
            Need (@($copies|Where-Object {$_.source -like '*plugins/RSNetworking/paks/RegistryBridge/*'}).Count -eq 0) 'Retired duplicate must not be packaged'
            Need (@($copies|Where-Object {$_.source -like '*RegistryBridge_P.*'}).Count -eq 3) 'One complete canonical bridge triplet required'
            Need (@($zip.Entries|Where-Object {$_.Name -in @('UE4SS.dll','dinput8.dll')}).Count -eq 0) 'Framework package must not replace the loader'
            Need (@($zip.Entries|Where-Object {$_.Name -eq 'Helpy.exe'}).Count -eq 1) 'The complete runtime includes the primary Helpy application'
        }finally{$zip.Dispose();$stream.Dispose()}
    }
    $desktop=Join-Path $fixture 'desktop.zip'
    & (Join-Path $repo 'build/package-helpy-desktop.ps1') -Executable (Join-Path $payload 'plugins/RuneSchema.Helpy/dll/Helpy.exe') -OutputPath $desktop|Out-Null
    $stream=[IO.File]::OpenRead($desktop);$zip=New-Object IO.Compression.ZipArchive($stream,[IO.Compression.ZipArchiveMode]::Read,$false)
    try{Need ($null -ne $zip.GetEntry('Helpy.exe') -and $null -ne $zip.GetEntry('HELPY-STANDALONE.md') -and $null -ne $zip.GetEntry('LICENSE')) 'Optional desktop archive includes executable, setup guide and license';Need (@($zip.Entries|Where-Object {$_.Name -in @('main.dll','RuneSchema.Helpy.dll')}).Count -eq 0) 'Optional archive must not silently replace runtime DLLs'}finally{$zip.Dispose();$stream.Dispose()}
    Put 'RuneSchema.log' 'private runtime data';$rejected=$false
    try{& (Join-Path $repo 'build/package-vortex.ps1') -RuneSchemaRoot $payload -OutputPath (Join-Path $fixture 'bad.zip') | Out-Null}catch{$rejected=$true}
    Need $rejected 'Runtime logs must not leak into release archives'
    Write-Output 'Vortex package destination, complete-container and privacy checks passed.'
}finally{
    if(!$fixture.StartsWith($temporaryBase.TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or !(Split-Path $fixture -Leaf).StartsWith('RuneSchemaVortexContract-')){throw 'Refusing cleanup outside private test root'}
    if(Test-Path -LiteralPath $fixture){Remove-Item -LiteralPath $fixture -Recurse -Force}
}
