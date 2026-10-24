[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SourceArchive,
    [Parameter(Mandatory = $true)][string]$DestinationArchive,
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)
# Packaging-only: never recompiles, recompresses DLLs, or changes runtime bytes.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$source = [IO.Path]::GetFullPath($SourceArchive)
$destination = [IO.Path]::GetFullPath($DestinationArchive)
$staging = Join-Path ([IO.Path]::GetTempPath()) ('RuneSchema-Runtime-' + [guid]::NewGuid().ToString('N'))
$temporary = $destination + '.' + [guid]::NewGuid().ToString('N') + '.tmp.zip'
$originalHashes = @{}
$removed = [Collections.Generic.List[string]]::new()
function Hash-File([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }
function Is-Extra([string]$Relative) {
    # Explicit paths, not a wildcard deleting all .txt/.json/.lua files.
    return $Relative.StartsWith('dlls/mappings/', [StringComparison]::OrdinalIgnoreCase) -or $Relative -in @(
        'dlls/mappings/README.txt', 'settings/defaults/README.md',
        'settings/DLL-COMPRESSION.json', 'settings/MAPPINGS-SOURCE.json',
        'settings/jobs/building-menu.example.json', 'settings/jobs/character-editor.example.json'
    )
}
try {
    New-Item -ItemType Directory -Path $staging -ErrorAction Stop | Out-Null
    $inputZip = [IO.Compression.ZipFile]::OpenRead($source)
    try {
        $seen = @{}
        foreach ($entry in $inputZip.Entries) {
            $relative = $entry.FullName.Replace('\','/')
            if ($relative -notmatch '^RuneSchema/' -or $relative -match '(^|/)\.\.(/|$)|:|^/' -or $seen.ContainsKey($relative)) {
                throw "Unsafe, unexpected, or duplicate ZIP entry: $relative"
            }
            $seen[$relative] = $true
            $target = [IO.Path]::GetFullPath((Join-Path $staging $relative))
            if (-not $target.StartsWith($staging + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
                throw 'ZIP entry escapes staging.'
            }
            if ($relative.EndsWith('/')) { New-Item -ItemType Directory -Path $target -Force | Out-Null; continue }
            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target)
            $originalHashes[$relative] = Hash-File $target
        }
    } finally { $inputZip.Dispose() }
    $payload = Join-Path $staging 'RuneSchema'
    foreach ($required in @('dlls/main.dll', 'enabled.txt', 'scripts/main.lua', 'plugins/plugins.txt')) {
        if (-not (Test-Path -LiteralPath (Join-Path $payload $required) -PathType Leaf)) { throw "Missing runtime file: $required" }
    }
    if (-not ((Test-Path -LiteralPath (Join-Path $payload 'settings/settings.ini')) -or
              (Test-Path -LiteralPath (Join-Path $payload 'settings/settings.jsonc')))) { throw 'Missing runtime settings.' }
    foreach ($file in Get-ChildItem -LiteralPath $payload -File -Recurse) {
        $relative = $file.FullName.Substring($payload.Length + 1).Replace('\','/')
        if (Is-Extra $relative) { Remove-Item -LiteralPath $file.FullName -Force; $removed.Add('RuneSchema/' + $relative) }
    }
    $legacyMappings = [IO.Path]::GetFullPath((Join-Path $payload 'dlls/mappings'))
    if (-not $legacyMappings.StartsWith($staging + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Mapping cleanup escaped package staging' }
    if (Test-Path -LiteralPath $legacyMappings) { Remove-Item -LiteralPath $legacyMappings -Recurse -Force }
    & (Join-Path $PSScriptRoot 'package-licenses.ps1') -RepositoryRoot $RepositoryRoot -PayloadRoot $payload
    $legalRelative = 'RuneSchema/settings/licenses/RuneSchema-Licenses.txt'
    $retained = @{}
    foreach ($file in Get-ChildItem -LiteralPath $payload -File -Recurse) {
        $relative = $file.FullName.Substring($staging.Length + 1).Replace('\','/')
        $retained[$relative] = Hash-File $file.FullName
        if ($relative -ne $legalRelative -and $retained[$relative] -ne $originalHashes[$relative]) { throw "Runtime bytes changed: $relative" }
    }
    foreach ($relative in $originalHashes.Keys) {
        if ($retained.ContainsKey($relative)) { continue }
        if ((Is-Extra $relative.Substring(11)) -or $relative -match '(?i)/(licenses/|LICENSE$|AUTHORS\.md$|LICENSING\.md$|CONTRIBUTING\.md$|THIRD_PARTY_NOTICES\.md$)') { continue }
        throw "Unexpected removed runtime file: $relative"
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Compress-Archive -LiteralPath $payload -DestinationPath $temporary -CompressionLevel Optimal
    $outputZip = [IO.Compression.ZipFile]::OpenRead($temporary)
    try {
        $verified = 0
        foreach ($entry in $outputZip.Entries) {
            if ($entry.FullName.EndsWith('/') -or $entry.FullName.EndsWith('\')) { continue }
            $relative = $entry.FullName.Replace('\','/')
            $stream = $entry.Open(); $sha = [Security.Cryptography.SHA256]::Create()
            try { $hash = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','') } finally { $sha.Dispose(); $stream.Dispose() }
            if ($hash -ne $retained[$relative]) { throw "Archive verification failed: $relative" }
            $verified++
        }
        if ($verified -ne $retained.Count) { throw 'Archive file count mismatch.' }
    } finally { $outputZip.Dispose() }
    Move-Item -LiteralPath $temporary -Destination $destination -Force
    [pscustomobject]@{ Archive = $destination; SHA256 = (Hash-File $destination); VerifiedFiles = $verified; RemovedExtras = @($removed) }
} finally {
    # Delete only this invocation's validated, unique temporary paths.
    $tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/')
    if ([IO.Path]::GetFullPath($staging).StartsWith($tempBase + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($staging) -match '^RuneSchema-Runtime-[0-9a-f]{32}$') {
        if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
    }
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
}
