[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$PluginOnly
)
$ErrorActionPreference = 'Stop'
$Version = '0.7.5.23'
$BuildRoot = [IO.Path]::GetFullPath($PSScriptRoot)
if (-not (Test-Path -LiteralPath (Join-Path $BuildRoot 'source\raw\CMakeLists.txt') -PathType Leaf)) {
    $BuildRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
}
$SourceRoot = Join-Path $BuildRoot 'source'
$RawSource = Join-Path $SourceRoot 'raw'
$CleanBase = Join-Path $BuildRoot 'clean-base\RuneSchema'
$BuildCache = Join-Path $PSScriptRoot 'cache'
$DistRoot = Join-Path $BuildRoot 'dist'
$LogRoot = Join-Path $PSScriptRoot 'logs'
$Upx = Join-Path $SourceRoot 'tools\upx\upx.exe'
$Configuration = 'Game__Shipping__Win64'

New-Item -ItemType Directory -Path $LogRoot -Force | Out-Null
$log = Join-Path $LogRoot ("build-{0:yyyyMMdd-HHmmss}.log" -f (Get-Date))
Start-Transcript -LiteralPath $log | Out-Null
try {
    function Find-Exe([string]$Name, [string[]]$Hints = @()) {
        $cmd = Get-Command $Name -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
        foreach ($hint in $Hints) { if (Test-Path -LiteralPath $hint -PathType Leaf) { return $hint } }
        return $null
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
    function Remove-SafeTree([string]$Path) {
        $resolved = [IO.Path]::GetFullPath($Path)
        $root = $BuildRoot.TrimEnd('\') + '\'
        if (-not $resolved.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
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
        # Fetch public UE4SS dependencies over HTTPS.
        $env:GIT_TERMINAL_PROMPT = '0'
        $env:GCM_INTERACTIVE = 'Never'
        $env:GIT_CONFIG_COUNT = '3'
        $env:GIT_CONFIG_KEY_0 = 'url.https://github.com/.insteadOf'
        $env:GIT_CONFIG_VALUE_0 = 'git@github.com:'
        $env:GIT_CONFIG_KEY_1 = 'url.https://github.com/.insteadOf'
        $env:GIT_CONFIG_VALUE_1 = 'ssh://git@github.com/'
        $env:GIT_CONFIG_KEY_2 = 'core.longpaths'
        $env:GIT_CONFIG_VALUE_2 = 'true'
        Invoke-Checked 'git.exe' @('ls-remote', '--exit-code', 'https://github.com/UE4SS-RE/RE-UE4SS.git', 'HEAD') 'GitHub connectivity check'
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
                    $actual = (Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash
                    if ($actual -ne $env:RUNESCHEMA_SIGNER_SHA256) { throw 'Downloaded signer SHA-256 did not match the configured pin.' }
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
        Write-Host "`n=== Configure universal RuneSchema ===" -ForegroundColor Cyan
        $configureArgs = @('-S', $RawSource, '-B', $build, '-G', $generator, "-DCMAKE_BUILD_TYPE=$Configuration", '-DFETCHCONTENT_FULLY_DISCONNECTED=OFF', '-DFETCHCONTENT_UPDATES_DISCONNECTED=OFF')
        Invoke-Checked 'cmake.exe' $configureArgs 'CMake configure for universal RuneSchema'
        $targets = if ($OnlyPlugin) { @('RuneSchemaHelpyPlugin') } else { @('RuneSchema', 'RuneSchemaHelpyPlugin') }
        Write-Host $(if ($OnlyPlugin) { '=== Compile Helpy plugin only (RuneSchema.dll is untouched) ===' } else { '=== Compile universal RuneSchema ===' }) -ForegroundColor Cyan
        & cmake.exe --build $build --target $targets --parallel
        if ($LASTEXITCODE) {
            Write-Warning 'Parallel build failed; retrying single-threaded with diagnostics.'
            & cmake.exe --build $build --target $targets --parallel 1 --verbose
            if ($LASTEXITCODE) { throw "Universal build failed (serial retry exit $LASTEXITCODE)." }
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
        New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $CleanBase 'plugins\RuneSchema.Helpy') -Destination $payload -Recurse
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
    function New-Package([string]$Name, [string]$CoreDll, [string]$HelpyDll, [bool]$IncludePlugins = $true) {
        $packageRoot = Join-Path $DistRoot $Name
        $payload = Join-Path $packageRoot 'RuneSchema'
        if (Test-Path $packageRoot) { Remove-Item -LiteralPath $packageRoot -Recurse -Force }
        New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
        Copy-Item -LiteralPath $CleanBase -Destination $packageRoot -Recurse
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

    foreach ($required in @((Join-Path $RawSource 'CMakeLists.txt'), $CleanBase, $Upx)) {
        if (-not (Test-Path -LiteralPath $required)) { throw "Required build input is missing: $required" }
    }
    $cmake = Find-Exe 'cmake.exe'; $git = Find-Exe 'git.exe'
    if (-not $cmake -or -not $git) { throw 'CMake and Git are required and must be on PATH.' }
    Initialize-MsvcEnvironment
    Initialize-GitHubTransport
    if ($Clean -and -not $PluginOnly) {
        foreach ($path in @($BuildCache, $DistRoot)) {
            Remove-SafeTree $path
        }
    }
    New-Item -ItemType Directory -Path $BuildCache, $DistRoot -Force | Out-Null
    $universal = Invoke-UniversalBuild -OnlyPlugin:$PluginOnly
    $dependencyRoot = Join-Path $universal.Build '_deps'
    $jsonHeaders = Join-Path $dependencyRoot 'nlohmann_json-src\include'
    $contractBuild = Join-Path $BuildCache 'contracts'
    Invoke-Checked 'cmake.exe' @('-S', (Join-Path $RawSource 'core'), '-B', $contractBuild, '-G', 'Ninja', "-DRUNESCHEMA_JSON_INCLUDE_DIR=$jsonHeaders", '-DCMAKE_BUILD_TYPE=Release') 'Release contract test configure'
    $releaseContracts = if ($PluginOnly) { @('helpy-instant-open') } else { @('vendor-offers','loader-schemas','npc-catalog','player-activity-events',
        'quest-gameplay-owner','quest-native-contract','quest-definition','event-definition',
        'dialogue-definition','building-preview-safety','building-clone-contract','static-building-assembly-contract','owned-content-ledger','owned-save-cleanup-contract','resource-additional-drops','resource-scale-idempotence','niagara-preset',
        'time-of-day-contract','registry-patch-plan','json-document','asset-patch-v2-contract','helpy-instant-open','plugin-catalog-compatibility','documentation-contract','usmap-index','native-binding-resolution',
        'vendor-category-refresh-contract','storefront-lanes','state-storage-contract','native-contract','journal-failure-isolation',
        'journal-wingdk-lane','main-menu-log-budget') }
    Invoke-Checked 'cmake.exe' (@('--build', $contractBuild, '--target') + $releaseContracts + @('--parallel', '1')) 'Release contract test build'
    $contractPattern = '^(' + (($releaseContracts | ForEach-Object {[regex]::Escape($_)}) -join '|') + ')$'
    Invoke-Checked 'ctest.exe' @('--test-dir', $contractBuild, '--output-on-failure', '-R', $contractPattern) 'Release contract tests'
    $helpy = $universal.Helpy
    if (-not (Test-Path $helpy -PathType Leaf)) { throw "Built Helpy DLL not found: $helpy" }
    if ($PluginOnly) {
        New-HelpyPluginPackage $helpy
        Write-Host "`n$Version Helpy-only build complete: $DistRoot" -ForegroundColor Green
        return
    }
    New-Package "RuneSchema-$Version-Universal" $universal.Core $helpy $true
    # Plugin-free runtime.
    New-Package "RuneSchema-$Version-Core" $universal.Core $helpy $false
    $pluginRoot = Join-Path $BuildRoot 'plugins'
    if (Test-Path $pluginRoot) { Remove-Item -LiteralPath $pluginRoot -Recurse -Force }
    New-Item -ItemType Directory -Path $pluginRoot -Force | Out-Null
    $package = Get-ChildItem -LiteralPath $DistRoot -Directory -Filter "RuneSchema-$Version-Universal" | Select-Object -First 1
    if (-not $package) { throw 'Universal package directory was not produced.' }
    # Keep clean-base usable as the current unpacked runtime, not merely as a
    # packaging template containing DLLs inherited from the previous version.
    Copy-Item -LiteralPath (Join-Path $package.FullName 'RuneSchema\dlls\main.dll') -Destination (Join-Path $CleanBase 'dlls\main.dll') -Force
    Copy-Item -LiteralPath (Join-Path $package.FullName 'RuneSchema\plugins\RuneSchema.Helpy\dll\RuneSchema.Helpy.dll') -Destination (Join-Path $CleanBase 'plugins\RuneSchema.Helpy\dll\RuneSchema.Helpy.dll') -Force
    Copy-Item -LiteralPath (Join-Path $package.FullName 'RuneSchema\dlls') -Destination (Join-Path $pluginRoot 'Universal\dlls') -Recurse
    Copy-Item -LiteralPath (Join-Path $package.FullName 'RuneSchema\plugins') -Destination (Join-Path $pluginRoot 'Universal\plugins') -Recurse
    foreach ($runtime in @(
        @{ Source = (Join-Path $BuildRoot 'runtime\steam-gog\UE4SS-3.0.1-f6d5f942-Steam-GOG.zip'); Name = 'UE4SS-3.0.1-f6d5f942-Steam-GOG.zip' },
        @{ Source = (Join-Path $BuildRoot 'runtime\gamepass\UE4SS-3.0.1-f6d5f942-GamePass-WinGDK.zip'); Name = 'UE4SS-3.0.1-f6d5f942-GamePass-WinGDK.zip' }
    )) { Copy-Item -LiteralPath $runtime.Source -Destination (Join-Path $DistRoot $runtime.Name) -Force }
    Write-Host "`n$Version build complete: $DistRoot" -ForegroundColor Green
} catch {
    Write-Error $_
    exit 1
} finally {
    Stop-Transcript | Out-Null
}
