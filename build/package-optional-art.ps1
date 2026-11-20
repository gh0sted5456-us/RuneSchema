param(
    [Parameter(Mandatory=$true)][string]$PayloadRoot,
    [Parameter(Mandatory=$true)][string]$StagingRoot,
    [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference='Stop'
$payload=[IO.Path]::GetFullPath($PayloadRoot).TrimEnd([char[]]'\/')
$stage=[IO.Path]::GetFullPath($StagingRoot).TrimEnd([char[]]'\/')
if([IO.Path]::GetFileName($payload) -notin @('RuneSchema.Helpy','Helpy') -or -not $payload.StartsWith($stage+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Artwork filtering requires a Helpy payload inside the explicit packaging staging root'}
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
# Helpy keeps its own files in AppData core/HELPY: settings, backgrounds, fonts and groupings are created there
# on first use, the dedicated-server script is built into Helpy.exe, and the window icon is an exe resource.
$owned=@('assets/Apply-DedicatedServerSettings.ps1','assets/RuneSchema.ico','assets/backgrounds/Backgrounds.txt','settings/ui.jsonc','settings/settings.jsonc','settings/item-groupings.example.json')
foreach($relative in $owned){
    $target=[IO.Path]::GetFullPath((Join-Path $payload $relative))
    if(-not $target.StartsWith($payload+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'AppData-owned path escaped payload'}
    if([IO.File]::Exists($target)){[IO.File]::Delete($target);$removed+=$relative}
}
foreach($relative in @('assets/portraits','assets/backgrounds','assets','settings')){
    $folder=Join-Path $payload $relative
    if((Test-Path -LiteralPath $folder -PathType Container) -and -not (Get-ChildItem -LiteralPath $folder -Force|Select-Object -First 1)){Remove-Item -LiteralPath $folder -Force}
}
Write-Output ('Optional artwork and AppData-owned Helpy files excluded from runtime payload: '+$removed.Count+' files')
