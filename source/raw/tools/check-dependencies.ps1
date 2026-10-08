param([Parameter(Mandatory=$true)][string]$BuildDirectory)
$ErrorActionPreference = 'Stop'
# Use Git builtins only: this also works without Git's shell utilities on PATH.
function Assert-CleanCheckout([string]$Source, [string]$Expected) {
    $actual = & git -C $Source rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $actual -ne $Expected) { throw "Wrong revision: $Source" }
    & git -C $Source diff --quiet HEAD --
    if ($LASTEXITCODE -ne 0) { throw "Tracked source changes: $Source" }
    $moduleFile = Join-Path $Source '.gitmodules'
    if (Test-Path -LiteralPath $moduleFile) {
        $modules = & git config --file $moduleFile --get-regexp '^submodule\..*\.path$'
        if ($LASTEXITCODE -gt 1) { throw "Cannot read $moduleFile" }
        foreach ($line in $modules) {
            if ($line -notmatch '^\S+\s+(.+)$') { throw "Invalid submodule entry: $line" }
            $relative = $Matches[1]
            $tree = & git -C $Source ls-tree HEAD -- $relative
            if ($LASTEXITCODE -ne 0 -or $tree -notmatch '^160000 commit ([0-9a-f]{40})\s') {
                throw "Missing pinned gitlink: $Source/$relative"
            }
            $revision = $Matches[1]
            Assert-CleanCheckout (Join-Path $Source $relative) $revision
        }
    }
}
# Revisions match raw/CMakeLists.txt; no dependency sources are edited.
$pins = [ordered]@{
    'ue4ss' = '265115c05bfcc75f96d8a1f4beda105a7d2753b1'
    'nlohmann_json' = 'v3.11.3'
    'safetyhook' = '8a975dee560c424329dcb0e151f3649fa4b19146'
    'glaze' = '3a850807501d98d23bab4bdc5af64d8d4e83e6bc'
    'efsw' = '22f17a0bcdf3a4edf61f8b14328391463389e548'
    'zydis' = 'a2278f1d254e492f6a6b39f6cb5d1f5d515659dc'
}
foreach ($name in $pins.Keys) {
    $source = Join-Path $BuildDirectory "_deps/$name-src"
    if (!(Test-Path -LiteralPath (Join-Path $source '.git'))) { throw "Missing checkout: $source" }
    $expected = & git -C $source rev-parse "$($pins[$name])^{commit}"
    if ($LASTEXITCODE -ne 0) { throw "Cannot resolve pin for $name" }
    Assert-CleanCheckout $source $expected
    [pscustomobject]@{ Dependency=$name; Commit=$expected; TrackedSourcesClean=$true }
}
