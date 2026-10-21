param([Parameter(Mandatory=$true)][string]$Source,[Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference='Stop'
$manifest=Get-Content -LiteralPath (Join-Path $Source 'manifest.json') -Raw|ConvertFrom-Json
if($manifest.Files.Count -ne 31){throw 'Expected 31 optional presentation assets'}
[IO.Directory]::CreateDirectory($Destination)|Out-Null
foreach($file in $manifest.Files){
  if($file.Name -notmatch '^[A-Za-z0-9_-]+\.png$'){throw 'Unexpected asset name'}
  $path=Join-Path $Source $file.Name
  $sha=[Security.Cryptography.SHA256]::Create()
  try{$hash=[BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($path))).Replace('-','')}finally{$sha.Dispose()}
  if((Get-Item -LiteralPath $path).Length -ne $file.Bytes -or $hash -ine $file.SHA256){throw ('Optional art did not verify: '+$file.Name)}
  Copy-Item -LiteralPath $path -Destination (Join-Path $Destination $file.Name) -Force
}
[IO.File]::WriteAllText((Join-Path $Destination 'ready'),'Verified offline render fixture')
