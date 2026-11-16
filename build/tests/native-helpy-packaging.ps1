$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$SourceRoot=Join-Path $repo 'source'
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $repo 'build/build.ps1'),[ref]$null,[ref]$null)
$helper=$ast.Find({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Copy-HelpyFeaturePlugin'},$true)
if(-not $helper){throw 'Native feature packaging helper missing'}
. ([scriptblock]::Create($helper.Extent.Text))
$base=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/')
$root=Join-Path $base ('RS-frontend-package-'+[guid]::NewGuid().ToString('N'))
try{
 New-Item -ItemType Directory -Path (Join-Path $root 'build') -Force|Out-Null
 foreach($id in @('RuneSchema.ModMenu2','RuneSchema.DeveloperTools')){
  $dll=Join-Path $root ('build/'+$id+'.dll');[IO.File]::WriteAllText($dll,$id)
  $out=Join-Path $root ('stage/'+$id);Copy-HelpyFeaturePlugin (Join-Path $root 'build/Helpy.dll') $id $out
  $manifest=Get-Content -LiteralPath (Join-Path $out 'plugin.json') -Raw|ConvertFrom-Json
  if($manifest.Id -ne $id -or $manifest.Host -ne 'helpy' -or $manifest.EntryPoint -ne ($id+'.dll')){throw 'Packaged frontend identity changed'}
  $installed=Join-Path $out ('dll/'+$manifest.EntryPoint)
  if((Get-FileHash -LiteralPath $installed).Hash -ne (Get-FileHash -LiteralPath $dll).Hash){throw 'Native frontend DLL bytes differ'}
 }
 $notice=Join-Path $root 'stage/RuneSchema.ModMenu2/ModMenu-MIT.txt'
 if((Get-FileHash -LiteralPath $notice).Hash -ne (Get-FileHash -LiteralPath (Join-Path $repo 'licenses/ModMenu-MIT.txt')).Hash){throw 'Original Mod Menu MIT notice missing'}
 'Native Helpy packaging: both frontend IDs, DLL filenames, manifest hosts, bytes and original notice passed.'
}finally{
 if([IO.Path]::GetDirectoryName($root) -eq $base -and [IO.Path]::GetFileName($root) -match '^RS-frontend-package-[a-f0-9]{32}$'){if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}}
}
