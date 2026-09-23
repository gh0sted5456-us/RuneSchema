[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Plugin,
    [Parameter(Mandatory)][string]$HostDll,
    [string]$Dumpbin = 'dumpbin.exe'
)
$ErrorActionPreference = 'Stop'
foreach ($path in @($Plugin, $HostDll)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing PE file: $path" }
}
if (-not (Get-Command $Dumpbin -ErrorAction SilentlyContinue)) { throw "dumpbin is unavailable: $Dumpbin" }

function Get-Exports([string]$Path) {
    $result = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($line in (& $Dumpbin /nologo /exports $Path)) {
        if ($line -match '^\s*\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)') {
            [void]$result.Add($Matches[1])
        }
    }
    $result
}

function Get-Ue4ssImports([string]$Path) {
    $result = [Collections.Generic.List[string]]::new()
    $inside = $false
    $awaitingName = $false
    foreach ($line in (& $Dumpbin /nologo /imports $Path)) {
        if ($line -match '^\s*UE4SS\.dll\s*$') { $inside = $true; continue }
        if (-not $inside) { continue }
        if ($line -match '^\s*Summary\s*$' -or $line -match '^\s*[A-Za-z0-9_.-]+\.dll\s*$') { break }
        if ($line -match '^\s*[0-9A-F]{1,8}\s+(\?.+?)\s*$') {
            $result.Add($Matches[1]); $awaitingName = $false; continue
        }
        if ($line -match '^\s*[0-9A-F]{1,8}\s*$') { $awaitingName = $true; continue }
        if ($awaitingName -and $line.TrimStart().StartsWith('?')) {
            $result.Add($line.Trim()); $awaitingName = $false
        }
    }
    $result
}

$exports = Get-Exports $HostDll
$imports = Get-Ue4ssImports $Plugin
if ($imports.Count -eq 0) { throw "No UE4SS imports found in $Plugin" }
$missing = @($imports | Where-Object { -not $exports.Contains($_) } | Sort-Object -Unique)
if ($missing.Count) {
    throw "UE4SS ABI mismatch: $($missing.Count) imported symbol(s) are absent from $HostDll`n$($missing -join "`n")"
}
Write-Host "UE4SS ABI OK: $($imports.Count) imports are exported by $HostDll" -ForegroundColor Green
