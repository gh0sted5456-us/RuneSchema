[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$PluginOnly,
    [switch]$Tests,
    [switch]$UpdateMappings
)
$ErrorActionPreference = 'Stop'
$Version = '0.7.7.1'
$BuildRoot = [IO.Path]::GetFullPath($PSScriptRoot)
if (-not (Test-Path -LiteralPath (Join-Path $BuildRoot 'source\raw\CMakeLists.txt') -PathType Leaf)) {
    $BuildRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
}
$SourceRoot = Join-Path $BuildRoot 'source'
$RawSource = Join-Path $SourceRoot 'raw'
$DependencyCache = Join-Path $BuildRoot '.cache\dependencies'
$CleanBase = Join-Path $DependencyCache 'runtime-template'
$BuildDependencyArchive = Join-Path $DependencyCache 'RuneSchema-BuildDependencies-experimental.zip'
$BuildDependencyUrl = 'https://github.com/gh0sted5456-us/RuneSchema/releases/download/experimental-build-deps/RuneSchema-BuildDependencies-experimental.zip'
$BuildDependencySha256 = '11f5eba4403c24b8085976176af0a20e0f298468e9c52fabaaa99349818cbd2c'
$UE4SSSource = Join-Path $DependencyCache 'ue4ss-source'
$UE4SSRepository = 'https://github.com/UE4SS-RE/RE-UE4SS.git'
$UEPseudoRepository = 'https://github.com/Re-UE4SS/UEPseudo.git'
# Keep only generated CMake/Ninja state on a short path. The successful
# e7e8a32 dependency/configure behavior remains otherwise unchanged.
$BuildCacheSeed = [Text.Encoding]::UTF8.GetBytes($BuildRoot.ToLowerInvariant())
$BuildCacheHasher = [Security.Cryptography.SHA256]::Create()
try {
    $BuildCacheDigest = (($BuildCacheHasher.ComputeHash($BuildCacheSeed) | ForEach-Object { $_.ToString('x2') }) -join '')
} finally {
    $BuildCacheHasher.Dispose()
}
if ($env:RUNESCHEMA_BUILD_CACHE) {
    $BuildCache = [IO.Path]::GetFullPath($env:RUNESCHEMA_BUILD_CACHE)
} else {
    $BuildCache = Join-Path ([IO.Path]::GetTempPath()) ("RSB-" + $BuildCacheDigest.Substring(0,12))
}
$DistRoot = Join-Path $BuildRoot 'dist'
$LogRoot = Join-Path $PSScriptRoot 'logs'
$Upx = Join-Path $DependencyCache 'tools\upx\upx.exe'
$Configuration = 'Game__Shipping__Win64'
$MappingsRepository = 'RSDWArchive/RSDWArchive'
$MappingsRef = 'main'
$MappingsLock = Join-Path $PSScriptRoot 'mappings.lock.json'
$MappingsCache = Join-Path $DependencyCache 'mappings'
$MaximumMappingsBytes = 16MB

$OwnTranscript = -not [bool]$env:RUNESCHEMA_OUTER_TRANSCRIPT
if ($OwnTranscript) {
    New-Item -ItemType Directory -Path $LogRoot -Force | Out-Null
    $log = Join-Path $LogRoot ("build-{0:yyyyMMdd-HHmmss}.log" -f (Get-Date))
    Start-Transcript -LiteralPath $log | Out-Null
}
try {
    function Find-Exe([string]$Name, [string[]]$Hints = @()) {
        $cmd = Get-Command $Name -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
        foreach ($hint in $Hints) { if (Test-Path -LiteralPath $hint -PathType Leaf) { return $hint } }
        return $null
    }
    function Get-Sha256Hex([string]$Path) {
        $stream = [IO.File]::OpenRead($Path)
        $sha = [Security.Cryptography.SHA256]::Create()
        try {
            return (($sha.ComputeHash($stream) | ForEach-Object { $_.ToString('x2') }) -join '')
        } finally {
            $sha.Dispose()
            $stream.Dispose()
        }
    }
    function Assert-Sha256([string]$Path, [string]$Expected) {
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
        $actual = Get-Sha256Hex $Path
        if ($actual -ne $Expected.ToLowerInvariant()) {
            Write-Warning "Cached dependency hash mismatch; discarding $Path"
            Remove-Item -LiteralPath $Path -Force
            return $false
        }
        return $true
    }
    function Get-GitHubHeaders {
        $headers = @{ 'User-Agent' = 'RuneSchema-Builder' }
        if ($env:GITHUB_TOKEN) { $headers.Authorization = "Bearer $($env:GITHUB_TOKEN)" }
        return $headers
    }
    function Update-MappingsLock {
        Write-Host 'Discovering the newest RSDWArchive USMAP...' -ForegroundColor Cyan
        $headers = Get-GitHubHeaders
        $api = "https://api.github.com/repos/$MappingsRepository"
        $commit = Invoke-RestMethod -Uri "$api/commits/$MappingsRef" -Headers $headers
        $commitSha = [string]$commit.sha
        if ($commitSha -notmatch '^[0-9a-f]{40}$') { throw 'RSDWArchive returned an invalid commit SHA.' }

        # Windows PowerShell 5.1 emits a JSON top-level array as one Object[];
        # do not wrap it again or the directory entries become a nested array.
        $root = Invoke-RestMethod -Uri "$api/contents?ref=$commitSha" -Headers $headers
        $versions = @($root | Where-Object {
            $_.type -eq 'dir' -and $_.name -match '^\d+\.\d+\.\d+\.\d+$'
        } | ForEach-Object {
            [pscustomobject]@{ Name = [string]$_.name; Version = [version]($_.name) }
        } | Sort-Object Version -Descending)
        if (-not $versions) { throw 'RSDWArchive does not contain a versioned mapping directory.' }

        $version = $versions[0].Name
        $entries = Invoke-RestMethod -Uri "$api/contents/$version/usmap`?ref=$commitSha" -Headers $headers
        $maps = @($entries | Where-Object { $_.type -eq 'file' -and $_.name -match '\.usmap$' })
        if ($maps.Count -ne 1) {
            throw "Expected exactly one USMAP in RSDWArchive $version/usmap; found $($maps.Count)."
        }
        $map = $maps[0]
        $declaredSize = [int64]$map.size
        if ($declaredSize -le 0 -or $declaredSize -gt $MaximumMappingsBytes) {
            throw "RSDWArchive USMAP size $declaredSize is outside the allowed range."
        }

        New-Item -ItemType Directory -Path $MappingsCache -Force | Out-Null
        $temporary = Join-Path $MappingsCache 'Mappings.usmap.download'
        Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
        Invoke-WebRequest -Uri $map.download_url -Headers $headers -OutFile $temporary
        $downloadedSize = (Get-Item -LiteralPath $temporary).Length
        if ($downloadedSize -ne $declaredSize) {
            Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
            throw "RSDWArchive USMAP size changed during download ($downloadedSize instead of $declaredSize)."
        }
        $sha256 = Get-Sha256Hex $temporary
        $cacheFile = Join-Path $MappingsCache "$sha256.usmap"
        Move-Item -LiteralPath $temporary -Destination $cacheFile -Force

        $lock = [ordered]@{
            Schema = 1
            Repository = $MappingsRepository
            Ref = $MappingsRef
            Commit = $commitSha
            Version = $version
            File = [string]$map.name
            GitBlobSha = [string]$map.sha
            Size = $declaredSize
            Sha256 = $sha256
            DownloadUrl = [string]$map.download_url
        }
        $lock | ConvertTo-Json | Set-Content -LiteralPath $MappingsLock -Encoding utf8
        Write-Host "Locked RSDWArchive $version/$($map.name) at $commitSha." -ForegroundColor Green
        return [pscustomobject]$lock
    }
    function Ensure-Mappings {
        $lock = if ($UpdateMappings) {
            Update-MappingsLock
        } else {
            if (-not (Test-Path -LiteralPath $MappingsLock -PathType Leaf)) {
                throw "Mappings lock is missing: $MappingsLock. Run Build RuneSchema.bat to create it."
            }
            Get-Content -LiteralPath $MappingsLock -Raw | ConvertFrom-Json
        }
        if ($lock.Repository -ne $MappingsRepository -or $lock.Sha256 -notmatch '^[0-9a-fA-F]{64}$' -or
            [int64]$lock.Size -le 0 -or [int64]$lock.Size -gt $MaximumMappingsBytes) {
            throw "Mappings lock is invalid: $MappingsLock"
        }

        New-Item -ItemType Directory -Path $MappingsCache -Force | Out-Null
        $cacheFile = Join-Path $MappingsCache "$($lock.Sha256.ToLowerInvariant()).usmap"
        if (-not (Assert-Sha256 $cacheFile $lock.Sha256)) {
            $temporary = "$cacheFile.download"
            Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
            Write-Host "Downloading locked RSDWArchive USMAP $($lock.Version)..." -ForegroundColor Cyan
            Invoke-WebRequest -Uri $lock.DownloadUrl -Headers (Get-GitHubHeaders) -OutFile $temporary
            if ((Get-Item -LiteralPath $temporary).Length -ne [int64]$lock.Size -or
                -not (Assert-Sha256 $temporary $lock.Sha256)) {
                Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
                throw 'Downloaded RSDWArchive USMAP failed size or SHA-256 verification.'
            }
            Move-Item -LiteralPath $temporary -Destination $cacheFile -Force
        }
        [pscustomobject]@{ Path = $cacheFile; Lock = $lock }
    }
    function Ensure-BuildDependencies {
        if ((Test-Path -LiteralPath $CleanBase -PathType Container) -and
            (Test-Path -LiteralPath $Upx -PathType Leaf)) {
            return
        }

        New-Item -ItemType Directory -Path $DependencyCache -Force | Out-Null
        if (-not (Assert-Sha256 $BuildDependencyArchive $BuildDependencySha256)) {
            $temporary = "$BuildDependencyArchive.download"
            Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
            Write-Host 'Downloading RuneSchema build dependencies...' -ForegroundColor Cyan
            Invoke-WebRequest -Uri $BuildDependencyUrl -OutFile $temporary
            $actual = Get-Sha256Hex $temporary
            if ($actual -ne $BuildDependencySha256.ToLowerInvariant()) {
                Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
                throw 'Downloaded RuneSchema build dependency package failed SHA-256 verification.'
            }
            Move-Item -LiteralPath $temporary -Destination $BuildDependencyArchive -Force
        }

        Remove-Item -LiteralPath $CleanBase -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath (Join-Path $DependencyCache 'tools') -Recurse -Force -ErrorAction SilentlyContinue
        Expand-Archive -LiteralPath $BuildDependencyArchive -DestinationPath $DependencyCache -Force

        if (-not (Test-Path -LiteralPath $CleanBase -PathType Container) -or
            -not (Test-Path -LiteralPath $Upx -PathType Leaf)) {
            throw 'RuneSchema build dependency package did not contain the expected runtime-template/tools layout.'
        }
        Write-Host "Build dependencies ready in $DependencyCache" -ForegroundColor Green
    }
    function Initialize-MsvcEnvironment {
        if ($env:VCToolsInstallDir -and $env:WindowsSdkDir -and (Get-Command cl.exe -ErrorAction SilentlyContinue)) { return }
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) { throw 'Visual Studio locator (vswhere.exe) was not found.' }
        $installation = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
        if (-not $installation) { throw 'Visual Studio C++ build tools were not found.' }
        $vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
        if (-not (Test-Path -LiteralPath $vcvars -PathType Leaf)) { throw "Missing Visual Studio environment script: $vcvars" }
        $environment = & $env:ComSpec /d /s /c "`"$vcvars`" >nul && set"
        if ($LASTEXITCODE) { throw 'Visual Studio x64 environment initialization failed.' }
        $pathValues = @()
        foreach ($line in $environment) {
            $separator = $line.IndexOf('=')
            if ($separator -le 0) { continue }
            $key = $line.Substring(0,$separator)
            $value = $line.Substring($separator+1)
            if ($key -ieq 'Path') { $pathValues += $value; continue }
            Set-Item -LiteralPath "Env:$key" -Value $value
        }
        # Prefer the vcvars PATH containing the selected compiler.
        $compilerPath = $pathValues | Where-Object { $_ -match 'VC\\Tools\\MSVC\\.+\\bin\\Hostx64\\x64' } | Select-Object -First 1
        if (-not $compilerPath) { $compilerPath = $pathValues | Sort-Object Length -Descending | Select-Object -First 1 }
        if ($compilerPath) { $env:PATH = $compilerPath }
    }
    function Invoke-Checked([string]$Exe, [string[]]$Arguments, [string]$What) {
        & $Exe @Arguments
        if ($LASTEXITCODE) { throw "$What failed (exit $LASTEXITCODE)." }
    }
    function Get-ConfigureFingerprint([string]$SourceDirectory, [string[]]$ConfigureArguments = @()) {
        $sha = [Security.Cryptography.SHA256]::Create()
        try {
            $builder = [Text.StringBuilder]::new()

            # CMake content controls the generated build graph.
            Get-ChildItem -LiteralPath $SourceDirectory -Filter 'CMakeLists.txt' -File -Recurse |
                Sort-Object FullName |
                ForEach-Object {
                    [void]$builder.AppendLine($_.FullName.Substring($SourceDirectory.Length).Replace('\','/'))
                    [void]$builder.AppendLine((Get-Content -LiteralPath $_.FullName -Raw))
                }

            # Adding/removing/renaming a .cpp must force configure for source
            # trees that discover files by glob. Smaller subprojects such as
            # source\raw\core do not have their own src directory; their
            # CMakeLists explicitly names sources, so the CMake file hash is
            # sufficient to detect graph changes.
            $sourceCppRoot = Join-Path $SourceDirectory 'src'
            if (Test-Path -LiteralPath $sourceCppRoot -PathType Container) {
                Get-ChildItem -LiteralPath $sourceCppRoot -Filter '*.cpp' -File -Recurse |
                    Sort-Object FullName |
                    ForEach-Object {
                        [void]$builder.AppendLine($_.FullName.Substring($SourceDirectory.Length).Replace('\','/'))
                    }
            }

            [void]$builder.AppendLine('--- configure arguments ---')
            foreach ($argument in $ConfigureArguments) {
                [void]$builder.AppendLine($argument)
            }

            $bytes = [Text.Encoding]::UTF8.GetBytes($builder.ToString())
            $hashBytes = $sha.ComputeHash($bytes)
            return (($hashBytes | ForEach-Object { $_.ToString('x2') }) -join '')
        } finally {
            $sha.Dispose()
        }
    }
    function Ensure-CMakeConfigured(
        [string]$SourceDirectory,
        [string]$BuildDirectory,
        [string[]]$ConfigureArguments,
        [string]$Label
    ) {
        $stamp = Join-Path $BuildDirectory '.runeschema-configure.sha256'
        $ninjaFile = Join-Path $BuildDirectory 'build.ninja'
        $fingerprint = Get-ConfigureFingerprint $SourceDirectory $ConfigureArguments
        $prior = if (Test-Path -LiteralPath $stamp -PathType Leaf) {
            (Get-Content -LiteralPath $stamp -Raw).Trim()
        } else { '' }

        if ((Test-Path -LiteralPath $ninjaFile -PathType Leaf) -and $prior -eq $fingerprint) {
            Write-Host "=== Reuse configured $Label ===" -ForegroundColor DarkCyan
            return
        }

        New-Item -ItemType Directory -Path $BuildDirectory -Force | Out-Null
        Write-Host "=== Configure $Label ===" -ForegroundColor Cyan
        Invoke-Checked 'cmake.exe' $ConfigureArguments "CMake configure for $Label"
        Set-Content -LiteralPath $stamp -Value $fingerprint -Encoding ascii
    }
    function Remove-SafeTree([string]$Path) {
        $resolved = [IO.Path]::GetFullPath($Path)
        $allowedRoots = @($BuildRoot, $BuildCache)
        $safe = $false
        foreach ($candidate in $allowedRoots) {
            $root = [IO.Path]::GetFullPath($candidate).TrimEnd('\')
            if ($resolved.Equals($root, [StringComparison]::OrdinalIgnoreCase) -or
                $resolved.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) {
                $safe = $true
                break
            }
        }
        if (-not $safe) {
            throw "Refusing unsafe clean path: $resolved"
        }
        if (-not (Test-Path -LiteralPath $resolved)) { return }
        # Build intermediates can exceed the legacy Win32 path limit.
        $extended = if ($resolved.StartsWith('\\')) { '\\?\UNC\' + $resolved.Substring(2) } else { '\\?\' + $resolved }
        $tree = [IO.DirectoryInfo]::new($extended)
        foreach ($file in $tree.EnumerateFiles('*',[IO.SearchOption]::AllDirectories)) {
            try { $file.Attributes = [IO.FileAttributes]::Normal } catch {}
        }
        foreach ($directory in $tree.EnumerateDirectories('*',[IO.SearchOption]::AllDirectories)) {
            try { $directory.Attributes = [IO.FileAttributes]::Directory } catch {}
        }
        $tree.Attributes = [IO.FileAttributes]::Directory
        [IO.Directory]::Delete($extended, $true)
    }
    function Initialize-GitHubTransport {
        # UE4SS itself is public, but its UEPseudo submodule is private and
        # requires the builder's GitHub account to have accepted the Epic Games
        # organization invitation. Rewrite UE4SS's SSH submodule URLs to HTTPS
        # so normal Git Credential Manager authentication works on Windows.
        if ($env:GITHUB_ACTIONS -eq 'true') {
            $env:GIT_TERMINAL_PROMPT = '0'
            $env:GCM_INTERACTIVE = 'Never'
        } else {
            Remove-Item Env:GIT_TERMINAL_PROMPT -ErrorAction SilentlyContinue
            Remove-Item Env:GCM_INTERACTIVE -ErrorAction SilentlyContinue
        }
        $env:GIT_CONFIG_COUNT = '3'
        $env:GIT_CONFIG_KEY_0 = 'url.https://github.com/.insteadOf'
        $env:GIT_CONFIG_VALUE_0 = 'git@github.com:'
        $env:GIT_CONFIG_KEY_1 = 'url.https://github.com/.insteadOf'
        $env:GIT_CONFIG_VALUE_1 = 'ssh://git@github.com/'
        $env:GIT_CONFIG_KEY_2 = 'core.longpaths'
        $env:GIT_CONFIG_VALUE_2 = 'true'
        Invoke-Checked 'git.exe' @('ls-remote', '--exit-code', $UE4SSRepository, 'HEAD') 'GitHub connectivity check'
    }
    function Get-UE4SSPinnedCommit {
        $cmakeFile = Join-Path $RawSource 'CMakeLists.txt'
        $text = Get-Content -LiteralPath $cmakeFile -Raw
        $match = [regex]::Match($text, 'set\(RUNESCHEMA_UE4SS_TAG\s+"([0-9a-fA-F]{40})"')
        if (-not $match.Success) {
            throw 'Could not read RUNESCHEMA_UE4SS_TAG from source\raw\CMakeLists.txt.'
        }
        return $match.Groups[1].Value.ToLowerInvariant()
    }
    function Assert-UEPseudoAccess {
        & git.exe ls-remote --exit-code $UEPseudoRepository HEAD *> $null
        if ($LASTEXITCODE -eq 0) { return }

        $message = @'
RuneSchema cannot access UE4SS's private UEPseudo dependency.

UE4SS requires source builders to:
  1. Connect the same GitHub account to an Epic Games account.
  2. Accept the Epic Games organization invitation on GitHub.
  3. Sign in to GitHub through Git Credential Manager (HTTPS).

After linking Epic/GitHub, check:
  https://github.com/settings/organizations

Then run Build RuneSchema.bat again.

For CI, the workflow token must separately have access to UEPseudo; the normal
GITHUB_TOKEN for the RuneSchema repository does not grant that private access.
'@
        throw $message.Trim()
    }
    function Ensure-UE4SSSource {
        $pin = Get-UE4SSPinnedCommit
        $gitDirectory = Join-Path $UE4SSSource '.git'

        if (-not (Test-Path -LiteralPath $gitDirectory -PathType Container)) {
            if (Test-Path -LiteralPath $UE4SSSource) {
                Remove-SafeTree $UE4SSSource
            }
            New-Item -ItemType Directory -Path $DependencyCache -Force | Out-Null
            Write-Host "Preparing pinned UE4SS source ($pin)..." -ForegroundColor Cyan
            Invoke-Checked 'git.exe' @('clone', '--filter=blob:none', '--no-checkout', $UE4SSRepository, $UE4SSSource) 'UE4SS source clone'
        }

        & git.exe -C $UE4SSSource cat-file -e "$pin^{commit}" 2>$null
        if ($LASTEXITCODE -ne 0) {
            Invoke-Checked 'git.exe' @('-C', $UE4SSSource, 'fetch', '--no-tags', 'origin', $pin) 'UE4SS pinned commit fetch'
        }
        Invoke-Checked 'git.exe' @('-C', $UE4SSSource, 'checkout', '--detach', '--force', $pin) 'UE4SS pinned checkout'
        Invoke-Checked 'git.exe' @('-C', $UE4SSSource, 'submodule', 'sync', '--recursive') 'UE4SS submodule sync'

        Assert-UEPseudoAccess
        Write-Host 'Initializing UE4SS private/public submodules with the authenticated Git session...' -ForegroundColor Cyan
        Invoke-Checked 'git.exe' @('-C', $UE4SSSource, 'submodule', 'update', '--init', '--recursive') 'UE4SS submodule initialization'

        $pseudo = Join-Path $UE4SSSource 'deps\first\Unreal'
        $patterns = Join-Path $UE4SSSource 'deps\first\patternsleuth'
        if (-not (Test-Path -LiteralPath (Join-Path $pseudo '.git')) -and
            -not (Test-Path -LiteralPath (Join-Path $pseudo 'CMakeLists.txt') -PathType Leaf)) {
            throw 'UE4SS UEPseudo submodule did not initialize correctly.'
        }
        if (-not (Test-Path -LiteralPath $patterns -PathType Container)) {
            throw 'UE4SS patternsleuth submodule did not initialize correctly.'
        }
        Write-Host "UE4SS source ready: $UE4SSSource" -ForegroundColor Green
        return $pin
    }
    function Compress-DllBestEffort([string]$Dll) {
        $backup = "$Dll.uncompressed"
        Copy-Item -LiteralPath $Dll -Destination $backup -Force
        try {
            & $Upx -9 --best --lzma $Dll
            if ($LASTEXITCODE) { throw "UPX declined the DLL (exit $LASTEXITCODE)" }
            & $Upx -t $Dll
            if ($LASTEXITCODE) { throw "UPX verification failed (exit $LASTEXITCODE)" }
            return 'UPX -9 --best --lzma; verified'
        } catch {
            Write-Warning "Compression skipped for $Dll; keeping original: $_"
            Copy-Item -LiteralPath $backup -Destination $Dll -Force
            return "Uncompressed; $($_.Exception.Message)"
        } finally {
            Remove-Item -LiteralPath $backup -Force -ErrorAction SilentlyContinue
        }
    }
    function Find-SignTool {
        $found = Get-Command signtool.exe -ErrorAction SilentlyContinue
        if ($found) { return $found.Source }
        $kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
        if (Test-Path $kits) {
            return Get-ChildItem -LiteralPath $kits -Filter signtool.exe -Recurse -File -ErrorAction SilentlyContinue |
                Where-Object FullName -Match '\\x64\\signtool.exe$' | Sort-Object FullName -Descending |
                Select-Object -First 1 -ExpandProperty FullName
        }
        return $null
    }
    function Attempt-Signing([string[]]$Files) {
        # Optional signer downloads require a pinned SHA-256 hash.
        $signer = $null
        if ($env:RUNESCHEMA_SIGNER_URL -and $env:RUNESCHEMA_SIGNER_SHA256) {
            if ($env:RUNESCHEMA_SIGNER_URL -notmatch '^https://github\.com/') {
                Write-Warning 'Optional signer URL must be an HTTPS GitHub release URL; ignoring it.'
            } else {
                try {
                    $download = Join-Path $PSScriptRoot 'signer\signer.exe'
                    New-Item -ItemType Directory -Path (Split-Path $download) -Force | Out-Null
                    Invoke-WebRequest -Uri $env:RUNESCHEMA_SIGNER_URL -OutFile $download
                    $actual = Get-Sha256Hex $download
                    if ($actual -ne $env:RUNESCHEMA_SIGNER_SHA256.ToLowerInvariant()) { throw 'Downloaded signer SHA-256 did not match the configured pin.' }
                    $signer = $download
                    Write-Host 'Pinned GitHub signer downloaded and verified.'
                } catch { Write-Warning "Could not obtain configured GitHub signer; continuing unsigned: $_" }
            }
        }
        if (-not $signer) { $signer = Find-SignTool }
        $hasPfx = [bool]$env:RUNESCHEMA_SIGN_PFX
        $hasThumbprint = [bool]$env:RUNESCHEMA_SIGN_CERT_THUMBPRINT
        if (-not $signer) { Write-Warning 'No signing utility is installed/configured; continuing unsigned.'; return }
        if (-not $hasPfx -and -not $hasThumbprint) { Write-Warning 'No code-signing certificate configured; signing skipped and build continues.'; return }
        foreach ($file in $Files) {
            try {
                $args = @('sign', '/fd', 'SHA256')
                if ($env:RUNESCHEMA_SIGN_TIMESTAMP) { $args += @('/tr', $env:RUNESCHEMA_SIGN_TIMESTAMP, '/td', 'SHA256') }
                if ($hasPfx) {
                    $args += @('/f', $env:RUNESCHEMA_SIGN_PFX)
                    if ($env:RUNESCHEMA_SIGN_PFX_PASSWORD) { $args += @('/p', $env:RUNESCHEMA_SIGN_PFX_PASSWORD) }
                } else { $args += @('/sha1', $env:RUNESCHEMA_SIGN_CERT_THUMBPRINT) }
                $args += $file
                & $signer @args
                if ($LASTEXITCODE) { Write-Warning "Signing failed for $file (exit $LASTEXITCODE); retaining the build." }
            } catch { Write-Warning "Signing attempt failed for $file; retaining the build: $_" }
        }
    }
    function Invoke-UniversalBuild([switch]$OnlyPlugin) {
        $build = Join-Path $BuildCache 'universal'
        $generator = 'Ninja'
        $ninjaHints = @()
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path $vswhere) {
            $vs = & $vswhere -latest -products '*' -property installationPath | Select-Object -First 1
            if ($vs) { $ninjaHints += Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe' }
        }
        $ninja = Find-Exe 'ninja.exe' $ninjaHints
        if (-not $ninja) { throw 'Ninja was not found (Visual Studio C++ CMake tools include it).'}
        $env:PATH = "$(Split-Path $ninja);$env:PATH"
        if (-not (Test-Path -LiteralPath $UE4SSSource -PathType Container)) {
            throw 'Prepared UE4SS source is missing; Ensure-UE4SSSource must run before CMake configure.'
        }
        Write-Host "Short generated build cache: $BuildCache" -ForegroundColor DarkCyan
        $configureArgs = @(
            '-S', $RawSource,
            '-B', $build,
            '-G', $generator,
            "-DCMAKE_BUILD_TYPE=$Configuration",
            '-DFETCHCONTENT_FULLY_DISCONNECTED=OFF',
            '-DFETCHCONTENT_UPDATES_DISCONNECTED=ON',
            "-DFETCHCONTENT_SOURCE_DIR_UE4SS=$UE4SSSource",
            '-DCMAKE_SUPPRESS_REGENERATION=ON',
            '-Wno-author',
            '-Wno-deprecated'
        )
        Ensure-CMakeConfigured $RawSource $build $configureArgs 'universal RuneSchema'
        $targets = if ($OnlyPlugin) { @('RuneSchemaHelpyPlugin') } else { @('RuneSchema', 'RuneSchemaHelpyPlugin') }
        Write-Host $(if ($OnlyPlugin) { '=== Compile Helpy plugin only (RuneSchema.dll is untouched) ===' } else { '=== Compile universal RuneSchema ===' }) -ForegroundColor Cyan
        Write-Host 'Ninja auto-regeneration is disabled; build.ps1 owns all CMake reconfiguration.' -ForegroundColor DarkGray

        function Invoke-NinjaCaptured([string[]]$Arguments, [string]$Label) {
            $stdout = Join-Path $LogRoot 'ninja-stdout.log'
            $stderr = Join-Path $LogRoot 'ninja-stderr.log'
            Remove-Item -LiteralPath $stdout,$stderr -Force -ErrorAction SilentlyContinue
            New-Item -ItemType Directory -Path $LogRoot -Force | Out-Null

            # Windows PowerShell 5's Start-Process -Wait follows descendants;
            # Cargo helpers can outlive a completed Ninja process and strand
            # the builder after a successful link. The native call operator
            # waits for Ninja itself and leaves its exact status in LASTEXITCODE.
            $savedErrorPreference = $ErrorActionPreference
            try {
                # Cargo writes ordinary progress to stderr. Under the script's
                # fail-fast preference PowerShell 5 promotes that text to a
                # terminating NativeCommandError even when Cargo succeeds.
                $ErrorActionPreference = 'Continue'
                $nativeOutput = & $ninja @Arguments 2>&1
                $exitCode = $LASTEXITCODE
                $nativeOutput | Set-Content -LiteralPath $stdout -Encoding utf8
                Set-Content -LiteralPath $stderr -Value '' -Encoding utf8
            } finally {
                $ErrorActionPreference = $savedErrorPreference
            }

            if (Test-Path -LiteralPath $stdout) {
                Get-Content -LiteralPath $stdout | ForEach-Object { Write-Host $_ }
            }
            if (Test-Path -LiteralPath $stderr) {
                Get-Content -LiteralPath $stderr | ForEach-Object { Write-Host $_ -ForegroundColor DarkYellow }
            }

            if ($exitCode -ne 0) {
                Write-Host "$Label failed with exit code $exitCode." -ForegroundColor Red
                Write-Host "Ninja stdout: $stdout" -ForegroundColor DarkGray
                Write-Host "Ninja stderr: $stderr" -ForegroundColor DarkGray
            }
            return $exitCode
        }

        $parallelArgs = @('-C', $build) + $targets
        $parallelExit = Invoke-NinjaCaptured $parallelArgs 'Parallel Ninja compile'
        if ($parallelExit -ne 0) {
            Write-Warning 'Parallel Ninja compile failed. Retrying the same generated graph single-threaded with verbose diagnostics.'
            $serialArgs = @('-C', $build, '-j', '1', '-v') + $targets
            $serialExit = Invoke-NinjaCaptured $serialArgs 'Serial Ninja compile'
            if ($serialExit -ne 0) { throw "Universal build failed (serial retry exit $serialExit)." }
        }
        $core = if ($OnlyPlugin) { $null } else { Join-Path $build 'RuneSchema.dll' }
        if (-not $OnlyPlugin) {
            if (-not (Test-Path $core -PathType Leaf)) { throw "Built core DLL not found: $core" }
            $hostDll = Join-Path $build "$Configuration\bin\UE4SS.dll"
            $abiCheck = Join-Path $RawSource 'tools\test-ue4ss-abi.ps1'
            if (Test-Path $hostDll -PathType Leaf) {
                & $abiCheck -Plugin $core -HostDll $hostDll
                if ($LASTEXITCODE) { throw 'UE4SS ABI audit failed for universal RuneSchema.' }
            } else { Write-Warning "Built UE4SS host DLL not found for an ABI audit: $hostDll" }
        }
        [pscustomobject]@{ Build = $build; Core = $core; Helpy = (Join-Path $build 'RuneSchema.Helpy.dll') }
    }
    function New-HelpyPluginPackage([string]$HelpyDll) {
        $name = "RuneSchema.Helpy-$Version"
        $packageRoot = Join-Path $DistRoot $name
        $payload = Join-Path $packageRoot 'RuneSchema.Helpy'
        if (Test-Path $packageRoot) { Remove-Item -LiteralPath $packageRoot -Recurse -Force }
        New-Item -ItemType Directory -Path $payload -Force | Out-Null
        $helpyTemplate = Join-Path $SourceRoot 'plugins\RuneSchema.Helpy'
        if (Test-Path -LiteralPath $helpyTemplate -PathType Container) {
            Get-ChildItem -LiteralPath $helpyTemplate -Force | ForEach-Object {
                Copy-Item -LiteralPath $_.FullName -Destination $payload -Recurse -Force
            }
        }
        New-Item -ItemType Directory -Path (Join-Path $payload 'dll') -Force | Out-Null
        Copy-Item -LiteralPath $HelpyDll -Destination (Join-Path $payload 'dll\RuneSchema.Helpy.dll') -Force
        $state = Compress-DllBestEffort (Join-Path $payload 'dll\RuneSchema.Helpy.dll')
        @{ File = 'dll\RuneSchema.Helpy.dll'; State = $state } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $payload 'DLL-COMPRESSION.json') -Encoding utf8
        Attempt-Signing @((Join-Path $payload 'dll\RuneSchema.Helpy.dll'))
        $zip = Join-Path $DistRoot "$name.zip"
        if (Test-Path $zip) { Remove-Item -LiteralPath $zip -Force }
        Compress-Archive -LiteralPath $payload -DestinationPath $zip -CompressionLevel Optimal
        $pluginRoot = Join-Path $BuildRoot 'plugins\RuneSchema.Helpy'
        if (Test-Path $pluginRoot) { Remove-Item -LiteralPath $pluginRoot -Recurse -Force }
        Copy-Item -LiteralPath $payload -Destination $pluginRoot -Recurse
        Write-Host "Created plugin-only package $zip; RuneSchema.dll was not built or replaced." -ForegroundColor Green
    }
    function New-Package([string]$Name, [string]$CoreDll, [string]$HelpyDll, [object]$Mappings, [bool]$IncludePlugins = $true) {
        $packageRoot = Join-Path $DistRoot $Name
        $payload = Join-Path $packageRoot 'RuneSchema'
        if (Test-Path $packageRoot) { Remove-Item -LiteralPath $packageRoot -Recurse -Force }

        # Build dependencies store the package template under the neutral
        # directory name "runtime-template". Do not copy that directory name
        # into dist; create the intended RuneSchema payload explicitly and copy
        # the template CONTENTS into it.
        New-Item -ItemType Directory -Path $payload -Force | Out-Null
        Get-ChildItem -LiteralPath $CleanBase -Force | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $payload -Recurse -Force
        }

        # Guarantee package directories required by the current build even if a
        # future slim dependency template omits an empty directory.
        foreach ($requiredDirectory in @(
            (Join-Path $payload 'dlls'),
            (Join-Path $payload 'settings'),
            (Join-Path $payload 'settings\defaults')
        )) {
            New-Item -ItemType Directory -Path $requiredDirectory -Force | Out-Null
        }
        $sourceSettingsFile = Join-Path $SourceRoot 'settings\settings.jsonc'
        if (Test-Path -LiteralPath $sourceSettingsFile -PathType Leaf) {
            Copy-Item -LiteralPath $sourceSettingsFile -Destination (Join-Path $payload 'settings\settings.jsonc') -Force
        }
        $defaultsReadme = Join-Path $SourceRoot 'settings\defaults\README.md'
        if (Test-Path -LiteralPath $defaultsReadme -PathType Leaf) {
            Copy-Item -LiteralPath $defaultsReadme -Destination (Join-Path $payload 'settings\defaults\README.md') -Force
        }
        if ($IncludePlugins) {
            # Plugin source is authoritative. Never let a cached runtime
            # template resurrect retired plugin names, PAKs, or manifests.
            $payloadPlugins = Join-Path $payload 'plugins'
            if (Test-Path -LiteralPath $payloadPlugins) {
                Remove-Item -LiteralPath $payloadPlugins -Recurse -Force
            }
            New-Item -ItemType Directory -Path $payloadPlugins -Force | Out-Null
            $sourcePlugins = Join-Path $SourceRoot 'plugins'
            if (Test-Path -LiteralPath $sourcePlugins -PathType Container) {
                Get-ChildItem -LiteralPath $sourcePlugins -Force | ForEach-Object {
                    Copy-Item -LiteralPath $_.FullName -Destination $payloadPlugins -Recurse -Force
                }
            }
            New-Item -ItemType Directory -Path (Join-Path $payload 'plugins\RuneSchema.Helpy\dll') -Force | Out-Null
        }

        # UE4SS enable marker.
        Set-Content -LiteralPath (Join-Path $payload 'enabled.txt') -Value '' -Encoding ascii
        $mods = Join-Path $payload 'mods'
        if (Test-Path $mods) { Remove-Item -LiteralPath $mods -Recurse -Force }
        # Keep the documented drop-in layout without shipping a load-order file
        # or sample mods that could replace an existing installation's content.
        New-Item -ItemType Directory -Path $mods -Force | Out-Null
        if (-not $IncludePlugins) {
            $optionalPlugins = Join-Path $payload 'plugins'
            if (Test-Path $optionalPlugins) { Remove-Item -LiteralPath $optionalPlugins -Recurse -Force }
        }
        Copy-Item -LiteralPath $CoreDll -Destination (Join-Path $payload 'dlls\main.dll') -Force
        $mappingDirectory = Join-Path $payload 'dlls\mappings'
        New-Item -ItemType Directory -Path $mappingDirectory -Force | Out-Null
        Copy-Item -LiteralPath $Mappings.Path -Destination (Join-Path $mappingDirectory 'Mappings.usmap') -Force
        $Mappings.Lock | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $payload 'settings\MAPPINGS-SOURCE.json') -Encoding utf8
        if ($IncludePlugins) {
            Copy-Item -LiteralPath $HelpyDll -Destination (Join-Path $payload 'plugins\RuneSchema.Helpy\dll\RuneSchema.Helpy.dll') -Force
        }
        $report = foreach ($dll in Get-ChildItem -LiteralPath $payload -Filter '*.dll' -File -Recurse) {
            $before = $dll.Length; $state = Compress-DllBestEffort $dll.FullName
            [pscustomobject]@{ File = $dll.FullName.Substring($payload.Length + 1); Before = $before; After = (Get-Item $dll.FullName).Length; State = $state }
        }
        if (-not $report) { throw "No RuneSchema DLLs found in $payload" }
        $report | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $payload 'settings\DLL-COMPRESSION.json') -Encoding utf8
        $dlls = @(Get-ChildItem -LiteralPath $payload -Filter '*.dll' -File -Recurse | ForEach-Object FullName)
        Attempt-Signing $dlls
        $zip = Join-Path $DistRoot "$Name.zip"
        if (Test-Path $zip) { Remove-Item -LiteralPath $zip -Force }
        Compress-Archive -LiteralPath $payload -DestinationPath $zip -CompressionLevel Optimal
        Write-Host "Created $zip" -ForegroundColor Green
    }

    Ensure-BuildDependencies
    foreach ($required in @((Join-Path $RawSource 'CMakeLists.txt'), $CleanBase, $Upx)) {
        if (-not (Test-Path -LiteralPath $required)) { throw "Required build input is missing: $required" }
    }
    $cmake = Find-Exe 'cmake.exe'; $git = Find-Exe 'git.exe'
    if (-not $cmake -or -not $git) { throw 'CMake and Git are required and must be on PATH.' }
    Initialize-MsvcEnvironment
    Initialize-GitHubTransport
    $mappings = if ($PluginOnly) { $null } else { Ensure-Mappings }
    $ue4ssPin = Ensure-UE4SSSource
    Write-Host "RuneSchema will compile against pinned UE4SS $ue4ssPin." -ForegroundColor DarkCyan
    if ($Clean -and -not $PluginOnly) {
        foreach ($path in @($BuildCache, $DistRoot)) {
            Remove-SafeTree $path
        }
    }
    New-Item -ItemType Directory -Path $BuildCache, $DistRoot -Force | Out-Null
    $universal = Invoke-UniversalBuild -OnlyPlugin:$PluginOnly
    if ($Tests) {
        Write-Host "`n=== Optional RuneSchema contract tests ===" -ForegroundColor Cyan
        $dependencyRoot = Join-Path $universal.Build '_deps'
        $jsonHeaders = Join-Path $dependencyRoot 'nlohmann_json-src\include'
        $glazeHeaders = Join-Path $dependencyRoot 'glaze-src\include'
        $contractBuild = Join-Path $BuildCache 'contracts'
        $contractSource = Join-Path $RawSource 'core'
        $contractConfigureArgs = @(
            '-S', $contractSource,
            '-B', $contractBuild,
            '-G', 'Ninja',
            "-DRUNESCHEMA_JSON_INCLUDE_DIR=$jsonHeaders",
            "-DRUNESCHEMA_GLAZE_INCLUDE_DIR=$glazeHeaders",
            '-DCMAKE_BUILD_TYPE=Release',
            '-DCMAKE_SUPPRESS_REGENERATION=ON',
            '-Wno-author',
            '-Wno-deprecated'
        )
        Ensure-CMakeConfigured $contractSource $contractBuild $contractConfigureArgs 'release contract tests'

        $releaseContracts = if ($PluginOnly) {
            @('helpy-instant-open')
        } else {
            @(
                'vendor-offers','loader-schemas','npc-catalog','player-activity-events',
                'quest-gameplay-owner','quest-native-contract','quest-definition','event-definition',
                'character-entry-recovery-contract',
                'appearance-defaults-contract',
                'dialogue-definition','building-preview-safety','building-clone-contract',
                'static-building-assembly-contract','owned-save-cleanup-contract',
                'resource-additional-drops','resource-scale-idempotence','niagara-preset',
                'time-of-day-contract','registry-patch-plan','registry-bridge-lifecycle-contract',
                'json-document','asset-patch-v2-contract','helpy-instant-open',
                'plugin-catalog-compatibility','recipe-placement-contract',
                'loader-folder-case','usmap-index','native-binding-resolution','identity-only-cleanup','persistence-diagnostic-ledger',
                'vendor-category-refresh-contract','storefront-lanes','state-storage-contract',
                'equipment-storefront-lane','native-contract','journal-failure-isolation',
                'journal-wingdk-lane','journal-save-ownership','loader-lifecycle-contract',
                'recipe-reference-contract','main-menu-log-budget','runtime-widget-v08-contract','config-settings',
                'persistence-mode-contract','preview-refresh-contract'
            )
        }

        & ninja.exe -C $contractBuild -j 1 @releaseContracts
        if ($LASTEXITCODE) { throw "Optional contract test build failed (exit $LASTEXITCODE)." }

        $contractPattern = '^(' + (($releaseContracts | ForEach-Object {[regex]::Escape($_)}) -join '|') + ')$'
        Invoke-Checked 'ctest.exe' @('--test-dir', $contractBuild, '--output-on-failure', '-R', $contractPattern) 'Optional contract tests'
    } else {
        Write-Host "`nSkipping internal contract tests for normal package build." -ForegroundColor DarkGray
        Write-Host "Run Build RuneSchema.bat -Tests to execute the supported local contract suite." -ForegroundColor DarkGray
    }

    $helpy = $universal.Helpy
    if (-not (Test-Path $helpy -PathType Leaf)) { throw "Built Helpy DLL not found: $helpy" }
    if ($PluginOnly) {
        New-HelpyPluginPackage $helpy
        Write-Host "`n$Version Helpy-only build complete: $DistRoot" -ForegroundColor Green
        return
    }
    Copy-Item -LiteralPath $universal.Core `
        -Destination (Join-Path $DistRoot "RuneSchema-$Version.dll") -Force
    New-Package "RuneSchema-$Version-Universal" $universal.Core $helpy $mappings $true
    # Plugin-free runtime.
    New-Package "RuneSchema-$Version-Core" $universal.Core $helpy $mappings $false
    $pluginRoot = Join-Path $BuildRoot 'plugins'
    if (Test-Path $pluginRoot) { Remove-Item -LiteralPath $pluginRoot -Recurse -Force }
    New-Item -ItemType Directory -Path $pluginRoot -Force | Out-Null
    $package = Get-ChildItem -LiteralPath $DistRoot -Directory -Filter "RuneSchema-$Version-Universal" | Select-Object -First 1
    if (-not $package) { throw 'Universal package directory was not produced.' }
    Copy-Item -LiteralPath (Join-Path $package.FullName 'RuneSchema\dlls') -Destination (Join-Path $pluginRoot 'Universal\dlls') -Recurse
    Copy-Item -LiteralPath (Join-Path $package.FullName 'RuneSchema\plugins') -Destination (Join-Path $pluginRoot 'Universal\plugins') -Recurse
    Write-Host "`n$Version build complete: $DistRoot" -ForegroundColor Green
    Write-Host 'UE4SS storefront packages are distributed separately from RuneSchema build output.' -ForegroundColor DarkGray
} catch {
    Write-Error $_
    exit 1
} finally {
    if ($OwnTranscript) {
        try { Stop-Transcript | Out-Null } catch {}
    }
}
