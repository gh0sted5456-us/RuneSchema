$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$helper = Join-Path $PSScriptRoot 'package-licenses.ps1'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('RuneSchema-LicenseTest-' + [guid]::NewGuid().ToString('N'))
$oldBuildCache = $env:RUNESCHEMA_BUILD_CACHE
$oldGitHubRefName = $env:GITHUB_REF_NAME
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Get-Sha256File([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $hasher = [Security.Cryptography.SHA256]::Create()
    try {
        return (($hasher.ComputeHash($stream) | ForEach-Object { $_.ToString('x2') }) -join '')
    } finally {
        $hasher.Dispose()
        $stream.Dispose()
    }
}
function Assert-Archive([string]$Path, [string]$Prefix) {
    $archive = [IO.Compression.ZipFile]::OpenRead($Path)
    try {
        $entries = @($archive.Entries | ForEach-Object { $_.FullName.Replace('\','/') })
        $licenseRoot = "$Prefix/settings/licenses"
        Assert-True ($entries -contains "$licenseRoot/RuneSchema-Licenses.txt") "Missing consolidated license document in $Path"
        $licenseFiles = @($entries | Where-Object { $_ -like "$licenseRoot/*" -and -not $_.EndsWith('/') })
        Assert-True ($licenseFiles.Count -eq 1) "Expected one consolidated license file in $licenseRoot; found: $($licenseFiles -join ', ')"
        $legalEntries = @($entries | Where-Object {
            $_ -match '(?i)(license|licensing|authors|contributing|notice|copying)'
        })
        $outsideCanonicalRoot = @($legalEntries | Where-Object { $_ -notlike "$licenseRoot/*" })
        Assert-True ($outsideCanonicalRoot.Count -eq 0) "Legal files remain outside $licenseRoot in $Path`: $($outsideCanonicalRoot -join ', ')"
        Assert-True (($entries | Select-Object -Unique).Count -eq $entries.Count) 'Duplicate ZIP entries.'
        $entry = $archive.Entries | Where-Object { $_.FullName.Replace('\','/') -eq "$licenseRoot/RuneSchema-Licenses.txt" } | Select-Object -First 1
        $reader = [IO.StreamReader]::new($entry.Open())
        try { $consolidated = $reader.ReadToEnd() } finally { $reader.Dispose() }
        foreach ($name in @('LICENSE', 'AUTHORS.md', 'LICENSING.md', 'CONTRIBUTING.md', 'THIRD_PARTY_NOTICES.md',
            'licenses/PalSchema-MIT.txt', 'licenses/UE4SS-MIT.txt', 'licenses/nlohmann-json-MIT.txt')) {
            $expected = (Get-Content -LiteralPath (Join-Path $repository $name) -Raw).Trim()
            Assert-True ($consolidated.Contains($expected)) "Consolidated document omits complete source text: $name"
        }
        foreach ($link in @('https://github.com/gh0sted5456-us/RuneSchema', 'https://github.com/Okaetsu/PalSchema',
            'https://github.com/UE4SS-RE/RE-UE4SS', 'https://github.com/nlohmann/json')) {
            Assert-True ($consolidated.Contains($link)) "Consolidated document omits upstream link: $link"
        }
        Assert-True ($consolidated.Contains('Synthetic dependency notice for packaging test.')) 'Collected dependency notice is not embedded.'
    } finally { $archive.Dispose() }
}
try {
    New-Item -ItemType Directory -Path $fixture -Force | Out-Null
    foreach ($name in @('LICENSE', 'AUTHORS.md', 'LICENSING.md', 'CONTRIBUTING.md', 'THIRD_PARTY_NOTICES.md', 'licenses')) {
        Copy-Item -LiteralPath (Join-Path $repository $name) -Destination $fixture -Recurse -Force
    }
    $fixtureBuild = Join-Path $fixture 'build'
    New-Item -ItemType Directory -Path $fixtureBuild -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $fixtureBuild 'build.ps1') -Value '$Version = ''0.0.0.0e''' -Encoding ascii
    $env:RUNESCHEMA_BUILD_CACHE = Join-Path $fixture 'fake-build-cache'
    # The fixture deliberately exercises an experimental package. Keep the
    # calling CI branch from leaking into its release-channel validation.
    $env:GITHUB_REF_NAME = 'experimental'
    $dependency = Join-Path $env:RUNESCHEMA_BUILD_CACHE '_deps/example-src'
    New-Item -ItemType Directory -Path $dependency -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $dependency 'LICENSE') -Value 'Synthetic dependency notice for packaging test.' -Encoding ascii
    $before = @{}
    foreach ($name in @('RuneSchema-0.0.0.0e-Core', 'RuneSchema-0.0.0.0e-Universal', 'RuneSchema.Helpy-0.0.0.0e')) {
        $prefix = if ($name -like 'RuneSchema.Helpy-*') { 'RuneSchema.Helpy' } else { 'RuneSchema' }
        $payload = Join-Path $fixture "dist/$name/$prefix"
        New-Item -ItemType Directory -Path $payload -Force | Out-Null
        $binary = Join-Path $payload 'fixture.dll'
        [IO.File]::WriteAllBytes($binary, [byte[]](0,1,2,3,255))
        $before[$binary] = Get-Sha256File $binary
        Set-Content -LiteralPath (Join-Path $payload 'LICENSE') -Value 'Existing upstream notice: preserve this exact file.' -Encoding ascii
        if ($name -like '*-Universal') {
            New-Item -ItemType Directory -Path (Join-Path $payload 'plugins/RuneSchema.Helpy') -Force | Out-Null
        }
        Compress-Archive -LiteralPath $payload -DestinationPath (Join-Path $fixture "dist/$name.zip")
    }
    & $helper -RepositoryRoot $fixture -FinalizeBuild
    & $helper -RepositoryRoot $fixture -FinalizeBuild
    foreach ($name in @('RuneSchema-0.0.0.0e-Core', 'RuneSchema-0.0.0.0e-Universal', 'RuneSchema.Helpy-0.0.0.0e')) {
        $prefix = if ($name -like 'RuneSchema.Helpy-*') { 'RuneSchema.Helpy' } else { 'RuneSchema' }
        Assert-Archive (Join-Path $fixture "dist/$name.zip") "$name/$prefix"
        $payload = Join-Path $fixture "dist/$name/$prefix"
        $licenseFiles = @(Get-ChildItem -LiteralPath (Join-Path $payload 'settings/licenses') -File -Recurse)
        Assert-True ($licenseFiles.Count -eq 1 -and $licenseFiles[0].Name -eq 'RuneSchema-Licenses.txt') 'Payload does not contain exactly one consolidated license document.'
        $licenseText = Get-Content -LiteralPath $licenseFiles[0].FullName -Raw
        Assert-True ($licenseText.Contains('Existing upstream notice: preserve this exact file.')) 'Existing inherited notice was lost on the second run.'
    }
    foreach ($binary in $before.Keys) {
        Assert-True ((Get-Sha256File $binary) -eq $before[$binary]) 'Binary bytes changed.'
    }
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $fixture 'dist/RuneSchema-0.0.0.0e-Universal/RuneSchema/plugins/RuneSchema.Helpy/LICENSE'))) 'Nested Helpy license duplicate remains.'
    $nested = Join-Path $fixture 'dist/RuneSchema-0.0.0.0e-Universal/RuneSchema/plugins/RuneSchema.Helpy'
    foreach ($name in @('AUTHORS.md', 'CONTRIBUTING.md')) {
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $nested $name))) "Nested Helpy $name duplicate remains."
    }
    $credits = Get-Content -LiteralPath (Join-Path $fixture 'AUTHORS.md') -Raw
    foreach ($member in @('Jonesing4Space', 'NuLLZz', 'Snorkles', 'CHP', 'gh0sted5456-us')) {
        Assert-True ($credits.Contains($member)) "Required community credit or publisher missing: $member"
    }
    $example = Join-Path $fixture 'example/ExampleMod'
    New-Item -ItemType Directory -Path $example -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $example 'example.json') -Value '{}' -Encoding ascii
    & $helper -RepositoryRoot $fixture -PayloadRoot $example -ArchivePath (Join-Path $fixture 'example.zip')
    Assert-Archive (Join-Path $fixture 'example.zip') 'ExampleMod'
    Remove-Item -LiteralPath (Join-Path $fixture 'AUTHORS.md') -Force
    $failed = $false
    try { & $helper -RepositoryRoot $fixture -PayloadRoot $example } catch { $failed = $true }
    Assert-True $failed 'Missing community credits were not rejected.'
    Copy-Item -LiteralPath (Join-Path $repository 'AUTHORS.md') -Destination $fixture
    Remove-Item -LiteralPath (Join-Path $fixture 'LICENSE') -Force
    $failed = $false
    try { & $helper -RepositoryRoot $fixture -PayloadRoot $example } catch { $failed = $true }
    Assert-True $failed 'Missing project license was not rejected.'
    Write-Host 'PASS: one-file consolidated license layout, complete notice text, upstream links, repeated runs, inherited notices, dependencies, binary hashes, examples, and missing-notice handling.' -ForegroundColor Green
} finally {
    $env:RUNESCHEMA_BUILD_CACHE = $oldBuildCache
    $env:GITHUB_REF_NAME = $oldGitHubRefName
    Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue
}
