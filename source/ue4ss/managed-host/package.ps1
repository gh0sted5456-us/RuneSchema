[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$FrameworkDirectory,
    [Parameter(Mandatory)][string]$HostDll,
    [Parameter(Mandatory)][string]$UE4SSSource
)
$ErrorActionPreference = 'Stop'
throw 'Custom-host bundles are retired. Do not replace UE4SS.dll; distribute RuneSchema framework files only.'
$framework = (Resolve-Path -LiteralPath $FrameworkDirectory).Path
$hostBinary = (Resolve-Path -LiteralPath $HostDll).Path
$payload = Join-Path $framework 'RuneSchema'
if (-not (Test-Path -LiteralPath (Join-Path $payload 'dlls\main.dll') -PathType Leaf)) { throw 'Not a RuneSchema framework package' }
$exports = (& dumpbin.exe /nologo /exports $hostBinary | Out-String)
if ($LASTEXITCODE -or $exports -notmatch '\bRuneSchema_UE4SS_QueuePackageV1\b') { throw 'Matching managed host export is missing' }
& (Join-Path $PSScriptRoot '..\..\raw\tools\test-ue4ss-abi.ps1') -Plugin (Join-Path $payload 'dlls\main.dll') -HostDll $hostBinary
if ($LASTEXITCODE) { throw 'Managed host ABI check failed' }
$compression = Get-Content -LiteralPath (Join-Path $payload 'settings\DLL-COMPRESSION.json') -Raw | ConvertFrom-Json
if (@($compression | Where-Object {$_.State -ne 'UPX -9 --best --lzma; verified'}).Count -or @($compression).Count -ne 2) { throw 'Framework DLLs must use verified canonical compression' }
$destination = $framework + '-managed-host'
$zip = $destination + '.zip'
if ((Test-Path -LiteralPath $destination) -or (Test-Path -LiteralPath $zip)) { throw 'Retain the existing bundle; use a new package name' }
New-Item -ItemType Directory -Path (Join-Path $destination 'ue4ss\Mods') -Force | Out-Null
Copy-Item -LiteralPath $payload -Destination (Join-Path $destination 'ue4ss\Mods\RuneSchema') -Recurse
Copy-Item -LiteralPath $hostBinary -Destination (Join-Path $destination 'ue4ss\UE4SS.dll')
New-Item -ItemType Directory -Path (Join-Path $destination 'licenses') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $UE4SSSource 'LICENSE') -Destination (Join-Path $destination 'licenses\UE4SS-MIT.txt')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.md') -Destination (Join-Path $destination 'README.md')
$files = @(Get-ChildItem -LiteralPath $destination -File -Recurse | ForEach-Object {
    [pscustomobject]@{File=$_.FullName.Substring($destination.Length+1).Replace('\','/'); SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
})
[pscustomobject]@{Format=1; Status='alpha; native in-game validation pending'; UE4SSCommit='f6d5f9425a31f3e3a4fd185b40148f5aa4d81a44'; Api='RuneSchema_UE4SS_QueuePackageV1'; HostCompressed=$false; Files=$files} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $destination 'MANIFEST.json') -Encoding utf8
Compress-Archive -Path (Join-Path $destination '*') -DestinationPath $zip -CompressionLevel Optimal
Get-FileHash -LiteralPath $zip -Algorithm SHA256
