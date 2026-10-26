$ErrorActionPreference='Stop'
$tool=Join-Path $PSScriptRoot '../../resources/tools/build-helpy-cache.ps1'
function Check([bool]$Value,[string]$Message){if(!$Value){throw $Message}}
function ZipBytes([string[]]$Names,[bool]$Link=$false){
    $memory=[IO.MemoryStream]::new();$archive=[IO.Compression.ZipArchive]::new($memory,[IO.Compression.ZipArchiveMode]::Create,$true)
    try {foreach($name in $Names){$entry=$archive.CreateEntry($name);if($Link){$entry.ExternalAttributes=[BitConverter]::ToInt32([BitConverter]::GetBytes([uint32]2717843456),0)};$stream=$entry.Open();try{$stream.WriteByte(42)}finally{$stream.Dispose()}}}finally{$archive.Dispose()}
    $bytes=$memory.ToArray();$memory.Dispose();return ,$bytes
}
. $tool
Check ((Get-CacheExtractionPercent 0 3216835819) -eq 5) 'Large ZIP start failed'
Check ((Get-CacheExtractionPercent 1608417910 3216835819) -eq 15) 'Large ZIP midpoint failed'
Check ((Get-CacheExtractionPercent 3216835819 3216835819) -eq 25) 'Large ZIP completion failed'
Check ((Get-CacheExtractionPercent 16GB 16GB) -eq 25) 'Maximum supported extraction size failed'
function Plan([string[]]$Names,[bool]$Link=$false){
    $memory=[IO.MemoryStream]::new((ZipBytes $Names $Link));$archive=[IO.Compression.ZipArchive]::new($memory,[IO.Compression.ZipArchiveMode]::Read)
    try {return Get-CacheArchivePlan $archive}finally{$archive.Dispose();$memory.Dispose()}
}
$valid=@('MyMod/paks/Mod.pak','MyMod/paks/Mod.utoc','MyMod/paks/Mod.ucas','MyMod/background.png','MyMod/never-run.ps1')
$plan=Plan $valid;Check ($plan.Files.Count -eq 4) 'ZIP scripts must not be staged';Check ($plan.Banner.Leaf -eq 'background.png') 'Background selection failed'
Check ((Plan @('Legacy.pak')).Files.Count -eq 1) 'Legacy PAK-only input failed'
Check ((Plan @('Nested/Mod.pak','Nested/Mod.utoc','Nested/Mod.ucas')).Files.Count -eq 3) 'Optional background must not be required'
$mapped=Plan ($valid+@('Nested/current.usmap'));Check ($mapped.Mapping.Leaf -eq 'current.usmap' -and $mapped.Files.Count -eq 5) 'Optional ZIP mapping was not selected'
foreach($bad in @('../outside.pak','C:/outside.pak','/outside.pak','Dir/NUL.txt','Dir/trailing.pak ','Dir/../other.pak')){
    $rejected=$false;try{Plan ($valid+@($bad)) | Out-Null}catch{$rejected=$true};Check $rejected "Unsafe path accepted: $bad"
}
foreach($badSet in @(@('A.pak','a.PAK'),@('A.pak','A.utoc'),@('One/A.pak','Two/A.pak'),@('A.pak','One/background.png','Two/background.png'),@('A.pak','One.usmap','Two.usmap'))){
    $rejected=$false;try{Plan $badSet | Out-Null}catch{$rejected=$true};Check $rejected 'Ambiguous or incomplete package accepted'
}
$rejected=$false;try{Plan @('Link.pak') $true | Out-Null}catch{$rejected=$true};Check $rejected 'ZIP symlink accepted'
$root=Join-Path ([IO.Path]::GetTempPath()) ('RS-zip-authoring-tests-'+[Guid]::NewGuid().ToString('N'))
$null=Assert-CachePath $root ([IO.Path]::GetTempPath());New-Item -ItemType Directory -Path $root | Out-Null
try {
    $zip=Join-Path $root 'MyMod.zip';[IO.File]::WriteAllBytes($zip,(ZipBytes $valid));$original=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
    $cache=Join-Path $root 'cache';New-Item -ItemType Directory -Path $cache | Out-Null;[IO.File]::WriteAllText((Join-Path $cache 'old.txt'),'preserve me')
    # Mock only the external compiler/exporter; exercise real ZIP staging and folder publication.
    function dotnet {
        if($args[0] -eq '--list-sdks'){'10.0.401 [test]';return}
        if($args -contains '--progress'){
            $built=$args[([array]::IndexOf($args,'--output')+1)];$mod=$args[([array]::IndexOf($args,'--mod-root')+1)]
            Check ((Get-ChildItem -LiteralPath (Join-Path $mod 'paks') -File).Count -eq 3) 'Wrong staged PAK set'
            Check (Test-Path -LiteralPath (Join-Path $mod 'background.png')) 'Background was not staged'
            Check (!(Test-Path -LiteralPath (Join-Path $mod 'never-run.ps1'))) 'ZIP code was staged'
            Check ($args -contains '--input-root') 'Original ZIP location was not passed for automatic discovery'
            if($script:FailBuild){$global:LASTEXITCODE=9;return}
            New-Item -ItemType Directory -Path $built -Force | Out-Null
            [IO.File]::WriteAllText((Join-Path $built 'offline-helpy.rsmk-cache'),'fixture bundle')
            [IO.File]::WriteAllText((Join-Path $built 'offline-helpy-report.json'),'{}')
            'RS_PROGRESS|Reading items|1|1'
        }
        $global:LASTEXITCODE=0
    }
    . $tool -ZipPath $zip -Game 'fixture-game' -Mapping 'fixture.usmap' -Oodle 'fixture.dll' -ModId 'MyMod' -OutputDirectory $cache
    Invoke-CacheZip
    Check (Test-Path -LiteralPath (Join-Path $cache 'offline-helpy.rsmk-cache')) 'Completed output was not published'
    $backups=@(Get-ChildItem -LiteralPath $root -Directory -Filter 'cache.previous-*');Check ($backups.Count -eq 1) 'Previous output was not backed up'
    Check ([IO.File]::ReadAllText((Join-Path $backups[0].FullName 'old.txt')) -eq 'preserve me') 'Previous output changed'
    Check ((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -eq $original) 'Input ZIP changed'
    . $tool -ZipPath $zip -ModId 'MyMod' -OutputDirectory $cache
    Invoke-CacheZip
    Check (Test-Path -LiteralPath (Join-Path $cache 'offline-helpy.rsmk-cache')) 'Zero-configuration invocation failed'
    [IO.File]::WriteAllBytes($zip,(ZipBytes ($valid+@('Nested/Current.usmap'))))
    function dotnet {
        if($args[0] -eq '--list-sdks'){'10.0.401 [test]';return}
        if($args -contains '--progress'){
            $map=$args[([array]::IndexOf($args,'--mapping')+1)]
            Check ([IO.Path]::GetFileName($map) -eq 'provided.usmap' -and (Test-Path -LiteralPath $map)) 'ZIP mapping was not passed as isolated offline input'
            $mod=$args[([array]::IndexOf($args,'--mod-root')+1)]
            Check (!(Test-Path -LiteralPath (Join-Path $mod 'provided.usmap'))) 'ZIP mapping must not affect mod content fingerprints'
            $global:LASTEXITCODE=9;return
        }
        $global:LASTEXITCODE=0
    }
    $script:FailBuild=$true;$rejected=$false;try{Invoke-CacheZip}catch{$rejected=$true}
    Check $rejected 'Exporter failure was ignored';Check ([IO.File]::ReadAllText((Join-Path $cache 'offline-helpy.rsmk-cache')) -eq 'fixture bundle') 'Failed build changed current cache'
    Check (@(Get-ChildItem -LiteralPath $root -Directory -Filter '.RuneSchema.Cache.*').Count -eq 0) 'Staging folder was left behind'
    'PASS: ZIP paths, duplicates, links, trios, background, staged-file allowlist, backup publication, input preservation and failure retention.'
}finally{
    $null=Assert-CachePath $root ([IO.Path]::GetTempPath());Remove-Item -LiteralPath $root -Recurse -Force
}
