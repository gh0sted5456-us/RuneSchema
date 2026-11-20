$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$SourceRoot=Join-Path $repo 'source'
$BuildRoot=$repo
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $repo 'build/build.ps1'),[ref]$null,[ref]$null)
$helper=$ast.Find({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Copy-HelpyFeaturePlugin'},$true)
if(-not $helper){throw 'Native feature packaging helper missing'}
. ([scriptblock]::Create($helper.Extent.Text))
$keyWriter=$ast.Find({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Write-ModMenu2Key'},$true)
if(-not $keyWriter){throw 'Mod Menu 2 key packaging helper missing'}
. ([scriptblock]::Create($keyWriter.Extent.Text))
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
 # The packaged key must satisfy the runtime check: hash, blank line, original license.
 $key=[IO.File]::ReadAllText((Join-Path $root 'stage/RuneSchema.ModMenu2/modmenu2.key'))
 $hash=[regex]::Match([IO.File]::ReadAllText((Join-Path $SourceRoot 'shared/ModMenu2Key.h')),'Hash\[\]="([0-9a-f]{64})"').Groups[1].Value
 $license=[IO.File]::ReadAllText((Join-Path $repo 'licenses/ModMenu-MIT.txt')).Replace("`r",'').TrimEnd("`n")
 if($key -ne ($hash+"`n`n"+$license+"`n")){throw 'Packaged modmenu2.key does not match the runtime key format'}
 if(Test-Path -LiteralPath (Join-Path $root 'stage/RuneSchema.DeveloperTools/modmenu2.key')){throw 'Only Mod Menu 2 carries the key'}
 'Native Helpy packaging: both frontend IDs, DLL filenames, manifest hosts, bytes, original notice and Mod Menu 2 key passed.'
}finally{
 if([IO.Path]::GetDirectoryName($root) -eq $base -and [IO.Path]::GetFileName($root) -match '^RS-frontend-package-[a-f0-9]{32}$'){if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}}
}
