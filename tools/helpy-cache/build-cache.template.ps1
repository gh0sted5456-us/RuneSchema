[CmdletBinding()]
param([Parameter(Position=0)][string]$ZipPath,[string]$Game,[string]$Mapping,[string]$Oodle,[string]$ModId,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem

function Assert-CachePath([string]$Path,[string]$Parent) {
    $resolved=[IO.Path]::GetFullPath($Path);$base=[IO.Path]::GetFullPath($Parent).TrimEnd('\')+'\'
    if(!$resolved.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw 'Cache staging path escaped its intended folder.'}
    for($at=$resolved;$at;){
        if(Test-Path -LiteralPath $at){if((Get-Item -LiteralPath $at -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Redirected cache path refused: $at"}}
        $next=[IO.Path]::GetDirectoryName($at);if($next -eq $at){break};$at=$next
    }
    return $resolved
}
function Get-CacheArchivePlan($Archive) {
    if($Archive.Entries.Count -gt 16384){throw 'ZIP contains too many entries (limit 16384).'}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $selected=[Collections.Generic.List[object]]::new();[long]$total=0
    foreach($entry in $Archive.Entries){
        $name=$entry.FullName.Replace('\','/');$parts=$name.TrimEnd('/').Split('/')
        if(!$name -or $name.StartsWith('/') -or !$seen.Add($name.TrimEnd('/'))){throw 'ZIP has an empty, absolute or duplicate path.'}
        foreach($part in $parts){
            if(!$part -or $part -in @('.','..') -or $part -match '[<>:"|?*\x00-\x1f]' -or $part -match '[. ]$' -or $part -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)'){throw "Unsafe ZIP path: $name"}
        }
        if((($entry.ExternalAttributes -shr 16) -band 61440) -eq 40960 -or ($entry.ExternalAttributes -band 1024)){throw 'ZIP links and reparse entries are unsupported.'}
        if($entry.Length -lt 0 -or $entry.Length -gt 8GB -or $total -gt (16GB-$entry.Length)){throw 'ZIP uncompressed size exceeds the 16 GiB limit.'};$total+=$entry.Length
        if($name.EndsWith('/')){continue}
        $leaf=$parts[-1];$extension=[IO.Path]::GetExtension($leaf).ToLowerInvariant()
        if($extension -in @('.pak','.utoc','.ucas','.usmap') -or $leaf.ToLowerInvariant() -in @('background.png','mod.png','banner.png')){
            $selected.Add([pscustomobject]@{Entry=$entry;Name=$name;Leaf=$leaf;Extension=$extension})
        }
    }
    $paks=@($selected | Where-Object Extension -eq '.pak');if(!$paks.Count){throw 'ZIP has no cooked .pak files.'}
    $stems=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($pak in $paks){if(!$stems.Add([IO.Path]::GetFileNameWithoutExtension($pak.Leaf))){throw 'ZIP contains ambiguous packages with the same name.'}}
    foreach($entry in @($selected | Where-Object Extension -in @('.utoc','.ucas'))){
        $stem=$entry.Name.Substring(0,$entry.Name.LastIndexOf('.'))
        foreach($extension in @('.pak','.utoc','.ucas')){if(!$seen.Contains($stem+$extension)){throw "Incomplete PAK trio: $stem"}}
    }
    $banner=$null
    foreach($leaf in @('background.png','mod.png','banner.png')){
        $bannerMatches=@($selected | Where-Object {$_.Leaf -ieq $leaf});if($bannerMatches.Count -gt 1){throw "ZIP has multiple $leaf files; include one banner."}
        if($bannerMatches.Count){$banner=$bannerMatches[0];if($banner.Entry.Length -gt 16MB){throw 'Background image exceeds 16 MiB.'};break}
    }
    $files=@($selected | Where-Object Extension -in @('.pak','.utoc','.ucas'));if($banner){$files+=@($banner)}
    $maps=@($selected | Where-Object Extension -eq '.usmap');if($maps.Count -gt 1){throw 'Include at most one optional .usmap in the ZIP.'}
    $providedMap=$null;if($maps.Count){$providedMap=$maps[0];if($providedMap.Entry.Length -gt 16MB){throw 'Mapping exceeds 16 MiB.'};$files+=@($providedMap)}
    return [pscustomobject]@{Files=$files;Banner=$banner;Mapping=$providedMap;Bytes=($files | ForEach-Object {$_.Entry.Length} | Measure-Object -Sum).Sum}
}
function Show-CacheProgress([string]$Stage,[int]$Percent) {
    Write-Progress -Id 1 -Activity 'RuneSchema mod cache' -Status $Stage -PercentComplete ([Math]::Min(100,[Math]::Max(0,$Percent)))
}
function Get-CacheExtractionPercent([long]$Done,[long]$Total) {
    return 5+[int](20.0*$Done/[Math]::Max(1.0,[double]$Total))
}
function Invoke-CacheZip {
    if(!$ZipPath){$script:ZipPath=Read-Host 'Drop a mod ZIP here, or enter its full path'}
    $zipFile=(Get-Item -LiteralPath $ZipPath.Trim('"')).FullName
    if([IO.Path]::GetExtension($zipFile) -ine '.zip'){throw 'Choose a ZIP containing cooked PAK files and background.png.'}
    if((Get-Item -LiteralPath $zipFile).Length -gt 8GB){throw 'ZIP exceeds the 8 GiB input limit.'}
    if(!(Get-Command dotnet -ErrorAction SilentlyContinue) -or !(& dotnet --list-sdks | Where-Object {$_ -match '^10\.'})){throw 'Install the .NET 10 SDK, then drop your ZIP again.'}
    if(!$ModId){$script:ModId=[IO.Path]::GetFileNameWithoutExtension($zipFile)}
    if($ModId -notmatch '^[a-zA-Z0-9][a-zA-Z0-9 _.-]{0,127}$' -or $ModId.Contains('..')){throw 'Use the final RuneSchema mod folder name as -ModId (letters, digits, spaces, underscore, dot or dash).'}
    $output=if($OutputDirectory){[IO.Path]::GetFullPath($OutputDirectory)}else{Join-Path ([IO.Path]::GetDirectoryName($zipFile)) 'cache'}
    $parent=[IO.Path]::GetDirectoryName($output);if(!$parent -or [IO.Path]::GetPathRoot($output) -eq $output){throw 'Choose a cache output folder, not a drive root.'}
    $null=Assert-CachePath $output $parent
    $token=[Guid]::NewGuid().ToString('N');$tempParent=[IO.Path]::GetTempPath();$stage=Join-Path $tempParent ('RuneSchema.Cache.'+$token)
    $publish=Join-Path $parent ('.RuneSchema.Cache.'+$token);$backup=$null
    $null=Assert-CachePath $stage $tempParent;$null=Assert-CachePath $publish $parent
    try {
        Write-Host 'RuneSchema mod cache builder' -ForegroundColor Yellow
        Write-Host "ZIP: $zipFile`nMod: $ModId`nOutput: $output"
        Show-CacheProgress 'Checking ZIP contents' 0
        New-Item -ItemType Directory -Path $stage | Out-Null
        $mod=Join-Path $stage 'mod';$paks=Join-Path $mod 'paks';New-Item -ItemType Directory -Path $paks | Out-Null
        $archive=[IO.Compression.ZipFile]::OpenRead($zipFile)
        try {
            $plan=Get-CacheArchivePlan $archive;[long]$done=0
            foreach($file in $plan.Files){
                $destination=if($plan.Banner -and $file.Name -eq $plan.Banner.Name){Join-Path $mod 'background.png'}elseif($file.Extension -eq '.usmap'){Join-Path $stage 'provided.usmap'}else{Join-Path $paks $file.Leaf}
                $null=Assert-CachePath $destination $stage
                $entryStream=$file.Entry.Open();$outputStream=[IO.File]::Open($destination,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
                try {[long]$writtenForEntry=0;$buffer=New-Object byte[] 1048576;while(($count=$entryStream.Read($buffer,0,$buffer.Length)) -gt 0){$writtenForEntry+=$count;if($writtenForEntry -gt $file.Entry.Length){throw 'ZIP entry exceeds its declared size.'};$outputStream.Write($buffer,0,$count);$done+=$count;Show-CacheProgress ('Unpacking '+$file.Leaf) (Get-CacheExtractionPercent $done $plan.Bytes)};if($writtenForEntry -ne $file.Entry.Length){throw 'ZIP entry is truncated.'}}finally{$entryStream.Dispose();$outputStream.Dispose()}
            }
        }finally{$archive.Dispose()}
        # Generated, reviewed builder sources; ZIP contents are never executed.
        @@BUILDER_SOURCES@@
        $project=Join-Path $stage 'HelpyCache.csproj'
        Show-CacheProgress 'Restoring the locked builder dependencies' 25
        & dotnet restore $project --locked-mode --verbosity quiet;if($LASTEXITCODE){throw 'Dependency restore failed; current cache retained.'}
        Show-CacheProgress 'Compiling the cache builder' 30
        & dotnet build $project --no-restore --configuration Release --verbosity quiet;if($LASTEXITCODE){throw 'Builder compilation failed; current cache retained.'}
        $built=Join-Path $stage 'result';$arguments=@('--folder',$mod,'--mod-root',$mod,'--mod-id',$ModId,'--output',$built,'--input-root',[IO.Path]::GetDirectoryName($zipFile),'--progress')
        $previousBundle=Join-Path $output 'offline-helpy.rsmk-cache'
        if(Test-Path -LiteralPath $previousBundle -PathType Leaf){New-Item -ItemType Directory -Path $built | Out-Null;Copy-Item -LiteralPath $previousBundle -Destination $built}
        $providedMapping=$Mapping;if(!$providedMapping -and $plan.Mapping){$providedMapping=Join-Path $stage 'provided.usmap'}
        foreach($entry in @(@('--game',$Game),@('--mapping',$providedMapping),@('--oodle',$Oodle))){if($entry[1]){$arguments+=@($entry[0],$entry[1].Trim('"'))}}
        Show-CacheProgress 'Finding game files and preparing offline mappings automatically' 35
        & dotnet (Join-Path $stage 'bin/Release/net10.0/HelpyCache.dll') @arguments | ForEach-Object {
            if($_ -match '^RS_PROGRESS\|([^|]+)\|(\d+)\|(\d+)$'){
                $phase=$Matches[1];$fraction=[double]$Matches[2]/[Math]::Max(1.0,[double]$Matches[3])
                $percent=switch($phase){'Hashing inputs'{35+15*$fraction};'Reading items'{50+20*$fraction};'Caching pictures'{70+20*$fraction};default{92}}
                Show-CacheProgress "$phase ($($Matches[2]) / $($Matches[3]))" ([int]$percent)
            }else{Write-Host $_}
        }
        if($LASTEXITCODE){throw 'Cooked cache build failed; current cache retained. See the message above.'}
        if(!(Test-Path -LiteralPath (Join-Path $built 'offline-helpy.rsmk-cache'))){throw 'Builder did not produce a cache bundle.'}
        Show-CacheProgress 'Publishing the completed cache folder' 95
        New-Item -ItemType Directory -Path $parent -Force | Out-Null;New-Item -ItemType Directory -Path $publish | Out-Null
        Copy-Item -LiteralPath (Join-Path $built 'offline-helpy.rsmk-cache'),(Join-Path $built 'offline-helpy-report.json') -Destination $publish
        if(Test-Path -LiteralPath (Join-Path $built 'mod.png')){Copy-Item -LiteralPath (Join-Path $built 'mod.png') -Destination $publish}
        if(Test-Path -LiteralPath $output){
            if(!(Test-Path -LiteralPath $output -PathType Container)){throw 'Cache output is an existing file.'}
            $backup=$output+'.previous-'+$token;$null=Assert-CachePath $output $parent;$null=Assert-CachePath $backup $parent
            Move-Item -LiteralPath $output -Destination $backup
        }
        try {$null=Assert-CachePath $publish $parent;$null=Assert-CachePath $output $parent;Move-Item -LiteralPath $publish -Destination $output}
        catch {if($backup -and !(Test-Path -LiteralPath $output)){$null=Assert-CachePath $backup $parent;Move-Item -LiteralPath $backup -Destination $output};throw}
        Show-CacheProgress 'Cache ready' 100;Write-Host "Cache ready: $output" -ForegroundColor Green
        if($backup){Write-Host "Previous cache preserved: $backup"}
    }finally{
        Write-Progress -Id 1 -Activity 'RuneSchema mod cache' -Completed
        if(Test-Path -LiteralPath $stage){$null=Assert-CachePath $stage $tempParent;Remove-Item -LiteralPath $stage -Recurse -Force}
        if(Test-Path -LiteralPath $publish){$null=Assert-CachePath $publish $parent;Remove-Item -LiteralPath $publish -Recurse -Force}
    }
}
if($MyInvocation.InvocationName -ne '.') {try{Invoke-CacheZip}catch{Write-Error $_;exit 1}}
