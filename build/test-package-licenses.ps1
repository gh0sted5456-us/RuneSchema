$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$helper = Join-Path $PSScriptRoot 'package-licenses.ps1'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('RuneSchema-LicenseTest-' + [guid]::NewGuid().ToString('N'))
$oldBuildCache = $env:RUNESCHEMA_BUILD_CACHE
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Assert-Archive([string]$Path, [string]$Prefix) {
    $archive = [IO.Compression.ZipFile]::OpenRead($Path)
    try {
        $entries = @($archive.Entries | ForEach-Object { $_.FullName.Replace('\','/') })
        foreach ($name in @('LICENSE', 'LICENSING.md', 'THIRD_PARTY_NOTICES.md',
            'licenses/PalSchema-MIT.txt', 'licenses/UE4SS-MIT.txt', 'licenses/nlohmann-json-MIT.txt')) {
            Assert-True ($entries -contains "$Prefix/$name") "Missing $Prefix/$name in $Path"
        }
        Assert-True (($entries | Select-Object -Unique).Count -eq $entries.Count) 'Duplicate ZIP entries.'
    } finally { $archive.Dispose() }
}
try {
    New-Item -ItemType Directory -Path $fixture -Force | Out-Null
    foreach ($name in @('LICENSE', 'LICENSING.md', 'THIRD_PARTY_NOTICES.md', 'licenses')) {
        Copy-Item -LiteralPath (Join-Path $repository $name) -Destination $fixture -Recurse -Force
    }
    $fixtureBuild = Join-Path $fixture 'build'
    New-Item -ItemType Directory -Path $fixtureBuild -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $fixtureBuild 'build.ps1') -Value '$Version = ''test-1''' -Encoding ascii
    $env:RUNESCHEMA_BUILD_CACHE = Join-Path $fixture 'fake-build-cache'
    $dependency = Join-Path $env:RUNESCHEMA_BUILD_CACHE '_deps/example-src'
    New-Item -ItemType Directory -Path $dependency -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $dependency 'LICENSE') -Value 'Synthetic dependency notice for packaging test.' -Encoding ascii
    $before = @{}
    foreach ($name in @('RuneSchema-test-1-Core', 'RuneSchema-test-1-Universal', 'RuneSchema.Helpy-test-1')) {
        $prefix = if ($name -like 'RuneSchema.Helpy-*') { 'RuneSchema.Helpy' } else { 'RuneSchema' }
        $payload = Join-Path $fixture "dist/$name/$prefix"
        New-Item -ItemType Directory -Path $payload -Force | Out-Null
        $binary = Join-Path $payload 'fixture.dll'
        [IO.File]::WriteAllBytes($binary, [byte[]](0,1,2,3,255))
        $before[$binary] = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
        Set-Content -LiteralPath (Join-Path $payload 'LICENSE') -Value 'Existing upstream notice: preserve this exact file.' -Encoding ascii
        if ($name -like '*-Universal') {
            New-Item -ItemType Directory -Path (Join-Path $payload 'plugins/RuneSchema.Helpy') -Force | Out-Null
        }
        Compress-Archive -LiteralPath $payload -DestinationPath (Join-Path $fixture "dist/$name.zip")
    }
    & $helper -RepositoryRoot $fixture -FinalizeBuild
    & $helper -RepositoryRoot $fixture -FinalizeBuild
    foreach ($name in @('RuneSchema-test-1-Core', 'RuneSchema-test-1-Universal', 'RuneSchema.Helpy-test-1')) {
        $prefix = if ($name -like 'RuneSchema.Helpy-*') { 'RuneSchema.Helpy' } else { 'RuneSchema' }
        Assert-Archive (Join-Path $fixture "dist/$name.zip") $prefix
        $payload = Join-Path $fixture "dist/$name/$prefix"
        $preserved = @(Get-ChildItem -LiteralPath (Join-Path $payload 'licenses/preserved') -Filter '*.txt')
        Assert-True ($preserved.Count -eq 1) 'Existing notice was lost or duplicated on the second run.'
        Assert-True ((Get-Content -LiteralPath $preserved[0].FullName -Raw).Contains('Existing upstream notice')) 'Existing notice contents changed.'
        $manifest = Get-Content -LiteralPath (Join-Path $payload 'licenses/dependencies/manifest.json') -Raw | ConvertFrom-Json
        Assert-True (@($manifest.Files).Count -eq 1) 'Synthetic dependency notice was not collected.'
    }
    foreach ($binary in $before.Keys) {
        Assert-True ((Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash -eq $before[$binary]) 'Binary bytes changed.'
    }
    Assert-True (Test-Path -LiteralPath (Join-Path $fixture 'dist/RuneSchema-test-1-Universal/RuneSchema/plugins/RuneSchema.Helpy/LICENSE')) 'Nested Helpy notice missing.'
    $example = Join-Path $fixture 'example/ExampleMod'
    New-Item -ItemType Directory -Path $example -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $example 'example.json') -Value '{}' -Encoding ascii
    & $helper -RepositoryRoot $fixture -PayloadRoot $example -ArchivePath (Join-Path $fixture 'example.zip')
    Assert-Archive (Join-Path $fixture 'example.zip') 'ExampleMod'
    Remove-Item -LiteralPath (Join-Path $fixture 'LICENSE') -Force
    $failed = $false
    try { & $helper -RepositoryRoot $fixture -PayloadRoot $example } catch { $failed = $true }
    Assert-True $failed 'Missing project license was not rejected.'
    Write-Host 'PASS: package layout, repeated runs, notice preservation, dependencies, binary hashes, examples, and missing-license handling.' -ForegroundColor Green
} finally {
    $env:RUNESCHEMA_BUILD_CACHE = $oldBuildCache
    Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue
}
