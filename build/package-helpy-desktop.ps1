param([Parameter(Mandatory=$true)][string]$Executable,[Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression
$source=(Resolve-Path -LiteralPath $Executable).Path
if((Get-Item -LiteralPath $source).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Use the built desktop executable, not a reparse entry'}
$output=[IO.Path]::GetFullPath($OutputPath)
if(Test-Path -LiteralPath $output){throw 'Desktop archive already exists'}
$folder=Split-Path -Parent $output;New-Item -ItemType Directory -Path $folder -Force|Out-Null
$temporary=Join-Path $folder ('.helpy-desktop-'+[Guid]::NewGuid().ToString('N')+'.zip')
$stream=$null;$zip=$null
try{
    $stream=[IO.File]::Open($temporary,[IO.FileMode]::CreateNew);$zip=New-Object IO.Compression.ZipArchive($stream,[IO.Compression.ZipArchiveMode]::Create,$false)
    $entry=$zip.CreateEntry('Helpy.exe',[IO.Compression.CompressionLevel]::Optimal);$write=$entry.Open();$read=[IO.File]::OpenRead($source)
    try{$read.CopyTo($write)}finally{$read.Dispose();$write.Dispose()}
    $entry=$zip.CreateEntry('INSTALL.txt');$writer=New-Object IO.StreamWriter($entry.Open(),(New-Object Text.UTF8Encoding($false)))
    try{$writer.WriteLine('Helpy desktop app. Local testing candidate; requires matching RuneSchema core and bridge DLLs.');$writer.WriteLine('Copy Helpy.exe beside RuneSchema/plugins/RuneSchema.Helpy/dll/RuneSchema.Helpy.dll. The game plugin starts the app hidden in the Windows system tray by default. Use the Helpy hotkey or tray icon to open it. Quick settings can select Open window at startup instead.');$writer.WriteLine('Double-click Helpy.exe to browse saved caches and manage mods with the game closed. Closing its window returns it to the tray; use Exit Helpy to quit. Read the included HELPY-STANDALONE.md for setup and testing limits.')}finally{$writer.Dispose()}
    foreach($name in @('LICENSE','THIRD_PARTY_NOTICES.md','HELPY-STANDALONE.md')){
        $file=Join-Path (Split-Path -Parent $PSScriptRoot) $(if($name -eq 'HELPY-STANDALONE.md'){'docs/'+$name}else{$name})
        $entry=$zip.CreateEntry($name);$write=$entry.Open();$read=[IO.File]::OpenRead($file)
        try{$read.CopyTo($write)}finally{$read.Dispose();$write.Dispose()}
    }
    $zip.Dispose();$zip=$null;$stream.Dispose();$stream=$null;Move-Item -LiteralPath $temporary -Destination $output
    [pscustomobject]@{Archive=$output;ExecutableBytes=(Get-Item -LiteralPath $source).Length;ArchiveBytes=(Get-Item -LiteralPath $output).Length;SHA256=(Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash}
}finally{if($zip){$zip.Dispose()};if($stream){$stream.Dispose()};if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary}}
