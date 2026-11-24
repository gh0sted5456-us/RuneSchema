param([Parameter(Mandatory=$true)][string]$Root)
$ErrorActionPreference='Stop'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $Root 'build/build.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Invalid build script'}
$function=$ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Write-ModMenu2Key'},$true)
if(@($function).Count -ne 1){throw 'Missing key packaging helper'}
. ([scriptblock]::Create($function.Extent.Text))
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('RuneSchema-PublicKeyTest-'+[Guid]::NewGuid().ToString('N'))
$previous=$env:GITHUB_ACTIONS
try{
    New-Item -ItemType Directory -Path $fixture|Out-Null
    $BuildRoot=$Root;$SourceRoot=Join-Path $Root 'source'
    $env:GITHUB_ACTIONS='true';Write-ModMenu2Key $fixture
    if(Test-Path (Join-Path $fixture 'modmenu2.key')){throw 'CI generated an activation key'}
    $env:GITHUB_ACTIONS='false';Write-ModMenu2Key $fixture
    if(!(Test-Path (Join-Path $fixture 'modmenu2.key'))){throw 'Private packaging lost its activation key'}
    'Public CI excludes activation keys; private packaging remains available.'
}finally{
    $env:GITHUB_ACTIONS=$previous
    $boundary=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    if(!$fixture.StartsWith($boundary,[StringComparison]::OrdinalIgnoreCase)){throw 'Test cleanup escaped temp'}
    if(Test-Path -LiteralPath $fixture){Remove-Item -LiteralPath $fixture -Recurse -Force}
}
