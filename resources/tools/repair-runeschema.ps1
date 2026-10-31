param([string]$Request,[string]$ProgressFile,[string]$ControlFile)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$script:RepairPlugins=@('RuneSchema.Helpy','RuneSchema.RegistryBridge','RuneSchema.QuestItems','RSNetworking')
function Get-RepairHash([string]$Path) {
    $stream=[IO.File]::OpenRead($Path);$hash=[Security.Cryptography.SHA256]::Create()
    try{return [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-','')}finally{$hash.Dispose();$stream.Dispose()}
}
function Write-RepairLine([string]$Text) {
    [IO.File]::AppendAllText($script:Console,"$Text`r`n",[Text.UTF8Encoding]::new($false))
}
function Write-RepairProgress([string]$State,[string]$Stage,[int]$Percent) {
    $data=@{State=$State;Stage=$Stage;Percent=$Percent;Warnings=@()} | ConvertTo-Json -Compress
    [IO.File]::WriteAllText($ProgressFile,$data,[Text.UTF8Encoding]::new($false))
}
function Test-RepairControl {
    if(!$ControlFile -or !(Test-Path -LiteralPath $ControlFile)){return}
    $control=Get-Content -LiteralPath $ControlFile -Raw | ConvertFrom-Json
    if($control.Action -eq 'stop'){throw 'Repair cancelled before file replacement.'}
    if($control.OwnerPid -and !(Get-Process -Id $control.OwnerPid -ErrorAction SilentlyContinue)){throw 'Helpy exited; repair cancelled.'}
}
function Assert-RepairPath([string]$Path) {
    for($item=[IO.Path]::GetFullPath($Path);$item;$item=[IO.Path]::GetDirectoryName($item)) {
        if((Test-Path -LiteralPath $item) -and ((Get-Item -LiteralPath $item -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Redirected repair path refused: $item"}
    }
}
function Assert-RepairClosed([string]$Root) {
    if(!(Get-Command Get-CimInstance -ErrorAction SilentlyContinue)){Import-Module (Join-Path $PSHOME 'Modules/CimCmdlets/CimCmdlets.psd1') -ErrorAction Stop}
    # Match only this install; private games in other folders are independent.
    $binary=[IO.Directory]::GetParent([IO.Directory]::GetParent([IO.Directory]::GetParent($Root).FullName).FullName).FullName
    foreach($process in @(Get-CimInstance Win32_Process -Filter "Name LIKE 'RSDragonwilds%'" -ErrorAction Stop)) {
        if(!$process.ExecutablePath){throw 'Cannot verify a running Dragonwilds process. Close the game/server before repairing.'}
        if($process.ExecutablePath.StartsWith($binary+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Close this Dragonwilds game/server before repairing.'}
    }
    $main=Join-Path $Root 'dlls/main.dll'
    if(Test-Path -LiteralPath $main){$handle=[IO.File]::Open($main,'Open','ReadWrite','None');$handle.Dispose()}
    elseif($Root.StartsWith('\\')){throw 'Repair a network installation locally on the server when its main DLL is missing; the remote runtime cannot be verified closed.'}
}
function Get-RepairRelease([string]$Version) {
    $headers=@{'User-Agent'='RuneSchema-Helpy-Repair';Accept='application/vnd.github+json'}
    foreach($tag in @($Version,'v'+$Version)) {
        try {return Invoke-RestMethod -Uri ('https://api.github.com/repos/gh0sted5456-us/RuneSchema/releases/tags/'+[Uri]::EscapeDataString($tag)) -Headers $headers -TimeoutSec 30}
        catch {if(!$_.Exception.Response -or [int]$_.Exception.Response.StatusCode -ne 404){throw}}
    }
    throw "No published repair package matches $Version. No files changed. Install a matching published build or wait for its release."
}
function Save-RepairPackage($Asset,[string]$Path) {
    if($Asset.digest -notmatch '^sha256:([0-9a-fA-F]{64})$'){throw 'The release asset has no SHA-256 digest. Repair will not use an unverified package.'}
    $expected=$Matches[1];$uri=[Uri]$Asset.browser_download_url
    if($uri.Scheme -ne 'https' -or $uri.Host -ne 'github.com' -or !$uri.AbsolutePath.StartsWith('/gh0sted5456-us/RuneSchema/releases/download/')){throw 'Unexpected repair download location.'}
    if($Asset.size -le 0 -or $Asset.size -gt 536870912){throw 'Repair package exceeds its download limit.'}
    Write-RepairLine ('Downloading '+$Asset.name)
    Receive-RepairPackage $Asset $Path
    if((Get-Item -LiteralPath $Path).Length -ne $Asset.size -or (Get-RepairHash $Path) -ne $expected){throw 'Repair package checksum did not match. No files changed.'}
    Write-RepairLine ('Verified SHA-256: '+$expected)
}
function Receive-RepairPackage($Asset,[string]$Path) {
    $uri=[Uri]$Asset.browser_download_url
    $request=[Net.HttpWebRequest]::Create($uri);$request.UserAgent='RuneSchema-Helpy-Repair';$request.Timeout=30000;$request.ReadWriteTimeout=30000
    $response=$request.GetResponse()
    try {
        $stream=$response.GetResponseStream();$file=[IO.File]::Create($Path)
        try {$buffer=New-Object byte[] 1048576;[long]$total=0
            while(($read=$stream.Read($buffer,0,$buffer.Length)) -gt 0){Test-RepairControl;$total+=$read;if($total -gt $Asset.size){throw 'Repair download exceeds its declared length.'};$file.Write($buffer,0,$read);Write-RepairProgress 'Running' 'Downloading verified base files' ([int](10+30*$total/$Asset.size))}
        } finally {$file.Dispose();$stream.Dispose()}
    } finally {$response.Dispose()}
}
function Read-RepairLogs([string]$Root) {
    $ue4ss=[IO.Directory]::GetParent([IO.Directory]::GetParent($Root).FullName).FullName
    foreach($path in @((Join-Path $Root 'RuneSchema.log'),(Join-Path $Root 'logs/RuneSchema.log'),(Join-Path $ue4ss 'UE4SS.log'))) {
        if(!(Test-Path -LiteralPath $path -PathType Leaf)){continue}
        try {
        Assert-RepairPath $path
        Write-RepairLine ("Recent log context (not a repair instruction): $path")
        $file=[IO.File]::Open($path,'Open','Read','ReadWrite')
        try {$file.Seek([Math]::Max([long]0,$file.Length-32768),'Begin') | Out-Null;$reader=[IO.StreamReader]::new($file);$tail=$reader.ReadToEnd();$reader.Dispose()} finally {$file.Dispose()}
        $lines=@($tail -split '\r?\n' | Where-Object {$_ -match '(?i)error|warning|DID-NOT-START'} | Select-Object -Last 16)
        if(!$lines.Count){Write-RepairLine '  No recent warning/error lines.'}
        foreach($line in $lines){Write-RepairLine ('  '+$line.Substring(0,[Math]::Min(1024,$line.Length)))}
        } catch {Write-RepairLine ('  Log unavailable: '+$_.Exception.Message)}
    }
}
function Get-RepairEntries([string]$Archive,[string]$Platform,[string]$Stage) {
    $zip=[IO.Compression.ZipFile]::OpenRead($Archive)
    try {
        if($zip.Entries.Count -gt 20000){throw 'Repair archive has too many entries.'}
        $all=@{};foreach($entry in $zip.Entries){
            $name=$entry.FullName.Replace('\','/')
            if($name -match '(^/|:|(^|/)\.\.?(/|$)|[\x00-\x1f]|[. ](/|$))' -or $name -match '(^|/)(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|/|$)' -or (($entry.ExternalAttributes -shr 16) -band 61440) -eq 40960){throw 'Unsafe repair archive path.'}
            if($name.EndsWith('/')){continue}
            if($all.ContainsKey($name)){throw 'Duplicate repair archive path.'};$all[$name]=$entry
        }
        $roots=@($all.Keys | Where-Object {$_ -match '(^|/)RuneSchema/dlls/main\.dll$' -or $_ -eq 'dlls/main.dll'} | ForEach-Object {$_ -replace 'dlls/main\.dll$',''})
        if($roots.Count -gt 1){$roots=@($roots | Where-Object {$_ -match ('Binaries/'+$Platform+'/')})}
        if($roots.Count -ne 1){throw 'The package does not have one unambiguous RuneSchema base.'};$prefix=$roots[0]
        $result=@();[long]$expanded=0
        foreach($name in @($all.Keys | Sort-Object)){
            if(!$name.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){continue};$relative=$name.Substring($prefix.Length)
            $allowed=$relative -eq 'dlls/main.dll' -or $relative -eq 'modmenu.json' -or $relative.StartsWith('scripts/',[StringComparison]::OrdinalIgnoreCase)
            foreach($plugin in $script:RepairPlugins){if($relative.StartsWith('plugins/'+$plugin+'/',[StringComparison]::OrdinalIgnoreCase)){$allowed=$true}}
            if(!$allowed -or $relative -match '(^|/)(settings|mods|cache|logs|profiles|saves|backups)(/|$)' -or $relative -match '\.(ini|log|pdb|bak)$'){continue}
            $entry=$all[$name];$expanded+=$entry.Length;if($entry.Length -gt 536870912 -or $expanded -gt 1073741824 -or $result.Count -ge 4096){throw 'Repair payload exceeds its expansion limit.'}
            $target=Join-Path $Stage $relative;[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$false)
            $result+=@{Relative=$relative;Stage=$target;Hash=(Get-RepairHash $target)}
        }
        foreach($required in @('dlls/main.dll')+@($script:RepairPlugins | ForEach-Object {'plugins/'+$_+'/plugin.json'})) {
            if($required -notin $result.Relative){throw "Required base file missing from package: $required"}
        }
        if(!@($result | Where-Object {$_.Relative -match '^plugins/RuneSchema\.Helpy/(dll/)?RuneSchema\.Helpy\.dll$'}).Count){throw 'Required Helpy bridge DLL missing from package.'}
        foreach($plugin in $script:RepairPlugins){
            $manifest=Get-Content -LiteralPath (Join-Path $Stage ('plugins/'+$plugin+'/plugin.json')) -Raw | ConvertFrom-Json
            if($manifest.Id -ne $plugin){throw 'Bundled plugin identity does not match its folder.'}
        }
        foreach($entry in @($result | Where-Object {$_.Relative -match '\.(utoc|ucas)$'})){
            $stem=$entry.Relative -replace '\.(utoc|ucas)$',''
            foreach($extension in @('.pak','.utoc','.ucas')){if(($stem+$extension) -notin $result.Relative){throw ('Incomplete base package triplet: '+$stem)}}
        }
        return $result
    } finally {$zip.Dispose()}
}
function Install-RepairFile([string]$Stage,[string]$Target) {
    if(Test-Path -LiteralPath $Target){[IO.File]::Replace($Stage,$Target,[NullString]::Value)}else{[IO.File]::Move($Stage,$Target)}
}
function Invoke-RuneSchemaRepair($InputData) {
    $root=[IO.Path]::GetFullPath([string]$InputData.Root);Assert-RepairPath $root
    if([IO.Path]::GetFileName($root) -ne 'RuneSchema' -or [IO.Path]::GetFileName([IO.Path]::GetDirectoryName($root)) -ne 'Mods' -or [IO.Path]::GetFileName([IO.Path]::GetDirectoryName([IO.Path]::GetDirectoryName($root))) -ne 'ue4ss'){throw 'Select a RuneSchema installation inside ue4ss/Mods.'}
    $version=[string]$InputData.Version
    $main=Join-Path $root 'dlls/main.dll'
    if(Test-Path -LiteralPath $main){$reported=[Diagnostics.FileVersionInfo]::GetVersionInfo($main).ProductVersion;if($reported -match '^\d+\.\d+\.\d+\.\d+[a-z]?$'){$version=$reported}}
    if($version -notmatch '^\d+\.\d+\.\d+\.\d+[a-z]?$'){throw 'Cannot determine the installed RuneSchema version. No files changed.'}
    Write-RepairLine "RuneSchema repair: $root ($version)";Write-RepairProgress 'Running' 'Checking logs and runtime' 1
    Assert-RepairClosed $root;Read-RepairLogs $root;Test-RepairControl
    $release=Get-RepairRelease $version
    if($release.draft -or ($release.tag_name -replace '^v','') -ne $version){throw 'The release does not match this installation.'}
    $assets=@($release.assets | Where-Object {$_.name -match ('^RuneSchema[-.]'+[regex]::Escape($version)+'\.zip$')})
    if($assets.Count -ne 1){throw 'This release has no unambiguous full RuneSchema repair ZIP.'}
    $archive=Join-Path $script:Job 'package.zip';Save-RepairPackage $assets[0] $archive
    $platform=if($root -match '(?i)[\\/]WinGDK[\\/]'){'WinGDK'}else{'Win64'}
    $entries=@(Get-RepairEntries $archive $platform (Join-Path $script:Job 'stage'));$changed=@();$count=0
    foreach($entry in $entries){
        Test-RepairControl;$target=Join-Path $root $entry.Relative;Assert-RepairPath $target;$entry.Target=$target;$entry.Existed=Test-Path -LiteralPath $target -PathType Leaf
        $entry.Before=if($entry.Existed){Get-RepairHash $target}else{''}
        if($entry.Before -eq $entry.Hash){Write-RepairLine ('OK      '+$entry.Relative)}else{Write-RepairLine ('REPAIR  '+$entry.Relative);$changed+=$entry}
        $count++;Write-RepairProgress 'Running' 'Scanning base files' ([int](40+30*$count/$entries.Count))
    }
    Write-RepairLine ("Scanned $($entries.Count) base files; $($changed.Count) need repair.")
    if(!$changed.Count){Write-RepairProgress 'Succeeded' 'All base files are valid' 100;return}
    Test-RepairControl;Assert-RepairClosed $root
    $applied=@()
    try {
    # Preflight every target and make all backups before the first replacement.
    foreach($entry in $changed){
        Assert-RepairPath $entry.Target
        if($entry.Existed){$handle=[IO.File]::Open($entry.Target,'Open','ReadWrite','None');$handle.Dispose();$backup=Join-Path $script:Job ('backup/'+$entry.Relative);[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($backup))|Out-Null;[IO.File]::Copy($entry.Target,$backup);$entry.Backup=$backup
            if((Get-RepairHash $backup) -ne $entry.Before){throw 'A base file changed during repair. Retry with the game closed.'}}
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($entry.Target))|Out-Null
        $entry.Pending=$entry.Target+'.repair-'+[Guid]::NewGuid().ToString('N');[IO.File]::Copy($entry.Stage,$entry.Pending)
    }
        Test-RepairControl;Assert-RepairClosed $root
        foreach($entry in $changed){
            Assert-RepairPath $entry.Target
            if((Test-Path -LiteralPath $entry.Target) -ne $entry.Existed){throw 'Base file state changed during repair.'}
            if($entry.Existed -and (Get-RepairHash $entry.Target) -ne $entry.Before){throw 'Base file changed during repair.'}
            Install-RepairFile $entry.Pending $entry.Target;$applied+=,$entry
            if((Get-RepairHash $entry.Target) -ne $entry.Hash){throw 'A replaced base file failed verification.'}
            Write-RepairLine ('REPLACED '+$entry.Relative)
        }
    } catch {
        $reason=$_.Exception.Message;[array]::Reverse($applied)
        foreach($entry in $applied){try{if($entry.Existed){$restore=$entry.Target+'.rollback-'+[Guid]::NewGuid().ToString('N');[IO.File]::Copy($entry.Backup,$restore);Install-RepairFile $restore $entry.Target}else{[IO.File]::Delete($entry.Target)};Write-RepairLine ('RESTORED '+$entry.Relative)}catch{Write-RepairLine ('ROLLBACK NEEDS ATTENTION: '+$entry.Target+'; backup: '+$entry.Backup)}}
        throw $reason
    } finally {foreach($entry in $changed){if($entry.Pending -and [IO.File]::Exists($entry.Pending)){[IO.File]::Delete($entry.Pending)}}}
    $entries | Select-Object Relative,Hash,Before | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $script:Job 'receipt.json') -Encoding UTF8
    Write-RepairLine ('Repair complete. Previous files retained in '+(Join-Path $script:Job 'backup'))
    Write-RepairProgress 'Succeeded' ('Repaired '+$changed.Count+' base files') 100
}
if($MyInvocation.InvocationName -ne '.') {
    $script:Job=[IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Request));$script:Console=Join-Path $script:Job 'console.log'
    try {Invoke-RuneSchemaRepair (Get-Content -LiteralPath $Request -Raw | ConvertFrom-Json);exit 0}
    catch {Write-RepairLine ('ERROR: '+$_.Exception.Message);Write-RepairProgress 'Failed' $_.Exception.Message 0;exit 1}
}
