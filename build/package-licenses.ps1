[CmdletBinding(DefaultParameterSetName = 'Payload')]
param(
    [string]$RepositoryRoot,
    [Parameter(Mandatory = $true, ParameterSetName = 'Build')]
    [switch]$FinalizeBuild,
    [Parameter(Mandatory = $true, ParameterSetName = 'Payload')]
    [string]$PayloadRoot,
    [Parameter(ParameterSetName = 'Payload')]
    [string]$ArchivePath
)
# Packaging only. This script does not change compiled code or game behavior.
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Split-Path -Parent $PSScriptRoot
}
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$required = @('LICENSE', 'AUTHORS.md', 'LICENSING.md', 'CONTRIBUTING.md', 'THIRD_PARTY_NOTICES.md',
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

function Remove-LegacyLicenseLayout([string]$Payload) {
    foreach ($name in @('LICENSE', 'AUTHORS.md', 'LICENSING.md', 'CONTRIBUTING.md', 'THIRD_PARTY_NOTICES.md')) {
        $legacy = Join-Path $Payload $name
        Remove-Item -LiteralPath $legacy -Force -ErrorAction SilentlyContinue
    }
    $legacyLicenses = Join-Path $Payload 'licenses'
    Remove-Item -LiteralPath $legacyLicenses -Recurse -Force -ErrorAction SilentlyContinue
}

function Install-LicenseBundle([string]$Payload) {
    $destination = Join-Path $Payload 'settings/licenses'
    $inherited = @()
    $existingAppendix = ''
    $existingConsolidated = Join-Path $destination 'RuneSchema-Licenses.txt'
    if (Test-Path -LiteralPath $existingConsolidated -PathType Leaf) {
        $existingText = Get-Content -LiteralPath $existingConsolidated -Raw
        $needle = ('=' * 78) + [Environment]::NewLine + 'Inherited payload'
        $appendixStart = $existingText.IndexOf($needle, [StringComparison]::Ordinal)
        if ($appendixStart -ge 0) { $existingAppendix = $existingText.Substring($appendixStart) }
    }
    $legacyMain = Join-Path $Payload 'LICENSE'
    if (Test-Path -LiteralPath $legacyMain -PathType Leaf) {
        $legacyHash = (Get-FileHash -LiteralPath $legacyMain -Algorithm SHA256).Hash.ToLowerInvariant()
        $projectHash = (Get-FileHash -LiteralPath (Join-Path $RepositoryRoot 'LICENSE') -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($legacyHash -ne $projectHash) {
            $inherited += [pscustomobject]@{ Name = 'Inherited payload license'; Content = (Get-Content -LiteralPath $legacyMain -Raw); Hash = $legacyHash }
        }
    }
    $legacyFolder = Join-Path $Payload 'licenses'
    if (Test-Path -LiteralPath $legacyFolder -PathType Container) {
        foreach ($file in Get-ChildItem -LiteralPath $legacyFolder -File -Recurse) {
            $inherited += [pscustomobject]@{
                Name = 'Inherited payload notice: ' + $file.Name
                Content = (Get-Content -LiteralPath $file.FullName -Raw)
                Hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        }
    }
    Remove-LegacyLicenseLayout $Payload

    # Universal packages carry Helpy inside the RuneSchema payload. Keep one
    # authoritative document instead of duplicating it inside that plugin.
    $nestedHelpy = Join-Path $Payload 'plugins/RuneSchema.Helpy'
    if (Test-Path -LiteralPath $nestedHelpy -PathType Container) {
        Remove-LegacyLicenseLayout $nestedHelpy
    }
    Remove-Item -LiteralPath $destination -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    $target = Join-Path $destination 'RuneSchema-Licenses.txt'
    Copy-Item -LiteralPath (Join-Path $bundle 'RuneSchema-Licenses.txt') -Destination $target -Force
    if ($existingAppendix) {
        Add-Content -LiteralPath $target -Value @('', $existingAppendix) -Encoding utf8
    }
    foreach ($notice in @($inherited | Sort-Object Hash -Unique)) {
        Add-Content -LiteralPath $target -Value @(
            '',
            ('=' * 78),
            $notice.Name,
            "SHA-256: $($notice.Hash)",
            ('=' * 78),
            '',
            $notice.Content
        ) -Encoding utf8
    }
}

function Write-LicensedArchive([string]$Payload, [string]$Archive, [bool]$VersionedWrapper = $false) {
    if (-not (Test-Path -LiteralPath $Payload -PathType Container)) {
        throw "Package payload does not exist: $Payload"
    }
    Write-ConsolidatedLicenseFile
    Install-LicenseBundle $Payload
    if ($Archive) {
        $Archive = [IO.Path]::GetFullPath($Archive)
        New-Item -ItemType Directory -Path (Split-Path -Parent $Archive) -Force | Out-Null
        $temporary = $Archive + '.' + [guid]::NewGuid().ToString('N') + '.tmp.zip'
        try {
            # Release archives retain their versioned package folder, matching
            # the downloadable layout: Package-Version/RuneSchema/...
            $archiveInput = if ($VersionedWrapper) { Split-Path -Parent $Payload } else { $Payload }
            Compress-Archive -LiteralPath $archiveInput -DestinationPath $temporary -CompressionLevel Optimal
            Move-Item -LiteralPath $temporary -Destination $Archive -Force
        } finally {
            Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
        }
        Write-Host "Packaged license notices: $Archive" -ForegroundColor Green
    }
}

function Get-DependencyNoticeName([string]$Relative, [string]$Hash) {
    $path = $Relative.Replace('\','/').ToLowerInvariant()
    if ($path -match '(^|/)tools/upx/copying$') { return 'UPX-GPL-2.0.txt' }
    if ($path -match '(^|/)tools/upx/license$') { return 'UPX-License-and-Compression-Exception.txt' }
    if ($path -match 'fonts/droid|license_droid') { return 'Droid-Font-Apache-2.0.txt' }
    if ($path -match 'license_roboto') { return 'Roboto-Font-OFL-1.1.txt' }
    if ($path -match 'usmapgenerator/license$') { return 'UE4SS-USMapGenerator-MIT.txt' }
    if ($path -match '/uvtd/license$') { return 'UE4SS-UVTD-MIT.txt' }
    if ($path -match 'ue4ss-source/(license|deps/first/.+/license)$') {
        return 'UE4SS-and-First-Party-Dependencies-MIT.txt'
    }
    $parts = @($Relative.Replace('\','/').Split('/') | Where-Object { $_ })
    $component = if ($parts.Count -gt 1) { $parts[$parts.Count - 2] } else { 'Dependency' }
    $leaf = if ($parts.Count) { [IO.Path]::GetFileNameWithoutExtension($parts[-1]) } else { 'Notice' }
    $label = ($component + '-' + $leaf) -replace '[^0-9A-Za-z._-]', '-'
    return $label + '-' + $Hash.Substring(0,8) + '.txt'
}

function Get-NoticeUpstream([string]$NoticeFile) {
    if ($NoticeFile -like 'UPX-*') { return 'https://github.com/upx/upx' }
    if ($NoticeFile -like 'Droid-Font-*') { return 'https://source.android.com/' }
    if ($NoticeFile -like 'Roboto-Font-*') { return 'https://github.com/googlefonts/roboto-classic' }
    if ($NoticeFile -like 'UE4SS-*') { return 'https://github.com/UE4SS-RE/RE-UE4SS' }
    return ''
}

function Write-ConsolidatedLicenseFile {
    $lines = [Collections.Generic.List[string]]::new()
    foreach ($line in @(
        'RUNESCHEMA CONSOLIDATED LICENSES, CREDITS, AND THIRD-PARTY NOTICES',
        '',
        'This is the only license document shipped with this RuneSchema package.',
        'It contains the complete local text for every project and dependency notice collected by the build.',
        '',
        'Primary upstream links:',
        'RuneSchema: https://github.com/gh0sted5456-us/RuneSchema',
        'PalSchema: https://github.com/Okaetsu/PalSchema',
        'UE4SS / RE-UE4SS: https://github.com/UE4SS-RE/RE-UE4SS',
        'JSON for Modern C++: https://github.com/nlohmann/json',
        'UPX: https://github.com/upx/upx',
        'Roboto: https://github.com/googlefonts/roboto-classic',
        'Android / Droid fonts: https://source.android.com/'
    )) { $lines.Add($line) }

    $sections = @(
        @{ Name = 'RuneSchema — MIT License'; File = 'RuneSchema-MIT.txt'; Upstream = 'https://github.com/gh0sted5456-us/RuneSchema' },
        @{ Name = 'RuneSchema — Licensing Scope and Ownership'; File = 'LICENSING.md'; Upstream = 'https://github.com/gh0sted5456-us/RuneSchema' },
        @{ Name = 'RuneSchema — Authors and Credits'; File = 'AUTHORS.md'; Upstream = 'https://github.com/gh0sted5456-us/RuneSchema' },
        @{ Name = 'RuneSchema — Contributor Terms'; File = 'CONTRIBUTING.md'; Upstream = 'https://github.com/gh0sted5456-us/RuneSchema' },
        @{ Name = 'RuneSchema — Third-Party Inventory'; File = 'THIRD_PARTY_NOTICES.md'; Upstream = 'https://github.com/gh0sted5456-us/RuneSchema' },
        @{ Name = 'PalSchema — MIT License'; File = 'PalSchema-MIT.txt'; Upstream = 'https://github.com/Okaetsu/PalSchema' },
        @{ Name = 'UE4SS / RE-UE4SS — MIT License'; File = 'UE4SS-MIT.txt'; Upstream = 'https://github.com/UE4SS-RE/RE-UE4SS' },
        @{ Name = 'JSON for Modern C++ — MIT License'; File = 'nlohmann-json-MIT.txt'; Upstream = 'https://github.com/nlohmann/json' }
    )
    foreach ($section in $sections) {
        $lines.Add('')
        $lines.Add(('=' * 78))
        $lines.Add($section.Name)
        $lines.Add('Upstream: ' + $section.Upstream)
        $lines.Add('Source document: ' + $section.File)
        $lines.Add(('=' * 78))
        $lines.Add('')
        $lines.Add((Get-Content -LiteralPath (Join-Path $bundle $section.File) -Raw))
    }

    $manifestPath = Join-Path $bundle 'dependencies/manifest.json'
    if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        foreach ($group in @($manifest.Files | Group-Object NoticeFile | Sort-Object Name)) {
            $notice = [string]$group.Name
            $label = [IO.Path]::GetFileNameWithoutExtension($notice).Replace('-',' ')
            $upstream = Get-NoticeUpstream $notice
            $lines.Add('')
            $lines.Add(('=' * 78))
            $lines.Add($label)
            if ($upstream) { $lines.Add('Upstream: ' + $upstream) }
            $lines.Add('Collected source records:')
            foreach ($record in $group.Group) {
                $lines.Add('  - ' + $record.Source + ' (SHA-256 ' + $record.Sha256 + ')')
            }
            $lines.Add(('=' * 78))
            $lines.Add('')
            $lines.Add((Get-Content -LiteralPath (Join-Path $bundle ('dependencies/' + $notice)) -Raw))
        }
    }
    $encoding = [Text.UTF8Encoding]::new($false)
    [IO.File]::WriteAllText(
        (Join-Path $bundle 'RuneSchema-Licenses.txt'),
        (($lines -join [Environment]::NewLine) + [Environment]::NewLine),
        $encoding)
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
    $destination = Join-Path $bundle 'dependencies'
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
            $noticeName = Get-DependencyNoticeName $relative $hash
            Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $destination $noticeName) -Force
            $records += [pscustomobject]@{
                Source = ($root.Name + '/' + $relative)
                Sha256 = $hash
                NoticeFile = $noticeName
            }
        }
    }
    [ordered]@{
        Schema = 2
        Scope = 'Notices found in local dependency/build caches; not a complete license audit.'
        Files = @($records)
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $destination 'manifest.json') -Encoding utf8
}

try {
    New-Item -ItemType Directory -Path $bundle -Force | Out-Null
    foreach ($name in @('LICENSE', 'AUTHORS.md', 'LICENSING.md', 'CONTRIBUTING.md', 'THIRD_PARTY_NOTICES.md')) {
        $destinationName = if ($name -eq 'LICENSE') { 'RuneSchema-MIT.txt' } else { $name }
        Copy-Item -LiteralPath (Join-Path $RepositoryRoot $name) -Destination (Join-Path $bundle $destinationName) -Force
    }
    foreach ($notice in Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'licenses') -File) {
        Copy-Item -LiteralPath $notice.FullName -Destination $bundle -Force
    }
    if ($FinalizeBuild) {
        $buildScript = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'build/build.ps1') -Raw
        $versionMatch = [regex]::Match($buildScript, '(?m)^\$Version\s*=\s*''([^'']+)''')
        if (-not $versionMatch.Success -or $versionMatch.Groups[1].Value -notmatch '^\d+\.\d+\.\d+\.\d+[em]$') {
            throw 'Could not determine the current package version from build/build.ps1.'
        }
        $version = $versionMatch.Groups[1].Value
        $releaseBranch = $env:GITHUB_REF_NAME
        if ([string]::IsNullOrWhiteSpace($releaseBranch) -and (Test-Path -LiteralPath (Join-Path $RepositoryRoot '.git'))) {
            $branchOutput = & git.exe -C $RepositoryRoot branch --show-current 2>$null
            $releaseBranch = if ($branchOutput) { ([string]$branchOutput).Trim() } else { '' }
        }
        if ($releaseBranch -eq 'main' -and -not $version.EndsWith('m')) {
            throw "Refusing to finalize a main package without a lowercase 'm' suffix: $version"
        }
        if ($releaseBranch -eq 'experimental' -and -not $version.EndsWith('e')) {
            throw "Refusing to finalize an experimental package without a lowercase 'e' suffix: $version"
        }
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
            Write-LicensedArchive $payload $archive $true
            $count++
        }
        if ($count -eq 0) { throw 'No current Core, Universal, or Helpy build archive was found to finalize.' }
        # Accompany the loose developer DLL and convenience install trees too.
        Install-LicenseBundle $dist
        foreach ($relative in @('plugins/Universal', 'plugins/RuneSchema.Helpy')) {
            $path = Join-Path $RepositoryRoot $relative
            if (Test-Path -LiteralPath $path -PathType Container) { Install-LicenseBundle $path }
        }
    } else {
        Collect-DependencyNotices
        Write-LicensedArchive ([IO.Path]::GetFullPath($PayloadRoot)) $ArchivePath
    }
} finally {
    Remove-Item -LiteralPath $bundle -Recurse -Force -ErrorAction SilentlyContinue
}
