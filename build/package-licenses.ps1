[CmdletBinding(DefaultParameterSetName = 'Payload')]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [Parameter(Mandatory = $true, ParameterSetName = 'Build')]
    [switch]$FinalizeBuild,
    [Parameter(Mandatory = $true, ParameterSetName = 'Payload')]
    [string]$PayloadRoot,
    [Parameter(ParameterSetName = 'Payload')]
    [string]$ArchivePath
)
# Packaging only. This script does not change compiled code or game behavior.
$ErrorActionPreference = 'Stop'
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$required = @('LICENSE', 'LICENSING.md', 'THIRD_PARTY_NOTICES.md',
    'licenses/PalSchema-MIT.txt', 'licenses/UE4SS-MIT.txt',
    'licenses/nlohmann-json-MIT.txt')
foreach ($relative in $required) {
    $path = Join-Path $RepositoryRoot $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
        (Get-Item -LiteralPath $path).Length -eq 0) {
        throw "Required license file is missing or empty: $relative"
    }
}
$bundle = Join-Path ([IO.Path]::GetTempPath()) ('RuneSchema-Licenses-' + [guid]::NewGuid().ToString('N'))

function Copy-LicenseBundle([string]$Destination) {
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    foreach ($file in Get-ChildItem -LiteralPath $bundle -File -Recurse) {
        $relative = $file.FullName.Substring($bundle.Length + 1)
        $target = Join-Path $Destination $relative
        if (Test-Path -LiteralPath $target -PathType Leaf) {
            $oldHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()
            $newHash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($oldHash -ne $newHash) {
                # Never silently discard a notice inherited from a template.
                $preserved = Join-Path $Destination ('licenses/preserved/' + $oldHash + '.txt')
                New-Item -ItemType Directory -Path (Split-Path -Parent $preserved) -Force | Out-Null
                Copy-Item -LiteralPath $target -Destination $preserved -Force
            }
        }
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $target -Force
    }
}

function Write-LicensedArchive([string]$Payload, [string]$Archive) {
    if (-not (Test-Path -LiteralPath $Payload -PathType Container)) {
        throw "Package payload does not exist: $Payload"
    }
    Copy-LicenseBundle $Payload
    $nestedHelpy = Join-Path $Payload 'plugins/RuneSchema.Helpy'
    if (Test-Path -LiteralPath $nestedHelpy -PathType Container) {
        Copy-LicenseBundle $nestedHelpy
    }
    if ($Archive) {
        $Archive = [IO.Path]::GetFullPath($Archive)
        New-Item -ItemType Directory -Path (Split-Path -Parent $Archive) -Force | Out-Null
        $temporary = $Archive + '.' + [guid]::NewGuid().ToString('N') + '.tmp.zip'
        try {
            # Keep the existing one-payload-directory ZIP layout.
            Compress-Archive -LiteralPath $Payload -DestinationPath $temporary -CompressionLevel Optimal
            Move-Item -LiteralPath $temporary -Destination $Archive -Force
        } finally {
            Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
        }
        Write-Host "Packaged license notices: $Archive" -ForegroundColor Green
    }
}

function Collect-DependencyNotices {
    $hasher = [Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes($RepositoryRoot.ToLowerInvariant())
        $digest = (($hasher.ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') }) -join '')
    } finally { $hasher.Dispose() }
    $buildCache = if ($env:RUNESCHEMA_BUILD_CACHE) {
        [IO.Path]::GetFullPath($env:RUNESCHEMA_BUILD_CACHE)
    } else {
        Join-Path ([IO.Path]::GetTempPath()) ('RSB-' + $digest.Substring(0,12))
    }
    $roots = @(
        @{ Name = 'dependency-cache'; Path = (Join-Path $RepositoryRoot '.cache/dependencies') },
        @{ Name = 'build-cache'; Path = $buildCache }
    )
    $records = @()
    $destination = Join-Path $bundle 'licenses/dependencies'
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    foreach ($root in $roots) {
        if (-not (Test-Path -LiteralPath $root.Path -PathType Container)) { continue }
        $prefix = [IO.Path]::GetFullPath($root.Path).TrimEnd([char[]]@('\','/'))
        foreach ($file in Get-ChildItem -LiteralPath $prefix -File -Recurse | Sort-Object FullName) {
            $relative = $file.FullName.Substring($prefix.Length + 1).Replace('\','/')
            if ($relative -match '(^|/)\.git(/|$)') { continue }
            $namedNotice = $file.Name -match '^(LICENSE|LICENCE|COPYING|NOTICE|COPYRIGHT)($|[._-])'
            $inLicenseDirectory = $relative -match '(^|/)(licenses|licences)/' -and
                $file.Extension -in @('.txt', '.md', '.rst', '')
            if (-not ($namedNotice -or $inLicenseDirectory)) { continue }
            if ($file.Extension -in @('.exe', '.dll', '.png', '.jpg', '.svg', '.pdf')) { continue }
            $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            $noticeName = $hash + '.txt'
            Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $destination $noticeName) -Force
            $records += [pscustomobject]@{
                Source = ($root.Name + '/' + $relative)
                Sha256 = $hash
                NoticeFile = $noticeName
            }
        }
    }
    [ordered]@{
        Schema = 1
        Scope = 'Notices found in local dependency/build caches; not a complete license audit.'
        Files = @($records)
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $destination 'manifest.json') -Encoding utf8
}

try {
    New-Item -ItemType Directory -Path $bundle -Force | Out-Null
    foreach ($name in @('LICENSE', 'LICENSING.md', 'THIRD_PARTY_NOTICES.md')) {
        Copy-Item -LiteralPath (Join-Path $RepositoryRoot $name) -Destination $bundle -Force
    }
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'licenses') -Destination $bundle -Recurse -Force
    if ($FinalizeBuild) {
        $buildScript = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'build/build.ps1') -Raw
        $versionMatch = [regex]::Match($buildScript, '(?m)^\$Version\s*=\s*''([^'']+)''')
        if (-not $versionMatch.Success -or $versionMatch.Groups[1].Value -notmatch '^[0-9A-Za-z._-]+$') {
            throw 'Could not determine the current package version from build/build.ps1.'
        }
        $version = $versionMatch.Groups[1].Value
        $dist = Join-Path $RepositoryRoot 'dist'
        $packages = @(
            @{ Name = "RuneSchema-$version-Core"; Payload = 'RuneSchema' },
            @{ Name = "RuneSchema-$version-Universal"; Payload = 'RuneSchema' },
            @{ Name = "RuneSchema.Helpy-$version"; Payload = 'RuneSchema.Helpy' }
        )
        Collect-DependencyNotices
        $count = 0
        foreach ($package in $packages) {
            $archive = Join-Path $dist ($package.Name + '.zip')
            if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) { continue }
            $payload = Join-Path (Join-Path $dist $package.Name) $package.Payload
            Write-LicensedArchive $payload $archive
            $count++
        }
        if ($count -eq 0) { throw 'No current Core, Universal, or Helpy build archive was found to finalize.' }
        # Accompany the loose developer DLL and convenience install trees too.
        Copy-LicenseBundle $dist
        foreach ($relative in @('plugins/Universal', 'plugins/RuneSchema.Helpy')) {
            $path = Join-Path $RepositoryRoot $relative
            if (Test-Path -LiteralPath $path -PathType Container) { Copy-LicenseBundle $path }
        }
    } else {
        Write-LicensedArchive ([IO.Path]::GetFullPath($PayloadRoot)) $ArchivePath
    }
} finally {
    Remove-Item -LiteralPath $bundle -Recurse -Force -ErrorAction SilentlyContinue
}
