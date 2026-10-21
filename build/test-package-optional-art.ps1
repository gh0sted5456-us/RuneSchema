$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('RuneSchema-OptionalArtTest-'+[Guid]::NewGuid().ToString('N'))
$stage=Join-Path $fixture 'dist'
$plugin=Join-Path $stage 'RuneSchema-0.8.0.1/RuneSchema/plugins/RuneSchema.Helpy'
function FixtureFile([string]$relative){$file=Join-Path $plugin $relative;[IO.Directory]::CreateDirectory((Split-Path -Parent $file))|Out-Null;[IO.File]::WriteAllText($file,'isolated test fixture');$file}
try{
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'build.ps1'),[ref]$tokens,[ref]$errors)
    $helpers=@($ast.FindAll({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Copy-HelpyDlls'},$true))
    if($errors.Count -or $helpers.Count -ne 1){throw 'Helpy packaging helper missing or invalid'}
    . ([scriptblock]::Create($helpers[0].Extent.Text))
    $compiled=Join-Path $fixture 'compiled';[IO.Directory]::CreateDirectory($compiled)|Out-Null
    $ui=Join-Path $compiled 'RuneSchema.Helpy.dll';[IO.File]::WriteAllText($ui,'tested UI')
    $destination=Join-Path $fixture 'packaged/dll';$missing=$false
    try{Copy-HelpyDlls $ui $destination}catch{$missing=$true}
    if(-not $missing -or (Test-Path -LiteralPath $destination)){throw 'Missing native companion allowed a partial Helpy package'}
    [IO.File]::WriteAllText((Join-Path $compiled 'RuneSchema.Helpy.Native.dll'),'tested native companion')
    Copy-HelpyDlls $ui $destination
    foreach($name in @('RuneSchema.Helpy.dll','RuneSchema.Helpy.Native.dll')){if((Get-FileHash -LiteralPath (Join-Path $compiled $name)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $destination $name)).Hash){throw 'Helpy package DLL differs from build'}}
    $manifest=Get-Content -LiteralPath (Join-Path $repo 'resources/helpy-ui/v1/manifest.json') -Raw|ConvertFrom-Json
    $optional=@($manifest.Files|ForEach-Object {'assets/'+$_.Name})+@('assets/portraits/portrait-player.svg','assets/portraits/portrait-tree.svg','assets/portraits/portrait-stone.svg','assets/portraits/portrait-enemy.svg','assets/backgrounds/Dragonwilds.png','assets/backgrounds/RuneSchema.png')
    foreach($file in $optional){FixtureFile $file|Out-Null}
    $kept=@('dll/RuneSchema.Helpy.dll','dll/RuneSchema.Helpy.Native.dll','plugin.json','assets/Apply-DedicatedServerSettings.ps1','assets/RuneSchema.ico','settings/ui.jsonc')
    foreach($file in $kept){FixtureFile $file|Out-Null}
    & (Join-Path $PSScriptRoot 'package-optional-art.ps1') -PayloadRoot $plugin -StagingRoot $stage -RepositoryRoot $repo
    foreach($file in $optional){if(Test-Path -LiteralPath (Join-Path $plugin $file)){throw 'Optional artwork remained in payload'}}
    foreach($file in $kept){if(-not (Test-Path -LiteralPath (Join-Path $plugin $file))){throw 'Required runtime file removed'}}
    & (Join-Path $PSScriptRoot 'package-optional-art.ps1') -PayloadRoot $plugin -StagingRoot $stage -RepositoryRoot $repo
    $rejected=$false
    try{& (Join-Path $PSScriptRoot 'package-optional-art.ps1') -PayloadRoot (Join-Path $repo 'source/plugins/RuneSchema.Helpy') -StagingRoot $stage -RepositoryRoot $repo}catch{$rejected=$true}
    if(-not $rejected){throw 'Source/installed-path safety guard failed'}
    Write-Output 'PASS: managed banners/portraits/backgrounds excluded; DLLs/scripts/settings/ICO preserved; repeated calls and outside-staging rejection verified.'
}finally{
    $base=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([char[]]'\/')
    $target=[IO.Path]::GetFullPath($fixture)
    if(-not $target.StartsWith($base+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($target) -notmatch '^RuneSchema-OptionalArtTest-[0-9a-f]{32}$'){throw 'Temporary fixture cleanup target invalid'}
    if(Test-Path -LiteralPath $target){Remove-Item -LiteralPath $target -Recurse -Force}
}
