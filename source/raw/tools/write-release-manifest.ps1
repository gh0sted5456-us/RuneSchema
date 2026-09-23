param(
    [Parameter(Mandatory=$true)][string]$ReleaseDirectory,
    [Parameter(Mandatory=$true)][string]$Version,
    [Parameter(Mandatory=$true)][string]$BuildDirectory
)
$ErrorActionPreference = 'Stop'
$releaseRoot = (Resolve-Path -LiteralPath $ReleaseDirectory).Path
$sourceRoot = Join-Path $releaseRoot 'raw'
$dllPath = Join-Path $releaseRoot 'dlls/main.dll'
if (!(Test-Path -LiteralPath $dllPath)) { throw 'No packaged DLL; build and verify before recording a release.' }
$dependencies = @(& (Join-Path $sourceRoot 'tools/check-dependencies.ps1') -BuildDirectory $BuildDirectory)
$files = @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -File | Sort-Object FullName | ForEach-Object {
    [ordered]@{
        Path = $_.FullName.Substring($sourceRoot.Length + 1).Replace('\','/')
        SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
})
# Emit JSON for capture by the release workflow; never installs or mutates files.
[ordered]@{
    Version = $Version
    RecordedUtc = [DateTime]::UtcNow.ToString('o')
    DllSHA256 = (Get-FileHash -LiteralPath $dllPath -Algorithm SHA256).Hash
    DllBytes = (Get-Item -LiteralPath $dllPath).Length
    Dependencies = $dependencies
    SourceFiles = $files
} | ConvertTo-Json -Depth 6
