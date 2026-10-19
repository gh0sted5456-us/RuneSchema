param(
    [Parameter(Mandatory=$true)][string]$PayloadRoot,
    [Parameter(Mandatory=$true)][string]$StagingRoot,
    [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference='Stop'
$payload=[IO.Path]::GetFullPath($PayloadRoot).TrimEnd([char[]]'\/')
$stage=[IO.Path]::GetFullPath($StagingRoot).TrimEnd([char[]]'\/')
if([IO.Path]::GetFileName($payload) -ne 'RuneSchema.Helpy' -or -not $payload.StartsWith($stage+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Artwork filtering requires a Helpy payload inside the explicit packaging staging root'}
$manifest=Get-Content -LiteralPath (Join-Path $RepositoryRoot 'resources/helpy-ui/v1/manifest.json') -Raw|ConvertFrom-Json
$paths=@($manifest.Files|ForEach-Object {
    if([IO.Path]::GetFileName($_.Name) -ne $_.Name -or $_.Name -notmatch '\.png$'){throw 'Unexpected managed artwork filename'}
    'assets/'+$_.Name
})
$paths+=@('assets/portraits/portrait-player.svg','assets/portraits/portrait-tree.svg','assets/portraits/portrait-stone.svg','assets/portraits/portrait-enemy.svg','assets/backgrounds/Dragonwilds.png','assets/backgrounds/RuneSchema.png')
$removed=@()
foreach($relative in $paths){
    $target=[IO.Path]::GetFullPath((Join-Path $payload $relative))
    if(-not $target.StartsWith($payload+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Artwork path escaped payload'}
    for($current=$target;$current -and $current.StartsWith($stage,[StringComparison]::OrdinalIgnoreCase);$current=Split-Path -Parent $current){
        if((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Linked artwork staging path refused'}
    }
    if([IO.File]::Exists($target)){[IO.File]::Delete($target);$removed+=$relative}
}
Write-Output ('Optional GitHub artwork excluded from runtime payload: '+$removed.Count+' files')
