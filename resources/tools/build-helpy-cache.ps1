[CmdletBinding()]
param([Parameter(Position=0)][string]$ZipPath,[string]$ModDirectory,[string]$Game,[string]$Mapping,[string]$Oodle,[string]$ModId,[string]$OutputDirectory,[string]$ProgressFile,[string]$ControlFile,[switch]$InstallDotNet,[string]$CreationRequest,[string]$PackagePath,[string]$Platform='Win64')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem

function Write-CacheStatus([string]$State,[string]$Stage,[int]$Percent) {
    if($ProgressFile){
        $null=Assert-CachePath $ProgressFile ([IO.Path]::GetDirectoryName($ProgressFile))
        $warnings=@($script:CreationWarnings | Select-Object -First 16 | ForEach-Object {if($_){$value=[string]$_;$value.Substring(0,[Math]::Min(256,$value.Length))}})
        $text=@{State=$State;Stage=$Stage.Substring(0,[Math]::Min(1024,$Stage.Length));Percent=$Percent;Warnings=$warnings} | ConvertTo-Json -Compress
        [IO.File]::WriteAllText($ProgressFile,$text,[Text.UTF8Encoding]::new($false))
    }
}
function Check-CacheControl {
    if(!$ControlFile){return}
    while($true){
        if((Get-Item -LiteralPath $ControlFile).Length -gt 16384){throw 'Invalid cache control file.'}
        $control=[IO.File]::ReadAllText($ControlFile) | ConvertFrom-Json
        if($control.Action -eq 'stop' -or ($control.OwnerPid -and !(Get-Process -Id $control.OwnerPid -ErrorAction SilentlyContinue))){throw [OperationCanceledException]::new('Cache build stopped; previous cache retained.')}
        if($control.Action -eq 'run'){return}
        if($control.Action -ne 'pause'){throw 'Invalid cache control action.'}
        Write-CacheStatus 'Paused' 'Paused - resume or stop when ready' $script:CachePercent
        Start-Sleep -Milliseconds 200
    }
}
# Private, checksum-verified SDK setup for explicit authoring jobs only.
function Get-CacheSdkFile($Metadata) {
    $version=$Metadata.'latest-sdk'
    if($version -notmatch '^10\.0\.\d+$'){throw 'Unsupported .NET SDK release metadata.'}
    $sdk=@($Metadata.releases | ForEach-Object {@($_.sdk)+@($_.sdks)} | Where-Object {$_.version -eq $version}) | Select-Object -First 1
    $file=@($sdk.files | Where-Object {$_.rid -eq 'win-x64' -and $_.url.EndsWith('.zip')}) | Select-Object -First 1
    $uri=if($file){[Uri]$file.url}else{$null}
    if(!$uri -or $uri.Scheme -ne 'https' -or $uri.Host -notin @('builds.dotnet.microsoft.com','download.visualstudio.microsoft.com') -or $uri.UserInfo -or $file.hash -notmatch '^[a-fA-F0-9]{128}$'){throw 'Invalid Microsoft SDK download metadata.'}
    return @{Version=$version;Url=$uri.AbsoluteUri;Hash=$file.hash}
}
function Resolve-CacheDotnet([string]$Stage) {
    $command=Get-Command dotnet -ErrorAction SilentlyContinue
    if($command){$candidate=if($command.CommandType -eq 'Function'){$command.Name}else{$command.Source};if(& $candidate --list-sdks | Where-Object {$_ -match '^10\.'}){return $candidate}}
    $sdkHome=Join-Path $env:LOCALAPPDATA 'RSDragonwilds/RuneSchema/core/tools/dotnet'
    $null=Assert-CachePath $sdkHome ([IO.Path]::GetDirectoryName($sdkHome))
    foreach($folder in @(Get-ChildItem -LiteralPath $sdkHome -Directory -ErrorAction SilentlyContinue | Where-Object {$_.Name -match '^10\.0\.\d+$'} | Sort-Object Name -Descending)){
        $candidate=Join-Path $folder.FullName 'dotnet.exe';$null=Assert-CachePath $candidate $sdkHome
        if((Test-Path -LiteralPath $candidate) -and (& $candidate --list-sdks | Where-Object {$_ -match '^10\.'})){return $candidate}
    }
    if(!$InstallDotNet){throw 'The .NET 10 SDK is needed. Enable automatic .NET setup under Tools > Authoring tools > Dependencies, or install the SDK yourself.'}
    Show-CacheProgress 'Reading Microsoft .NET release metadata' 0
    [Net.ServicePointManager]::SecurityProtocol=[Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $metadata=Invoke-RestMethod -Uri 'https://builds.dotnet.microsoft.com/dotnet/release-metadata/10.0/releases.json' -TimeoutSec 30
    $sdk=Get-CacheSdkFile $metadata;$zip=Join-Path $Stage 'dotnet-sdk.zip'
    $request=[Net.HttpWebRequest]::Create($sdk.Url);$request.AllowAutoRedirect=$false;$request.Timeout=30000;$request.ReadWriteTimeout=30000
    $response=$request.GetResponse()
    try{
        if($response.ContentLength -gt 1GB){throw 'SDK download exceeds 1 GiB.'}
        $downloadInput=$response.GetResponseStream();$downloadOutput=[IO.File]::Create($zip)
        try{[long]$done=0;$buffer=New-Object byte[] 1048576
            while(($count=$downloadInput.Read($buffer,0,$buffer.Length)) -gt 0){
                $done+=$count;if($done -gt 1GB){throw 'SDK download exceeds 1 GiB.'}
                $downloadOutput.Write($buffer,0,$count);Show-CacheProgress ('Downloading authoring .NET SDK: '+[int]($done/1MB)+' MiB') ([int](15*$done/[Math]::Max([double]$done,[double]$response.ContentLength)))
            }
        }finally{$downloadOutput.Dispose();$downloadInput.Dispose()}
    }finally{$response.Dispose()}
    Show-CacheProgress 'Verifying the Microsoft SDK checksum' 15
    if((Get-FileHash -LiteralPath $zip -Algorithm SHA512).Hash -ine $sdk.Hash){throw 'Microsoft SDK checksum failed. Download was not installed.'}
    $destination=Join-Path $sdkHome $sdk.Version;$stage=Join-Path $sdkHome ('.i-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
    $null=Assert-CachePath $destination $sdkHome;$null=Assert-CachePath $stage $sdkHome
    try{
        Show-CacheProgress 'Installing the verified authoring SDK' 15
        New-Item -ItemType Directory -Path $sdkHome -Force | Out-Null;New-Item -ItemType Directory -Path $stage | Out-Null
        $archive=[IO.Compression.ZipFile]::OpenRead($zip)
        try{[long]$total=0;$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
            if($archive.Entries.Count -gt 100000){throw 'SDK archive has too many entries.'}
            $entryCount=0;foreach($entry in $archive.Entries){
                Check-CacheControl;$total+=$entry.Length
                if((++$entryCount % 100) -eq 0){Show-CacheProgress 'Installing the verified authoring SDK' ([int](15+8.0*$entryCount/$archive.Entries.Count))}
                if($entry.Length -lt 0 -or $total -gt 4GB -or !$seen.Add($entry.FullName) -or (($entry.ExternalAttributes -shr 16) -band 61440) -eq 40960){throw 'Invalid SDK archive entry.'}
                $path=Assert-CachePath (Join-Path $stage $entry.FullName) $stage
                if($entry.FullName.EndsWith('/')){New-Item -ItemType Directory -Path $path -Force | Out-Null;continue}
                New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($path)) -Force | Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$path)
            }
        }finally{$archive.Dispose()}
        $candidate=Join-Path $stage 'dotnet.exe'
        if(!(Test-Path -LiteralPath $candidate) -or !(& $candidate --list-sdks | Where-Object {$_ -match '^10\.'})){throw 'The installed authoring SDK did not pass its check.'}
        Check-CacheControl
        if(Test-Path -LiteralPath $destination){$existing=Join-Path $destination 'dotnet.exe';if((Test-Path -LiteralPath $existing) -and (& $existing --list-sdks | Where-Object {$_ -match '^10\.'})){return $existing};throw 'An SDK installation already occupies this version folder; it was retained.'}
        Move-Item -LiteralPath $stage -Destination $destination
        return (Join-Path $destination 'dotnet.exe')
    }finally{if(Test-Path -LiteralPath $stage){$null=Assert-CachePath $stage $sdkHome;Remove-Item -LiteralPath $stage -Recurse -Force}}
}


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
    $script:CachePercent=$Percent;Check-CacheControl
    Write-CacheStatus 'Running' $Stage $Percent
    Write-Progress -Id 1 -Activity 'RuneSchema mod cache' -Status $Stage -PercentComplete ([Math]::Min(100,[Math]::Max(0,$Percent)))
}
function Get-CacheExtractionPercent([long]$Done,[long]$Total) {
    return 5+[int](20.0*$Done/[Math]::Max(1.0,[double]$Total))
}
function Invoke-CacheZip {
    $env:DOTNET_CLI_TELEMETRY_OPTOUT='1'
    $env:DOTNET_GENERATE_ASPNET_CERTIFICATE='false';$env:MSBUILDDISABLENODEREUSE='1';$env:DOTNET_CLI_DO_NOT_USE_MSBUILD_SERVER='1'
    if($ModDirectory -and $ZipPath){throw 'Choose a mod directory or a ZIP, not both.'}
    if($CreationRequest){
        if($ModDirectory -or $ZipPath -or !$PackagePath){throw 'Creation needs its request and a new output ZIP.'}
        if((Get-Item -LiteralPath $CreationRequest).Length -gt 65536){throw 'Creation request exceeds 64 KiB.'}
        $creation=[IO.File]::ReadAllText($CreationRequest) | ConvertFrom-Json
        $script:ModId=$creation.Metadata.ModID;$inputRoot=[IO.Path]::GetDirectoryName($CreationRequest)
    }elseif($ModDirectory){
        $mod=(Get-Item -LiteralPath $ModDirectory).FullName
        if(!(Test-Path -LiteralPath $mod -PathType Container)){throw 'Mod folder is missing.'}
        $null=Assert-CachePath $mod ([IO.Path]::GetDirectoryName($mod));$inputRoot=$mod
        if(!$ModId){$script:ModId=[IO.Path]::GetFileName($mod)}
    }else{
        if(!$ZipPath){$script:ZipPath=Read-Host 'Drop a mod ZIP here, or enter its full path'}
        $zipFile=(Get-Item -LiteralPath $ZipPath.Trim('"')).FullName
        if([IO.Path]::GetExtension($zipFile) -ine '.zip'){throw 'Choose a ZIP containing cooked PAK files and background.png.'}
        if((Get-Item -LiteralPath $zipFile).Length -gt 8GB){throw 'ZIP exceeds the 8 GiB input limit.'}
        $inputRoot=[IO.Path]::GetDirectoryName($zipFile)
        if(!$ModId){$script:ModId=[IO.Path]::GetFileNameWithoutExtension($zipFile)}
    }
    if($ModId -notmatch '^[a-zA-Z0-9][a-zA-Z0-9 _.-]{0,127}$' -or $ModId.Contains('..')){throw 'Use the final RuneSchema mod folder name as -ModId (letters, digits, spaces, underscore, dot or dash).'}
    $output=if($OutputDirectory){[IO.Path]::GetFullPath($OutputDirectory)}else{Join-Path $inputRoot 'cache'}
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
        if($ProgressFile){Start-Transcript -LiteralPath (Join-Path ([IO.Path]::GetDirectoryName($ProgressFile)) 'build.log') -Force | Out-Null}
        Show-CacheProgress 'Checking authoring .NET SDK' 0
        $script:DotnetCommand=Resolve-CacheDotnet $stage
        $plan=$null
        if(!$ModDirectory -and !$CreationRequest){
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
        }
        # Generated, reviewed builder sources; ZIP contents are never executed.
        $builderSource=@'
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Exe</OutputType>
    <TargetFramework>net10.0</TargetFramework>
    <ImplicitUsings>enable</ImplicitUsings>
    <Nullable>enable</Nullable>
    <RestorePackagesWithLockFile>true</RestorePackagesWithLockFile>
  </PropertyGroup>
  <ItemGroup>
    <PackageReference Include="CUE4Parse" Version="1.2.2.202609" />
    <PackageReference Include="AssetRipper.TextureDecoder" Version="2.6.2" />
    <PackageReference Include="SkiaSharp" Version="2.88.9" />
    <PackageReference Include="Microsoft.Bcl.Memory" Version="10.0.10" />
    <PackageReference Include="SharpCompress" Version="1.0.0" />
  </ItemGroup>
</Project>
'@
[IO.File]::WriteAllText((Join-Path $stage 'HelpyCache.csproj'), $builderSource, [Text.UTF8Encoding]::new($false))
$builderSource=@'
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using CUE4Parse.Compression;
using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.AssetRegistry;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Versions;
using CUE4Parse.UE4.VirtualFileSystem;
using CUE4Parse.UE4.Objects.UObject;
using AssetRipper.TextureDecoder.Bc;
using AssetRipper.TextureDecoder.Rgb.Formats;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Serilog;
using SkiaSharp;

return CacheTool.Run(args);

static class CacheTool
{
    static bool EmitProgress;
    static long LastProgress;
    static string LastPhase = "";
    static string? ControlFile;
    static void CheckControl()
    {
        if(ControlFile is null)return;
        var invalid=0;var paused=false;
        while(true){
            Need(new FileInfo(ControlFile).Length<=16384,"Invalid cache control file.");
            JObject control;try{control=JObject.Parse(File.ReadAllText(ControlFile));}catch(JsonException){if(++invalid>10)throw;Thread.Sleep(100);continue;}
            var action=control.Value<string>("Action");
            var owner=control.Value<int?>("OwnerPid");
            if(owner is not null){try{using var process=Process.GetProcessById(owner.Value);if(process.HasExited)action="stop";}catch(ArgumentException){action="stop";}}
            if(action=="stop")throw new OperationCanceledException("Cache build stopped; previous cache retained.");
            if(action=="run")return;
            Need(action=="pause","Invalid cache control action.");if(!paused){Console.WriteLine("RS_PAUSED");paused=true;}Thread.Sleep(200);
        }
    }
    static void Progress(string phase, int done, int total)
    {
        CheckControl();
        var now = Environment.TickCount64;
        if (!EmitProgress || (phase == LastPhase && done != total && now - LastProgress < 100)) return;
        LastPhase = phase; LastProgress = now;
        Console.WriteLine($"RS_PROGRESS|{phase}|{done}|{total}");
    }
    const long MaxBundle = 256L * 1024 * 1024;
    static readonly HashSet<string> NativeExtensions = new(StringComparer.OrdinalIgnoreCase) { ".pak", ".utoc", ".ucas", ".uplugin" };
    static readonly HashSet<string> OwnerExtensions = new(StringComparer.OrdinalIgnoreCase) { ".pak", ".utoc", ".ucas", ".uplugin", ".json", ".jsonc", ".ini", ".lua", ".dll", ".exe" };
    static readonly HashSet<string> SkipFolders = new(StringComparer.OrdinalIgnoreCase) { "runtime", "cache", "saved", "logs", "exports", "settings" };
    static readonly EnumerationOptions Walk = new() { RecurseSubdirectories = true, AttributesToSkip = FileAttributes.ReparsePoint, IgnoreInaccessible = false };
    static string Hash(byte[] data) => Convert.ToHexStringLower(SHA256.HashData(data));
    static string Json(JToken value) => value.ToString(Formatting.None);
    static void Need(bool value, string message) { if (!value) throw new InvalidDataException(message); }
    static string Full(string path) => Path.GetFullPath(path);
    static bool Within(string path, string parent) => Full(path).StartsWith(Full(parent).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase);
    static void Safe(string path)
    {
        for (var current = Full(path); !string.IsNullOrEmpty(current); current = Path.GetDirectoryName(current))
            if (File.Exists(current) || Directory.Exists(current)) Need((File.GetAttributes(current) & FileAttributes.ReparsePoint) == 0, "Links/junctions are not supported: " + current);
    }
    static string[] Files(string root, HashSet<string> extensions) => Directory.Exists(root)
        ? Directory.EnumerateFiles(root, "*", Walk).Where(p => (extensions.Contains(Path.GetExtension(p)) || ReferenceEquals(extensions, OwnerExtensions) && new[] { "enabled.txt", "mods.txt", "runeschema.txt" }.Contains(Path.GetFileName(p).ToLowerInvariant())) && !Path.GetRelativePath(root, p).Split(Path.DirectorySeparatorChar).SkipLast(1).Any(SkipFolders.Contains)).Order(StringComparer.Ordinal).ToArray() : [];
    static void Closed(string game)
    {
        foreach (var process in Process.GetProcesses()) using (process)
            if (process.ProcessName.StartsWith("RSDragonwilds", StringComparison.OrdinalIgnoreCase))
            {
                string? executable;
                try { executable = process.MainModule?.FileName; } catch { throw new IOException("Cannot identify a running Dragonwilds process. Close that runtime before scanning."); }
                Need(executable is not null && !Within(executable, game), "Close this Dragonwilds client/server before building its cache. Another installation can remain open.");
            }
    }
    static string Source(string path)
    {
        var text = path.ToLowerInvariant(); Need(text.StartsWith('/') && !text.Contains(".."), "Invalid asset path");
        var end = text.IndexOf('/', 1); Need(end > 1, "Invalid source path");
        foreach (var prefix in new[] { "/game/mods/", "/game/runeschema/", "/game/gameplay/mods/" })
            if (text.StartsWith(prefix, StringComparison.Ordinal)) { var next = text.IndexOf('/', prefix.Length); if (next >= 0) end = next; break; }
        var source = text[1..end]; Need(source.Length <= 192 && Regex.IsMatch(source, "^[a-z0-9_./-]+$"), "Unsupported source name"); return source;
    }
    static string Asset(string package, string? name = null)
    {
        var text = package.Replace('\\', '/'); while (text.StartsWith("../", StringComparison.Ordinal)) text = text[3..];
        var content = text.IndexOf("/Content/", StringComparison.OrdinalIgnoreCase); Need(content > 0, "Unrecognized cooked package: " + text);
        var prefix = text[..content]; var root = prefix.Equals("RSDragonwilds", StringComparison.OrdinalIgnoreCase) ? "Game" : prefix.Equals("Engine", StringComparison.OrdinalIgnoreCase) ? "Engine" : prefix.Split('/')[^1];
        var tail = text[(content + 9)..]; tail = tail[..tail.LastIndexOf('.')];
        var result = "/" + root + "/" + tail + "." + (name ?? tail.Split('/')[^1]);
        Need(result.Length <= 2048 && !result.Contains("..") && !result.Contains(':') && !result.Any(char.IsControl), "Unsupported asset path"); return result;
    }
    static string Text(JToken? token)
    {
        if (token is null) return "";
        if (token.Type == JTokenType.String) return token.Value<string>() ?? "";
        if (token is JObject obj) foreach (var field in new[] { "LocalizedString", "SourceString", "Text", "Name" })
            if (obj[field]?.Type == JTokenType.String) return obj.Value<string>(field) ?? "";
        return "";
    }
    static string First(JObject props, params string[] fields) => fields.Select(f => Text(props[f])).FirstOrDefault(t => t.Length > 0) ?? "";
    static string Reference(JToken? token, IReadOnlyDictionary<string, string> paths)
    {
        if (token is null) return "";
        var value = token.Type == JTokenType.String ? token.Value<string>() ?? "" : Text(token["AssetPathName"] ?? token["ObjectPath"]);
        if (value.Contains('\'')) value = value.Split('\'')[1];
        if (value.StartsWith('/')) return value.Contains('.') && !value.EndsWith(".0", StringComparison.Ordinal) ? value : "";
        var dot = value.LastIndexOf('.'); if (dot >= 0) value = value[..dot];
        return paths.GetValueOrDefault(value.ToLowerInvariant(), "");
    }
    static string InferModRoot(string folder)
    {
        for (var current = new DirectoryInfo(Full(folder)); current is not null; current = current.Parent)
            if (current.Name.Equals("paks", StringComparison.OrdinalIgnoreCase) && current.Parent is not null) return current.Parent.FullName;
        return Full(folder);
    }
    static Dictionary<string, (long Size, long Stamp)> Stamps(IEnumerable<string> files) => files.Distinct(StringComparer.OrdinalIgnoreCase).ToDictionary(p => p, p => { var f = new FileInfo(p); return (f.Length, f.LastWriteTimeUtc.Ticks); }, StringComparer.OrdinalIgnoreCase);
    static void Unchanged(Dictionary<string, (long Size, long Stamp)> before, IEnumerable<string> files)
    {
        var after = Stamps(files); Need(before.Count == after.Count && before.All(p => after.TryGetValue(p.Key, out var stamp) && stamp == p.Value), "Inputs changed while building; previous cache retained. Close the game/updater and retry.");
    }
    static string Fingerprint(string prefix, IEnumerable<KeyValuePair<string, string>> files)
    {
        using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256); hash.AppendData(Encoding.UTF8.GetBytes(prefix));
        foreach (var entry in files.OrderBy(p => p.Key, StringComparer.Ordinal).ThenBy(p => p.Value, StringComparer.Ordinal)) hash.AppendData(Encoding.UTF8.GetBytes(entry.Key + "|" + entry.Value + "\n"));
        return Convert.ToHexStringLower(hash.GetHashAndReset());
    }
    static byte[] Pixels(SKBitmap original, bool banner = false)
    {
        Need(original.Width > 0 && original.Height > 0 && original.Width <= 8192 && original.Height <= 8192, "Image extent outside 1..8192");
        var scale = Math.Min(1.0, 64.0 / Math.Max(original.Width, original.Height));
        int width = banner ? 256 : Math.Max(1, (int)(original.Width * scale)), height = banner ? 144 : Math.Max(1, (int)(original.Height * scale));
        using var bitmap = new SKBitmap(new SKImageInfo(width, height, SKColorType.Rgba8888, SKAlphaType.Unpremul));
        using (var canvas = new SKCanvas(bitmap))
        {
            canvas.Clear(SKColors.Transparent);
            if (banner) { var fit = Math.Max(256f / original.Width, 144f / original.Height); var w = original.Width * fit; var h = original.Height * fit; canvas.DrawBitmap(original, new SKRect((256 - w) / 2, (144 - h) / 2, (256 + w) / 2, (144 + h) / 2)); }
            else canvas.DrawBitmap(original, new SKRect(0, 0, width, height));
        }
        var rgba = new byte[width * height * 4]; Marshal.Copy(bitmap.GetPixels(), rgba, 0, rgba.Length);
        if (banner) return rgba;
        using var output = new MemoryStream(); using var writer = new BinaryWriter(output); writer.Write(Encoding.ASCII.GetBytes("RSI3")); writer.Write(width); writer.Write(height); writer.Write(rgba.Length); writer.Write(rgba); return output.ToArray();
    }
    static SKBitmap Decode(UTexture2D texture)
    {
        Need(texture.PlatformData?.VTData is null, "Virtual textures require Helpy's runtime capture");
        var mip = texture.GetMip(texture.GetFirstMipIndex()) ?? throw new InvalidDataException("Texture has no readable mip");
        int width = mip.SizeX, height = mip.SizeY;
        Need(width is > 0 and <= 8192 && height is > 0 and <= 8192, "Texture extent outside 1..8192");
        var input = mip.BulkData?.Data ?? throw new InvalidDataException("Texture has no pixel payload");
        var format = texture.Format.ToString(); var color = SKColorType.Rgba8888; byte[] pixels;
        var blockBytes = format is "PF_DXT1" ? 8 : 16;
        if (format is "PF_DXT1" or "PF_DXT3" or "PF_DXT5" or "PF_BC7") Need(input.Length >= ((width + 3) / 4) * ((height + 3) / 4) * blockBytes, "Truncated block-compressed texture");
        switch (format)
        {
            case "PF_DXT1": Bc1.Decompress<ColorRGBA<byte>, byte>(input, width, height, out pixels); break;
            case "PF_DXT3": Bc2.Decompress<ColorRGBA<byte>, byte>(input, width, height, out pixels); break;
            case "PF_DXT5": Bc3.Decompress<ColorRGBA<byte>, byte>(input, width, height, out pixels); break;
            case "PF_BC7": Bc7.Decompress<ColorRGBA<byte>, byte>(input, width, height, out pixels); break;
            case "PF_B8G8R8A8": pixels = input; color = SKColorType.Bgra8888; break;
            case "PF_R8G8B8A8": pixels = input; break;
            case "PF_G8":
                Need(input.Length == width * height, "Truncated grayscale texture"); pixels = new byte[width * height * 4];
                for (var i = 0; i < input.Length; ++i) { pixels[i * 4] = pixels[i * 4 + 1] = pixels[i * 4 + 2] = input[i]; pixels[i * 4 + 3] = 255; } break;
            default: throw new InvalidDataException("Unsupported texture format " + format + "; use Helpy runtime capture");
        }
        Need(pixels.Length == width * height * 4, "Unsupported decoded pixel layout");
        var bitmap = new SKBitmap(new SKImageInfo(width, height, color, SKAlphaType.Unpremul)); Marshal.Copy(pixels, 0, bitmap.GetPixels(), pixels.Length); return bitmap;
    }
    public static int Run(string[] args)
    {
        try
        {
            if (args.Contains("--self-test")) { SelfTest(); return 0; }
            if (args.Contains("--help")) { Console.WriteLine("Helpy offline cache builder (Windows, .NET 10)\n[--game GAME/RSDragonwilds] [--mapping matching.usmap] [--oodle local-decoder.dll]\n[--folder PAK-FOLDER] [--mod-root MOD-ROOT] [--mod-id MOD-ID] [--output DIRECTORY]\n[--base-game [--include-mods]] [--locale en-US] [--input-root ORIGINAL-FOLDER]\nAutomatically finds an inactive installation, reads a supplied/platform .usmap or downloads RSDW Archive's snapshot, and prepares the pinned decoder. Mod mode is the default.\nNo game, runtime mapping, save, PAK or installed Helpy cache is modified. Output is presentation only."); return 0; }
            Need(OperatingSystem.IsWindows(), "This release supports Windows/Git Bash only (native texture decoder and game process checks).");
            var options = new Dictionary<string, string>();
            foreach (var (arg, index) in args.Select((s, i) => (s, i)))
                if (arg.StartsWith("--", StringComparison.Ordinal))
                {
                    Need(new[] { "--game", "--mapping", "--oodle", "--folder", "--mod-root", "--mod-id", "--output", "--base-game", "--include-mods", "--locale", "--progress", "--input-root", "--check-inputs", "--control-file", "--prepare-creation", "--package-creation", "--platform" }.Contains(arg), "Unknown option " + arg);
                    Need(!options.ContainsKey(arg), "Repeated option " + arg);
                    if (arg is "--base-game" or "--include-mods" or "--progress" or "--check-inputs") options[arg] = "true";
                    else { Need(index + 1 < args.Length && !args[index + 1].StartsWith("--", StringComparison.Ordinal), "Missing value for " + arg); options[arg] = args[index + 1]; }
                }
            EmitProgress = options.ContainsKey("--progress");
            ControlFile=options.GetValueOrDefault("--control-file");CheckControl();
            if(options.TryGetValue("--prepare-creation",out var request)){CreationArchive.Prepare(request,options["--output"],Progress,CheckControl);return 0;}
            if(options.TryGetValue("--package-creation",out var prepared)){CreationArchive.Package(prepared,options["--platform"],options["--output"],options["--mod-id"],Progress,CheckControl);return 0;}
            var inputRoot = Full(options.GetValueOrDefault("--input-root", Directory.GetCurrentDirectory())); Safe(inputRoot);
            var game = AuthoringInputs.Game(options.GetValueOrDefault("--game"), inputRoot, Closed); Safe(game);
            var oodle = AuthoringInputs.Decoder(options.GetValueOrDefault("--oodle"), Safe); OodleHelper.Initialize(oodle);
            Need(OodleHelper.Instance is not null, "Oodle decoder could not be initialized.");
            var mapping = AuthoringInputs.Mapping(options.GetValueOrDefault("--mapping"), inputRoot, game, Safe);
            Console.WriteLine("Offline mapping: " + mapping);
            if (options.ContainsKey("--check-inputs")) { Console.WriteLine("Authoring inputs ready; no PAK scan or cache write performed."); return 0; }
            var locale = options.GetValueOrDefault("--locale", "en-US"); Need(locale == "en-US", "Offline source-text capture currently supports en-US only; it must not be labelled as another translation.");
            var folder = Full(options.GetValueOrDefault("--folder", Directory.GetCurrentDirectory()));
            var baseMode = options.ContainsKey("--base-game"); Need(baseMode || !options.ContainsKey("--include-mods"), "--include-mods requires --base-game");
            var modRoot = Full(options.GetValueOrDefault("--mod-root", InferModRoot(folder)));
            var modId = options.GetValueOrDefault("--mod-id", new DirectoryInfo(modRoot).Name).ToLowerInvariant();
            Need(Regex.IsMatch(modId, "^[a-z0-9][a-z0-9 _.-]{0,127}$") && !modId.Contains(".."), "Use the final RuneSchema mod folder name as --mod-id");
            var output = Full(options.GetValueOrDefault("--output", baseMode ? Path.Combine(folder, "base-game-cache") : Path.Combine(modRoot, "cache"))); Safe(output); Safe(modRoot);
            if (!baseMode) Need(Directory.Exists(Path.Combine(modRoot, "paks"))||Directory.Exists(Path.Combine(modRoot,"ue4ss")), "Mod root must contain paks/ or ue4ss/. --mod-root selects it.");
            var nativeRoots = new[] { Path.Combine(game, "Content", "Paks"), Path.Combine(game, "Plugins"), Path.Combine(game, "GameFeatures"), Path.Combine(Directory.GetParent(game)!.FullName, "Engine", "Plugins") }.Where(Directory.Exists).ToArray();
            var nativeFiles = nativeRoots.SelectMany(r => Files(r, NativeExtensions)).Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
            Need(nativeFiles.Length is > 0 and <= 100000, "Native source file count outside 1..100000");
            var mods = new List<(string Root, string Id, string Output)>();
            if (!baseMode) mods.Add((modRoot, modId, output));
            else if (options.ContainsKey("--include-mods"))
            {
                var installed = new[] { "Win64", "WinGDK" }.Select(l => Path.Combine(game, "Binaries", l, "ue4ss", "Mods", "RuneSchema", "mods")).FirstOrDefault(Directory.Exists);
                Need(installed is not null, "No RuneSchema/mods folder found");
                foreach (var mod in Directory.EnumerateDirectories(installed!).Order(StringComparer.Ordinal)) if (Directory.Exists(Path.Combine(mod, "paks")))
                    mods.Add((mod, Path.GetFileName(mod).ToLowerInvariant(), Path.Combine(output, "mods", Path.GetFileName(mod), "cache")));
            }
            Need(mods.Count <= 512, "Too many mods");
            foreach (var mod in mods) { Safe(mod.Root); Safe(mod.Output); Need(!Within(mod.Output, mod.Root) || Full(mod.Output).Equals(Path.Combine(mod.Root, "cache"), StringComparison.OrdinalIgnoreCase), "Output inside the mod must be its cache/ folder"); }
            var ownerFiles = mods.ToDictionary(m => m.Id, m => Files(m.Root, OwnerExtensions), StringComparer.Ordinal);
            Need(ownerFiles.Values.Sum(files => files.Length) + nativeFiles.Length <= 100000, "Source file count exceeds 100000");
            var before = Stamps(nativeFiles.Concat(ownerFiles.Values.SelectMany(x => x)).Append(mapping).Append(oodle));
            Console.WriteLine($"Hashing {nativeFiles.Length} native files; fingerprints match Helpy's portable contract...");
            var digests = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            var memoFile = Path.Combine(Path.GetTempPath(), "runeschema-helpy-cache-digests-v1.json"); Safe(memoFile); var memo = new JObject();
            try { if (File.Exists(memoFile) && new FileInfo(memoFile).Length <= 16 * 1024 * 1024) memo = JObject.Parse(File.ReadAllText(memoFile)); } catch { }
            var hashed = 0;
            foreach (var file in before.Keys)
            {
                var stamp = before[file]; var previous = memo[file]; var digest = previous?.Value<string>("SHA256") ?? "";
                if (previous?.Value<long>("Size") != stamp.Size || previous?.Value<long>("Stamp") != stamp.Stamp || !Regex.IsMatch(digest, "^[a-f0-9]{64}$"))
                { using var input = File.OpenRead(file); digest = Convert.ToHexStringLower(SHA256.HashData(input)); }
                digests[file] = digest; memo[file] = new JObject { ["Size"] = stamp.Size, ["Stamp"] = stamp.Stamp, ["SHA256"] = digest };
                Progress("Hashing inputs", ++hashed, before.Count);
            }
            var memoBytes = Encoding.UTF8.GetBytes(Json(memo)); if (memoBytes.Length <= 16 * 1024 * 1024) Atomic(memoFile, memoBytes);
            var nativeKeys = nativeFiles.Select(p => KeyValuePair.Create(Path.GetExtension(p).ToLowerInvariant(), digests[p])).ToArray();
            var ownKeys = mods.ToDictionary(m => m.Id, m => ownerFiles[m.Id].Select(p => KeyValuePair.Create(Path.GetExtension(p).ToLowerInvariant(), digests[p])).OrderBy(p => p.Key, StringComparer.Ordinal).ToArray());
            OodleHelper.Initialize(oodle);
            CUE4Parse.Globals.FatalObjectSerializationErrors = true;
            Log.Logger = new LoggerConfiguration().MinimumLevel.Fatal().WriteTo.Console().CreateLogger();
            // Base capture mounts native containers only, even when installed mods exist.
            if (baseMode) Build(null, output);
            var failures = 0;
            foreach (var mod in mods) try { Build(mod, mod.Output); } catch (Exception e) { ++failures; Console.Error.WriteLine(mod.Id + ": cache unchanged; " + e.Message); }
            return failures == 0 ? 0 : 1;

            void CheckInputs() { CheckControl(); Closed(game); Unchanged(before, nativeRoots.SelectMany(r => Files(r, NativeExtensions)).Concat(mods.SelectMany(m => Files(m.Root, OwnerExtensions))).Append(mapping).Append(oodle)); }
            void Build((string Root, string Id, string Output)? mod, string destination)
            {
                CheckInputs();
                var extra = nativeRoots.Skip(1).Concat(mod is null ? [] : new[] { Path.Combine(mod.Value.Root, "paks") }).Select(p => new DirectoryInfo(p)).ToArray();
                using var provider = new DefaultFileProvider(new DirectoryInfo(nativeRoots[0]), extra, SearchOption.AllDirectories, new VersionContainer(EGame.GAME_UE5_6), StringComparer.OrdinalIgnoreCase) { MappingsContainer = new FileUsmapTypeMappingsProvider(mapping) };
                provider.Initialize(); provider.Mount(); provider.PostMount(); provider.LoadVirtualPaths();
                var report = new JArray();
                bool IsClass(string name, string parent)
                {
                    name = name.Split('.').Last(); var seen = new HashSet<string>();
                    while (seen.Add(name) && seen.Count <= 128)
                    {
                        if (name.Equals(parent, StringComparison.Ordinal)) return true;
                        if (!provider.MappingsContainer!.MappingsForGame!.Types.TryGetValue(name, out var schema)) break;
                        name = schema.SuperType ?? "";
                    }
                    return false;
                }
                bool IsObject(CUE4Parse.UE4.Assets.Exports.UObject obj, string parent)
                {
                    var current = obj.Class; var seen = new HashSet<string>();
                    while (current is not null && seen.Add(current.Name.Text) && seen.Count <= 128)
                    {
                        if (provider.MappingsContainer!.MappingsForGame!.Types.ContainsKey(current.Name.Text)) return IsClass(current.Name.Text, parent);
                        current = current.Object?.Value is UStruct schema ? schema.SuperStruct.ResolvedObject : null;
                    }
                    return false;
                }
                string Container(CUE4Parse.FileProvider.Objects.GameFile file) => file is VfsEntry entry ? Full(entry.Vfs.Path) : "";
                bool Selected(CUE4Parse.FileProvider.Objects.GameFile file) => mod is null ? nativeFiles.Contains(Container(file), StringComparer.OrdinalIgnoreCase) : Within(Container(file), Path.Combine(mod.Value.Root, "paks"));
                var assets = provider.Files.Values.Where(f => f.Extension.Equals("uasset", StringComparison.OrdinalIgnoreCase)).ToArray(); Need(assets.Length <= 524288, "Too many packages");
                var paths = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                foreach (var file in assets) try { paths[file.PathWithoutExtension.ToLowerInvariant()] = Asset(file.Path); } catch { }
                var packageFiles = new Dictionary<string, CUE4Parse.FileProvider.Objects.GameFile>(StringComparer.OrdinalIgnoreCase);
                foreach (var file in assets) if (paths.TryGetValue(file.PathWithoutExtension, out var path)) packageFiles[path[..path.LastIndexOf('.')]] = file;
                var candidates = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                foreach (var file in assets.Where(Selected)) if (file.NameWithoutExtension.StartsWith("ITEM_", StringComparison.OrdinalIgnoreCase) || file.NameWithoutExtension.StartsWith("JOURNAL_", StringComparison.OrdinalIgnoreCase)) candidates.Add(file.Path);
                // Registry class names add non-conventional filenames without loading every asset.
                foreach (var registry in provider.Files.Values.Where(f => f.Path.EndsWith("AssetRegistry.bin", StringComparison.OrdinalIgnoreCase)).Take(64)) try
                {
                    using var reader = registry.CreateReader(); var state = new FAssetRegistryState(reader);
                    foreach (var asset in state.PreallocatedAssetDataBuffers)
                    {
                        var cls = asset.AssetClass.ToString(); if (!IsClass(cls, "ItemData") && !IsClass(cls, "JournalEntryData")) continue;
                        if (packageFiles.TryGetValue(asset.PackageName.ToString(), out var match) && Selected(match)) candidates.Add(match.Path);
                    }
                } catch (Exception e) { report.Add(new JObject { ["Stage"] = "Registry", ["Asset"] = registry.Path, ["Error"] = e.Message }); }
                Need(candidates.Count <= 65536, "Too many candidate records");
                var records = new JArray(); var wantedIcons = new SortedSet<string>(StringComparer.Ordinal);
                var owners = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                foreach (var file in assets.Where(Selected)) try { var path = Asset(file.Path); owners[path[..path.LastIndexOf('.')]] = mod is null ? Source(path) is "game" or "engine" ? "core" : "plugins/" + Source(path) : "mods/" + mod.Value.Id; } catch { }
                Console.WriteLine($"{mod?.Id ?? "Base game"}: {candidates.Count} item/journal candidates");
                var inspected = 0; Progress("Reading items", 0, candidates.Count);
                foreach (var package in candidates.Order(StringComparer.Ordinal)) try
                {
                    var count = records.Count;
                    foreach (var obj in provider.LoadPackage(package).GetExports())
                    {
                        var item = IsObject(obj, "ItemData"); var journal = IsObject(obj, "JournalEntryData");
                        if (!item && !journal) continue;
                        var export = JObject.Parse(JsonConvert.SerializeObject(obj));
                        var type = export.Value<string>("Type") ?? ""; if (export["Properties"] is not JObject props) continue;
                        var path = Asset(package, export.Value<string>("Name")); var owner = owners.GetValueOrDefault(path[..path.LastIndexOf('.')]); if (owner is null) continue;
                        var name = First(props, "DisplayName", "ItemName", "Title", "Name"); if (name.Length == 0) name = export.Value<string>("Name") ?? "";
                        var data = new JObject { ["Name"] = name, ["Type"] = type };
                        foreach (var field in new[] { "Category", "Section", "InternalName" }) { var text = Text(props[field]); if (text.Length > 0) data[field] = text; }
                        var description = First(props, "Description", "FlavourText"); if (description.Length > 0) data["Description"] = description;
                        foreach (var field in new[] { "PowerLevel", "MaxStackSize", "StackSize" }) if (props[field]?.Type is JTokenType.Integer or JTokenType.Float && props.Value<double>(field) is >= 0 and <= 1e9) data[field] = props[field]!.DeepClone();
                        var icon = new[] { "Icon", "ItemIcon", "UIIcon", "IconTexture", "Image" }.Select(f => Reference(props[f], paths)).FirstOrDefault(p => p.Length > 0) ?? "";
                        if (icon.Length > 0) { data["Icon"] = icon; wantedIcons.Add(icon); }
                        if (journal && props["Pages"] is JArray pages) data["Pages"] = new JArray(pages.Select(Text).Where(s => s.Length > 0).Take(128));
                        var pid = Text(props["PersistenceID"]); // An absent/unknown ID stays absent; never invent one.
                        records.Add(new JObject { ["Kind"] = journal ? "journal" : "items", ["Owner"] = owner, ["Source"] = Source(path), ["Path"] = path, ["PersistenceID"] = pid, ["Data"] = data });
                    }
                    if (records.Count == count) report.Add(new JObject { ["Stage"] = "Metadata", ["Asset"] = package, ["Error"] = "No item/journal export with verified native class ancestry" });
                } catch (Exception e) { report.Add(new JObject { ["Stage"] = "Metadata", ["Asset"] = package, ["Error"] = e.Message }); }
                finally { Progress("Reading items", ++inspected, candidates.Count); }
                // Include selected cooked UI textures even if this partial catalogue did not reference them.
                foreach (var file in assets.Where(Selected)) if (file.Name.StartsWith("T_", StringComparison.OrdinalIgnoreCase) && (file.Path.Contains("/UI/", StringComparison.OrdinalIgnoreCase) || file.Path.Contains("/Icons/", StringComparison.OrdinalIgnoreCase) || file.Name.StartsWith("T_Icon_", StringComparison.OrdinalIgnoreCase))) try { wantedIcons.Add(Asset(file.Path)); } catch { }
                // Native item/journal providers only; unrelated engine/plugin UI is not an item cache.
                if (mod is null) { var groups = records.Select(r => r.Value<string>("Source")!).ToHashSet(StringComparer.Ordinal); wantedIcons.RemoveWhere(path => !groups.Contains(Source(path))); }
                var icons = new JArray(); using var payload = new MemoryStream();
                var captured = 0; Progress("Caching pictures", 0, wantedIcons.Count);
                foreach (var icon in wantedIcons.Order(StringComparer.Ordinal))
                {
                    try
                    {
                        var package = icon[..icon.LastIndexOf('.')];
                        Need(packageFiles.TryGetValue(package, out var textureFile), "Icon package is not in the mounted input set");
                        var texture = provider.LoadPackage(textureFile!.Path).GetExports().OfType<UTexture2D>().Single(t => t.Name == icon[(icon.LastIndexOf('.') + 1)..]);
                        using var bitmap = Decode(texture);
                        var pixels = Pixels(bitmap); Need(payload.Length + pixels.Length <= MaxBundle - 80 * 1024 * 1024, "Pixel budget exceeded; split the mod cache");
                        var origin = owners.GetValueOrDefault(package) ?? (Source(icon) is "game" or "engine" ? "core" : "plugins/" + Source(icon));
                        icons.Add(new JObject { ["Icon"] = icon, ["Source"] = Source(icon), ["SHA256"] = Hash(pixels), ["Bytes"] = pixels.Length, ["Origins"] = new JArray(origin) }); payload.Write(pixels);
                    } catch (Exception e) { report.Add(new JObject { ["Stage"] = "Icon", ["Asset"] = icon, ["Error"] = e.Message }); }
                    finally { Progress("Caching pictures", ++captured, wantedIcons.Count); }
                }
                var banner = mod is null ? null : new[] { Path.Combine(mod.Value.Root, "background.png"), Path.Combine(mod.Value.Root, "mod.png"), Path.Combine(mod.Value.Root, "banner.png"), Path.Combine(folder, "background.png"), Path.Combine(folder, "mod.png"), Path.Combine(folder, "banner.png") }.FirstOrDefault(File.Exists);
                if (banner is not null)
                {
                    Safe(banner); Need(new FileInfo(banner).Length <= 16 * 1024 * 1024, "Banner exceeds 16 MiB");
                    var group = owners.Keys.Select(p => Source(p + ".asset")).FirstOrDefault(s => s is not "game" and not "engine");
                    if (group is not null)
                    {
                        using var codec = SKCodec.Create(banner) ?? throw new InvalidDataException("Banner cannot be decoded");
                        Need(codec.Info.Width is > 0 and <= 8192 && codec.Info.Height is > 0 and <= 8192, "Banner extent outside 1..8192");
                        using var bitmap = SKBitmap.Decode(codec) ?? throw new InvalidDataException("Banner cannot be decoded");
                        var owner = "mods/" + mod!.Value.Id; ulong fnv = 1469598103934665603UL; foreach (var b in Encoding.UTF8.GetBytes(owner)) fnv = unchecked((fnv ^ b) * 1099511628211UL);
                        var path = "/" + group + "/__Helpy__/SourceBanner_" + fnv.ToString("x16") + ".SourceBanner";
                        records.Add(new JObject { ["Kind"] = "art", ["Owner"] = owner, ["Source"] = group, ["Path"] = path, ["PersistenceID"] = "", ["Data"] = new JObject { ["Name"] = "Source banner", ["SourceBannerPixels"] = new JObject { ["Width"] = 256, ["Height"] = 144, ["RGBAHex"] = Convert.ToHexStringLower(Pixels(bitmap, true)) } } });
                    }
                    else report.Add(new JObject { ["Stage"] = "Banner", ["Error"] = "No independent mod namespace for a source banner; loose banner retained" });
                }
                var sources = records.Select(r => r.Value<string>("Source")!).Concat(icons.Select(r => r.Value<string>("Source")!)).Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal).ToArray();
                if (sources.Length == 0 || records.Count + icons.Count == 0) throw new InvalidDataException("No cacheable presentation found. Previous cache retained. " + report.ToString(Formatting.None));
                Need(sources.Length <= 512 && icons.Count <= 32768 && records.Count <= 65536, "Cache entry count exceeds import limits");
                var fingerprints = new JObject();
                var ownedGroups = owners.Keys.Select(p => Source(p + ".asset")).ToHashSet(StringComparer.Ordinal);
                foreach (var source in sources) fingerprints[source] = Fingerprint("rsmk-content-v2|" + source + "\n", mod is not null && ownedGroups.Contains(source) ? ownKeys[mod.Value.Id] : nativeKeys);
                var manifest = new JObject { ["Version"] = 2, ["FingerprintVersion"] = 2, ["Kind"] = "RuneSchemaCacheBundle", ["Locale"] = locale, ["Sources"] = fingerprints, ["Records"] = records, ["Icons"] = icons,
                    ["RecordCoverage"] = "Partial", ["Coverage"] = new JObject { ["Complete"] = false, ["Expected"] = wantedIcons.Count, ["Exported"] = icons.Count } };
                if (mod is not null) manifest["ContentOwner"] = "mods/" + mod.Value.Id;
                // Stable across identical exports, monotonic when cache content changes.
                var previousBundle = Path.Combine(destination, "offline-helpy.rsmk-cache");
                long revision = 1;
                if (File.Exists(previousBundle)) try {
                    using var reader = new BinaryReader(File.OpenRead(previousBundle), Encoding.UTF8);
                    Need(Encoding.ASCII.GetString(reader.ReadBytes(8)) == "RSCACHE1", "Unsupported previous cache");
                    var length = reader.ReadInt32(); Need(length > 0 && length <= 80 * 1024 * 1024, "Invalid previous cache size");
                    var previous = JObject.Parse(Encoding.UTF8.GetString(reader.ReadBytes(length)));
                    revision = Math.Max(1, previous.Value<long?>("CacheVersion") ?? 1); previous.Remove("CacheVersion");
                    if (!JToken.DeepEquals(previous, manifest)) revision = checked(revision + 1);
                } catch (Exception e) { throw new InvalidDataException("Previous cache revision cannot be read; retain it and choose a fresh output folder. " + e.Message); }
                manifest["CacheVersion"] = revision;
                var metadata = Encoding.UTF8.GetBytes(Json(manifest)); Need(metadata.Length <= 80 * 1024 * 1024 && metadata.Length + payload.Length + 12 <= MaxBundle, "Bundle exceeds download/import limits");
                using var bundle = new MemoryStream(); using (var writer = new BinaryWriter(bundle, Encoding.UTF8, true)) { writer.Write(Encoding.ASCII.GetBytes("RSCACHE1")); writer.Write(metadata.Length); writer.Write(metadata); payload.Position = 0; payload.CopyTo(bundle); }
                var bytes = bundle.ToArray(); var digest = Hash(bytes); CheckInputs();
                Directory.CreateDirectory(destination); Safe(destination);
                if (banner is not null) Atomic(Path.Combine(destination, "mod.png"), File.ReadAllBytes(banner));
                var receipt = new JObject { ["Tool"] = "RuneSchema offline Helpy cache", ["Format"] = 1, ["Owner"] = mod is null ? "native" : "mods/" + mod.Value.Id, ["SHA256"] = digest, ["Bytes"] = bytes.Length,
                    ["CacheVersion"] = revision, ["FingerprintVersion"] = 2, ["Records"] = records.Count, ["Icons"] = icons.Count, ["RecordCoverage"] = "Partial", ["Mapping"] = Path.GetFileName(mapping), ["MappingSHA256"] = Hash(File.ReadAllBytes(mapping)), ["Issues"] = report };
                Atomic(Path.Combine(destination, "offline-helpy-report.json"), Encoding.UTF8.GetBytes(receipt.ToString()));
                Atomic(Path.Combine(destination, "offline-helpy.rsmk-cache"), bytes);
                Console.WriteLine($"Wrote {destination}: {records.Count} records, {icons.Count} icons, {report.Count} reported skips. Partial presentation cache; no gameplay or save changes.");
            }
        }
        catch (Exception error) { Console.Error.WriteLine("Cache not completed: " + error.Message); return 1; }
    }
    static void Atomic(string path, byte[] bytes)
    {
        Safe(path); var temporary = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try { using (var file = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None)) { file.Write(bytes); file.Flush(true); } File.Move(temporary, path, true); }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }
    static void SelfTest()
    {
        AuthoringInputTests.Run();
        CreationArchiveTests.Run();
        Need(Source("/Game/Gameplay/Mods/Dye/ITEM.Item") == "game/gameplay/mods/dye", "Nested mod namespace");
        Need(Asset("RSDragonwilds/Plugins/Test/Content/UI/T_Icon.uasset") == "/Test/UI/T_Icon.T_Icon", "Plugin virtual path");
        Need(Asset("RSDragonwilds/Content/Items/ITEM_X.uasset") == "/Game/Items/ITEM_X.ITEM_X", "Game virtual path");
        Need(Fingerprint("seed\n", new[] { KeyValuePair.Create("b", "2"), KeyValuePair.Create("a", "1") }) == Hash(Encoding.UTF8.GetBytes("seed\na|1\nb|2\n")), "Portable ordering and separators");
        Need(Fingerprint("rsmk-content-v2|example\n", new[] { KeyValuePair.Create(".pak", "b"), KeyValuePair.Create(".pak", "a") }) == Hash(Encoding.UTF8.GetBytes("rsmk-content-v2|example\n.pak|a\n.pak|b\n")), "Duplicate extensions sort by content digest, never folder names");
        Need(Text(JObject.Parse("{\"SourceString\":\"A readable name\"}")) == "A readable name" && Text(new JObject()) == "", "Text extraction must not invent values");
        using var pixel = new SKBitmap(new SKImageInfo(1, 1, SKColorType.Bgra8888, SKAlphaType.Unpremul)); pixel.SetPixel(0, 0, new SKColor(10, 20, 30, 255)); var bytes = Pixels(pixel);
        Need(bytes.Length == 20 && bytes[16] == 10 && bytes[17] == 20 && bytes[18] == 30 && bytes[19] == 255, "BGRA must become RGBA");
        Need(Pixels(pixel, true).Length == 256 * 144 * 4, "Banner dimensions");
        var folder = Path.Combine(Path.GetTempPath(), "runeschema-cache-test-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(folder);
        try { var file = Path.Combine(folder, "source.pak"); File.WriteAllText(file, "one"); var before = Stamps([file]); File.WriteAllText(file, "longer"); var refused = false; try { Unchanged(before, [file]); } catch (InvalidDataException) { refused = true; } Need(refused, "Changed inputs must be refused"); Atomic(Path.Combine(folder, "keep"), [1, 2, 3]); Atomic(Path.Combine(folder, "keep"), [4]); Need(File.ReadAllBytes(Path.Combine(folder, "keep")).SequenceEqual(new byte[] { 4 }), "Atomic replacement"); }
        finally { Directory.Delete(folder, true); }
        Console.WriteLine("Offline cache self-tests passed.");
    }
}
'@
[IO.File]::WriteAllText((Join-Path $stage 'Program.cs'), $builderSource, [Text.UTF8Encoding]::new($false))
$builderSource=@'
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using CUE4Parse.Compression;
using CUE4Parse.MappingsProvider.Usmap;
using Microsoft.Win32;
using Newtonsoft.Json.Linq;

// Offline authoring inputs only: never install mappings or change UE4SS settings.
static class AuthoringInputs
{
    internal const int MaxMapping = 16 * 1024 * 1024;
    static readonly string Store = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "RSDragonwilds", "RuneSchema", "core", "cache", "authoring-tools");
    static void Need(bool ok, string message) { if (!ok) throw new InvalidDataException(message); }
    internal static string? Project(string path)
    {
        foreach (var candidate in new[] { path, Path.Combine(path, "RSDragonwilds"), Path.Combine(path, "Content", "RSDragonwilds") })
            if (Directory.Exists(Path.Combine(candidate, "Content", "Paks"))) return Path.GetFullPath(candidate);
        return null;
    }
    internal static IEnumerable<string> SteamLibraries(string root)
    {
        yield return root;
        var vdf = Path.Combine(root, "steamapps", "libraryfolders.vdf");
        if (!File.Exists(vdf) || new FileInfo(vdf).Length > 1024 * 1024) yield break;
        foreach (Match match in Regex.Matches(File.ReadAllText(vdf), "\"path\"\\s*\"([^\"]+)\""))
            yield return match.Groups[1].Value.Replace("\\\\", "\\");
    }
    static IEnumerable<string> InstalledGames(string input)
    {
        if (!OperatingSystem.IsWindows()) yield break;
        for (var at = new DirectoryInfo(input); at is not null; at = at.Parent) yield return at.FullName;
        var steam = new[] {
            Registry.GetValue(@"HKEY_CURRENT_USER\Software\Valve\Steam", "SteamPath", null) as string,
            Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath", null) as string,
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Steam")
        };
        foreach (var root in steam.Where(p => !string.IsNullOrWhiteSpace(p)).Distinct(StringComparer.OrdinalIgnoreCase))
            foreach (var library in SteamLibraries(root!)) yield return Path.Combine(library, "steamapps", "common", "RSDragonwilds");
        foreach (var drive in DriveInfo.GetDrives().Where(d => d.IsReady && d.DriveType == DriveType.Fixed))
        {
            var xbox = Path.Combine(drive.RootDirectory.FullName, "XboxGames");
            if (Directory.Exists(xbox))
                foreach (var folder in Directory.EnumerateDirectories(xbox).Where(p => Path.GetFileName(p).Contains("Dragonwilds", StringComparison.OrdinalIgnoreCase))) yield return folder;
        }
        var epic = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "Epic", "EpicGamesLauncher", "Data", "Manifests");
        if (Directory.Exists(epic)) foreach (var file in Directory.EnumerateFiles(epic, "*.item"))
        {
            if (new FileInfo(file).Length > 1024 * 1024) continue;
            string? install = null;
            try { install = JObject.Parse(File.ReadAllText(file)).Value<string>("InstallLocation"); } catch (Newtonsoft.Json.JsonException) { }
            if (install is not null) yield return install;
        }
    }
    internal static string Game(string? supplied, string input, Action<string> closed)
    {
        if (supplied is not null)
        {
            var project = Project(supplied) ?? throw new InvalidDataException("Selected game must contain RSDragonwilds/Content/Paks."); closed(project); return project;
        }
        var candidates = InstalledGames(input).Select(Project).OfType<string>().Distinct(StringComparer.OrdinalIgnoreCase);
        foreach (var candidate in candidates)
        {
            try { closed(candidate); Console.WriteLine("Using installed game: " + candidate); return candidate; }
            catch (InvalidDataException) { Console.WriteLine("Skipping active installation: " + candidate); }
        }
        throw new InvalidDataException("No inactive Dragonwilds installation found. Close the target game, or use -Game/--game for a custom installation.");
    }
    internal static void ValidateMapping(string path)
    {
        Need(Path.GetExtension(path).Equals(".usmap", StringComparison.OrdinalIgnoreCase) && File.Exists(path), "Mapping must be an existing .usmap file (not a .umap level).");
        Need(new FileInfo(path).Length is >= 12 and <= MaxMapping, "Mapping size is outside 12 bytes..16 MiB.");
        using (var reader = new BinaryReader(File.OpenRead(path)))
        {
            Need(reader.ReadUInt16() == 0x30C4, "Invalid .usmap header.");
            var version = reader.ReadByte(); Need(version <= (byte)EUsmapVersion.Latest, "Unsupported .usmap version.");
            if (version >= (byte)EUsmapVersion.PackageVersioning)
            {
                var versioned = reader.ReadInt32(); Need(versioned is 0 or 1, "Invalid mapping version flag.");
                if (versioned == 1)
                {
                    reader.ReadUInt32(); reader.ReadUInt32(); var customs = reader.ReadInt32(); Need(customs is >= 0 and <= 4096, "Invalid mapping custom versions.");
                    reader.ReadBytes(customs * 20); reader.ReadUInt32();
                }
            }
            reader.ReadByte(); var compressed = reader.ReadUInt32(); var expanded = reader.ReadUInt32();
            Need(compressed <= reader.BaseStream.Length - reader.BaseStream.Position && expanded <= 256 * 1024 * 1024, "Mapping is truncated or expands beyond the 256 MiB limit.");
        }
        var types = new FileUsmapTypeMappingsProvider(path).MappingsForGame?.Types;
        Need(types is not null && types.ContainsKey("ItemData") && types.ContainsKey("Texture2D"), "Mapping does not contain Dragonwilds ItemData/Texture2D schemas.");
    }
    internal static byte[] Download(HttpClient client, string url, int limit)
    {
        using var cancel = new CancellationTokenSource(TimeSpan.FromSeconds(30));
        using var response = client.GetAsync(url, HttpCompletionOption.ResponseHeadersRead, cancel.Token).GetAwaiter().GetResult(); response.EnsureSuccessStatusCode();
        Need(response.Content.Headers.ContentLength is null || response.Content.Headers.ContentLength <= limit, "Authoring download exceeds its size limit.");
        using var stream = response.Content.ReadAsStream(); using var output = new MemoryStream();
        var buffer = new byte[65536]; int count;
        while ((count = stream.ReadAsync(buffer.AsMemory(), cancel.Token).AsTask().GetAwaiter().GetResult()) != 0) { Need(output.Length + count <= limit, "Authoring download exceeds its size limit."); output.Write(buffer, 0, count); }
        return output.ToArray();
    }
    internal static string BlobHash(byte[] data) => Convert.ToHexStringLower(SHA1.HashData(Encoding.ASCII.GetBytes("blob " + data.Length + "\0").Concat(data).ToArray()));
    internal static JObject ArchiveMapping(JArray entries)
    {
        var maps = entries.OfType<JObject>().Where(e => e.Value<string>("type") == "file" && e.Value<string>("name")?.EndsWith(".usmap", StringComparison.OrdinalIgnoreCase) == true).ToArray();
        Need(maps.Length == 1, "RSDW Archive mapping selection is ambiguous; use an explicit .usmap for this build.");
        var map = maps[0]; var path = map.Value<string>("path") ?? "";
        Need(Regex.IsMatch(path, @"^\d+\.\d+\.\d+\.\d+/usmap/[^/\\:]+\.usmap$") && !path.Contains("..") && Regex.IsMatch(map.Value<string>("sha") ?? "", "^[a-f0-9]{40}$") && map.Value<long>("size") is >= 12 and <= MaxMapping, "Invalid RSDW Archive mapping metadata.");
        return map;
    }
    internal static string Mapping(string? supplied, string input, string game, Action<string> safe)
    {
        if (supplied is not null) { safe(supplied); ValidateMapping(supplied); return Path.GetFullPath(supplied); }
        var provided = Directory.GetFiles(input, "*.usmap", SearchOption.TopDirectoryOnly);
        Need(provided.Length <= 1, "Multiple supplied .usmap files: include one, or select -Mapping/--mapping.");
        if (provided.Length == 1) return Mapping(provided[0], input, game, safe);
        var win64 = Path.Combine(game, "Binaries", "Win64");
        var platform = Directory.Exists(Path.Combine(game, "Binaries", "WinGDK")) ? "gamepass" : Directory.Exists(win64) && Directory.GetFiles(win64, "*Server*.exe").Length > 0 ? "server" : "steam";
        var runtime = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "RSDragonwilds", "RuneSchema", platform, "cache", "mappings");
        var current = Path.Combine(runtime, "current.json");
        try
        {
            safe(current);
            if (File.Exists(current) && new FileInfo(current).Length <= 65536)
            {
                var meta = JObject.Parse(File.ReadAllText(current)); var build = meta.Value<string>("Build") ?? ""; var fingerprint = meta.Value<string>("Fingerprint") ?? "";
                if (Regex.IsMatch(build, "^[a-f0-9]{16}$") && Regex.IsMatch(fingerprint, "^[a-f0-9]{16}$"))
                { var local = Path.Combine(runtime, build, fingerprint + ".usmap"); safe(local); ValidateMapping(local); Console.WriteLine("Reading existing platform mapping (unchanged): " + local); return local; }
            }
        }
        catch (Exception e) when (e is IOException or InvalidDataException or CUE4Parse.UE4.Exceptions.ParserException or Newtonsoft.Json.JsonException or ArgumentException) { Console.WriteLine("Local mapping unavailable: " + e.Message); }
        return Archive(safe);
    }
    internal static string Archive(Action<string> safe, string? cache = null, HttpClient? downloadClient = null)
    {
        cache ??= Path.Combine(Store, "mappings"); safe(cache); Directory.CreateDirectory(cache);
        var receipt = Path.Combine(cache, "archive.json"); safe(receipt);
        JObject? selected = null;
        using var ownedClient = downloadClient is null ? new HttpClient { Timeout = TimeSpan.FromSeconds(30) } : null;
        var client = downloadClient ?? ownedClient!; client.DefaultRequestHeaders.UserAgent.ParseAdd("RuneSchema-Authoring/1.0");
        const string api = "https://api.github.com/repos/RSDWArchive/RSDWArchive/contents/";
        try
        {
            Console.WriteLine("Finding published .usmap in RSDW Archive...");
            var root = JArray.Parse(Encoding.UTF8.GetString(Download(client, api, 1024 * 1024)));
            var version = root.OfType<JObject>().Where(e => e.Value<string>("type") == "dir").Select(e => e.Value<string>("name") ?? "").Where(n => Regex.IsMatch(n, @"^\d+\.\d+\.\d+\.\d+$")).OrderByDescending(n => Version.Parse(n)).FirstOrDefault();
            Need(version is not null, "RSDW Archive has no published version.");
            selected = ArchiveMapping(JArray.Parse(Encoding.UTF8.GetString(Download(client, api + version + "/usmap", 1024 * 1024))));
            var target = Path.Combine(cache, selected.Value<string>("sha") + ".usmap"); safe(target);
            var data = File.Exists(target) && new FileInfo(target).Length <= MaxMapping ? File.ReadAllBytes(target) : [];
            if (BlobHash(data) != selected.Value<string>("sha"))
            {
                var url = "https://raw.githubusercontent.com/RSDWArchive/RSDWArchive/main/" + string.Join('/', selected.Value<string>("path")!.Split('/').Select(Uri.EscapeDataString));
                data = Download(client, url, MaxMapping);
                Need(data.LongLength == selected.Value<long>("size") && BlobHash(data) == selected.Value<string>("sha"), "Downloaded mapping does not match GitHub's blob fingerprint.");
                WriteMapping(target, data, safe);
            }
            ValidateMapping(target); var receiptTemp = receipt + "." + Guid.NewGuid().ToString("N"); safe(receiptTemp);
            try { File.WriteAllText(receiptTemp, selected.ToString()); File.Move(receiptTemp, receipt, true); }
            finally { if (File.Exists(receiptTemp)) File.Delete(receiptTemp); }
            Console.WriteLine("Using archive snapshot: " + selected.Value<string>("path") + "; check the report for skipped assets after game updates."); return target;
        }
        catch (Exception e) when (e is HttpRequestException or TaskCanceledException or IOException or InvalidDataException or CUE4Parse.UE4.Exceptions.ParserException or Newtonsoft.Json.JsonException)
        {
            if (File.Exists(receipt) && new FileInfo(receipt).Length <= 65536)
            {
                selected = ArchiveMapping(new JArray(JObject.Parse(File.ReadAllText(receipt)))); var previous = Path.Combine(cache, selected.Value<string>("sha") + ".usmap"); safe(previous);
                ValidateMapping(previous); Need(BlobHash(File.ReadAllBytes(previous)) == selected.Value<string>("sha"), "Cached archive mapping fingerprint differs.");
                Console.WriteLine("Archive unavailable; using verified previous mapping: " + e.Message); return previous;
            }
            throw new InvalidDataException("Cannot obtain an offline .usmap. Include one in the mod ZIP, place one beside it, or retry with internet access. " + e.Message, e);
        }
    }
    static void WriteMapping(string target, byte[] data, Action<string> safe)
    {
        var temporary = target + "." + Guid.NewGuid().ToString("N") + ".usmap"; safe(temporary);
        try { File.WriteAllBytes(temporary, data); ValidateMapping(temporary); File.Move(temporary, target, true); }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }
    internal static string Decoder(string? supplied, Action<string> safe)
    {
        var path = supplied is null ? Path.Combine(Store, "decoder", "oodle-data-shared.dll") : Path.GetFullPath(supplied);
        safe(path);
        if (supplied is not null) Need(File.Exists(path), "Explicit Oodle DLL was not found.");
        else if (!File.Exists(path))
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path)!); var temporary = path + "." + Guid.NewGuid().ToString("N") + ".dll"; safe(temporary);
            try
            {
                Console.WriteLine("Preparing CUE4Parse's pinned Oodle decoder for offline authoring...");
                using var cancel = new CancellationTokenSource(TimeSpan.FromSeconds(60)); string? download = temporary;
                Need(OodleHelper.DownloadOodleDllAsync(ref download, cancel.Token).GetAwaiter().GetResult() && File.Exists(temporary), "Decoder bootstrap failed; retry online or select -Oodle/--oodle.");
                Need(new FileInfo(temporary).Length is > 0 and <= 32 * 1024 * 1024, "Decoder has an invalid size.");
                File.Move(temporary, path, true);
            }
            finally { if (File.Exists(temporary)) File.Delete(temporary); }
        }
        return path;
    }
}
'@
[IO.File]::WriteAllText((Join-Path $stage 'AuthoringInputs.cs'), $builderSource, [Text.UTF8Encoding]::new($false))
$builderSource=@'
using System.Net;
using System.Text;
using Newtonsoft.Json.Linq;

static class AuthoringInputTests
{
    static void Check(bool ok, string message) { if (!ok) throw new Exception(message); }
    static void Refuses(Action action) { try { action(); } catch (InvalidDataException) { return; } throw new Exception("Invalid authoring input accepted"); }
    static byte[] Mapping(byte version, bool versioned = false)
    {
        using var body = new MemoryStream(); using var w = new BinaryWriter(body);
        w.Write(2u);
        foreach (var name in new[] { "ItemData", "Texture2D" }) { if (version >= 2) w.Write((ushort)name.Length); else w.Write((byte)name.Length); w.Write(Encoding.ASCII.GetBytes(name)); }
        w.Write(0u); w.Write(2u);
        for (var i = 0; i < 2; ++i) { w.Write(i); w.Write(-1); w.Write((ushort)0); w.Write((ushort)0); }
        using var result = new MemoryStream(); using var header = new BinaryWriter(result);
        header.Write((ushort)0x30c4); header.Write(version);
        if (version >= 1) { header.Write(versioned ? 1 : 0); if (versioned) { header.Write(0); header.Write(0); header.Write(0); header.Write(0u); } }
        header.Write((byte)0); header.Write((uint)body.Length); header.Write((uint)body.Length); header.Write(body.ToArray()); return result.ToArray();
    }
    sealed class ArchiveHandler(byte[] mapping) : HttpMessageHandler
    {
        public bool Offline, BadHash;
        public int MappingRequests;
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken token)
        {
            if (Offline) throw new HttpRequestException("fixture offline");
            var path = request.RequestUri!.AbsolutePath;
            var blob = AuthoringInputs.BlobHash(mapping);
            byte[] bytes;
            if (path.EndsWith("/contents/")) bytes = Encoding.UTF8.GetBytes("[{\"type\":\"dir\",\"name\":\"0.9.0.0\"},{\"type\":\"dir\",\"name\":\"1.0.0.5\"}]");
            else if (path.EndsWith("/contents/1.0.0.5/usmap")) bytes = Encoding.UTF8.GetBytes(new JArray(new JObject { ["type"] = "file", ["name"] = "game.usmap", ["path"] = "1.0.0.5/usmap/game.usmap", ["sha"] = BadHash ? new string('0', 40) : blob, ["size"] = mapping.Length }).ToString());
            else { MappingRequests++; bytes = mapping; }
            return Task.FromResult(new HttpResponseMessage(HttpStatusCode.OK) { Content = new ByteArrayContent(bytes) });
        }
    }
    internal static void Run()
    {
        var root = Path.Combine(Path.GetTempPath(), "rs-author-input-tests-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(root);
        try
        {
            var game = Path.Combine(root, "Installed", "RSDragonwilds"); Directory.CreateDirectory(Path.Combine(game, "Content", "Paks"));
            Check(AuthoringInputs.Project(Path.GetDirectoryName(game)!) == game, "Installed root normalization failed");
            Check(AuthoringInputs.Game(game, root, _ => { }) == game, "Explicit game selection failed");
            Refuses(() => AuthoringInputs.Game(game, root, _ => throw new InvalidDataException("active")));
            var steam = Path.Combine(root, "Steam"); Directory.CreateDirectory(Path.Combine(steam, "steamapps"));
            File.WriteAllText(Path.Combine(steam, "steamapps", "libraryfolders.vdf"), "\"path\" \"D:\\\\Library With Spaces\"");
            Check(AuthoringInputs.SteamLibraries(steam).Last() == @"D:\Library With Spaces", "Steam library decoding failed");
            var supplied = Path.Combine(root, "provided.usmap");
            foreach (var version in new byte[] { 0, 1, 2, 3, 4 }) { File.WriteAllBytes(supplied, Mapping(version)); AuthoringInputs.ValidateMapping(supplied); }
            File.WriteAllBytes(supplied, Mapping(4, true)); AuthoringInputs.ValidateMapping(supplied);
            var original = File.ReadAllBytes(supplied);
            Check(AuthoringInputs.Mapping(null, root, game, _ => { }) == supplied, "Supplied mapping must take priority without network");
            Check(File.ReadAllBytes(supplied).SequenceEqual(original), "Supplied mapping changed");
            var another = Path.Combine(root, "ambiguous.usmap"); File.WriteAllBytes(another, original);
            Refuses(() => AuthoringInputs.Mapping(null, root, game, _ => { })); File.Delete(another);
            var unsafeMetadata = new JArray(new JObject { ["type"] = "file", ["name"] = "x.usmap", ["path"] = "../x.usmap", ["sha"] = new string('0', 40), ["size"] = 12 });
            Refuses(() => AuthoringInputs.ArchiveMapping(unsafeMetadata));
            var handler = new ArchiveHandler(original); using var client = new HttpClient(handler);
            var cache = Path.Combine(root, "archive"); var downloaded = AuthoringInputs.Archive(_ => { }, cache, client);
            Check(File.ReadAllBytes(downloaded).SequenceEqual(original) && handler.MappingRequests == 1, "Archive download failed");
            AuthoringInputs.Archive(_ => { }, cache, client); Check(handler.MappingRequests == 1, "Unchanged mapping was downloaded again");
            handler.Offline = true; Check(AuthoringInputs.Archive(_ => { }, cache, client) == downloaded, "Offline verified mapping reuse failed");
            handler.Offline = false; handler.BadHash = true;
            Check(AuthoringInputs.Archive(_ => { }, cache, client) == downloaded, "Bad download must retain verified previous mapping");
            Refuses(() => AuthoringInputs.Archive(_ => { }, Path.Combine(root, "empty-cache"), client));
            File.WriteAllBytes(supplied, new byte[16]); Refuses(() => AuthoringInputs.ValidateMapping(supplied));
            var oversized = Mapping(0); BitConverter.GetBytes(uint.MaxValue).CopyTo(oversized, 8); File.WriteAllBytes(supplied, oversized); Refuses(() => AuthoringInputs.ValidateMapping(supplied));
            Console.WriteLine("Authoring input tests passed: discovery, supplied schemas, archive fingerprint, download reuse, offline fallback and bad-input retention.");
        }
        finally { Directory.Delete(root, true); }
    }
}
'@
[IO.File]::WriteAllText((Join-Path $stage 'AuthoringInputs.Tests.cs'), $builderSource, [Text.UTF8Encoding]::new($false))
$builderSource=@'
using System.IO.Compression;
using System.Text.RegularExpressions;
using Newtonsoft.Json.Linq;
using SharpCompress.Archives;

// Archive contents are data: never run their installers, scripts or executables.
static class CreationArchive
{
    static void Need(bool ok,string message){if(!ok)throw new InvalidDataException(message);}
    static readonly string[] Loaders=["assets","blueprints","buildings","courses","dialogue","effects","enums","equipment","events","journal","lore","nameplates","niagara","npc","players","quests","raw","recipes","registry","spawns","strings","ue4ss","vendors","paks","logicmods","items","skills","loot","npcs","attacks"];
    static bool NativeDll(string root)=>File.Exists(Path.Combine(root,"main.dll"))||Directory.Exists(Path.Combine(root,"dlls"))&&Directory.EnumerateFiles(Path.Combine(root,"dlls"),"*.dll").Any();
    static void Safe(string path){for(var at=Path.GetFullPath(path);!string.IsNullOrEmpty(at);at=Path.GetDirectoryName(at))if(File.Exists(at)||Directory.Exists(at))Need((File.GetAttributes(at)&FileAttributes.ReparsePoint)==0,"Links are unsupported: "+at);}
    internal static string Relative(string name){
        name=name.Replace('\\','/').TrimEnd('/');Need(name.Length>0&&name.Length<=1024,"Invalid archive path.");
        foreach(var part in name.Split('/'))Need(part.Length>0&&part is not "." and not ".."&&!part.EndsWith('.')&&!part.EndsWith(' ')&&!part.Any(c=>c<32||"<>:\"|?*".Contains(c))&&!Regex.IsMatch(part,@"^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)",RegexOptions.IgnoreCase),"Unsafe archive path: "+name);
        return name;
    }
    internal static string Identity(JObject metadata){
        var id=metadata.Value<string>("ModID")??"";Need(Regex.IsMatch(id,@"^[a-zA-Z0-9][a-zA-Z0-9 _-]{0,95}$")&&!id.EndsWith(' '),"Mod folder names need 1..96 letters, digits, spaces, underscores or dashes.");Relative(id);
        var limits=new Dictionary<string,int>{{"ModID",128},{"Name",256},{"Nickname",192},{"Author",256},{"Version",64},{"Build",64},{"Website",2048},{"NexusModID",20},{"NexusFileID",20},{"Description",1024},{"Banner",1024}};
        var lines=new List<string>();
        foreach(var (key,max) in limits){var value=metadata.Value<string>(key)??"";Need(value.Length<=max&&!value.Any(char.IsControl),"Invalid ID.txt field: "+key);
            if(key=="Author")Need(!string.IsNullOrWhiteSpace(value),"Author is required.");
            if(key=="Website"&&value.Length>0)Need(Uri.TryCreate(value,UriKind.Absolute,out var uri)&&uri.Scheme is "https" or "http","Website must use http or https.");
            if(key is "NexusModID" or "NexusFileID"&&value.Length>0)Need(Regex.IsMatch(value,@"^[0-9]{1,20}$")&&value.Any(c=>c!='0'),"Nexus IDs must be positive decimal numbers or blank.");
            if(key=="Banner"&&value.Length>0){Relative(value);Need(!value.Contains('\\')&&new[]{".png",".jpg",".jpeg",".webp"}.Contains(Path.GetExtension(value).ToLowerInvariant()),"Banner must be a relative picture path.");}
            lines.Add(key+"="+(key=="Name"&&value.Length==0?id:value));
        }return string.Join('\n',lines)+"\n";
    }
    static void Copy(Stream source,Stream target,Action check,long maximum){var buffer=new byte[1024*1024];long done=0;int count;while((count=source.Read(buffer))>0){check();done+=count;Need(done<=maximum,"Input exceeds its declared size.");target.Write(buffer,0,count);}Need(done==maximum,"Truncated input.");}
    static void CopyFile(string from,string to,Action check){Safe(from);Safe(to);Need(!File.Exists(to),"Duplicate payload filename: "+to);Directory.CreateDirectory(Path.GetDirectoryName(to)!);using var input=File.OpenRead(from);using var output=File.Create(to);Copy(input,output,check,input.Length);}
    static IEnumerable<string> Walk(string root,int depth=0){Need(depth<32,"Input folders are nested too deeply.");Safe(root);foreach(var entry in Directory.EnumerateFileSystemEntries(root)){Safe(entry);if(Directory.Exists(entry)){foreach(var file in Walk(entry,depth+1))yield return file;}else yield return entry;}}
    static void CopyFolder(string from,string to,Action check){var count=0;long bytes=0;foreach(var file in Walk(from)){Need(++count<=16384&&(bytes+=new FileInfo(file).Length)<=16L*1024*1024*1024,"Mod folder exceeds authoring limits.");CopyFile(file,Path.Combine(to,Relative(Path.GetRelativePath(from,file))),check);}}
    static void Unpack(string input,string destination,Action<string,int,int> progress,Action check){
        Need(new FileInfo(input).Length<=8L*1024*1024*1024,"Archive exceeds 8 GiB.");Safe(input);
        using var archive=ArchiveFactory.Open(input);var entries=archive.Entries.ToArray();Need(entries.Length<=16384,"Archive has too many entries.");
        var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);long bytes=0;
        foreach(var entry in entries){Relative(entry.Key??"");Need(names.Add(entry.Key!.Replace('\\','/').TrimEnd('/'))&&!entry.IsEncrypted&&string.IsNullOrEmpty(entry.LinkTarget)&&((entry.Attrib??0)&1024)==0&&(((entry.Attrib??0)>>16)&61440)!=40960,"Duplicate, encrypted or linked archive entry.");Need(entry.Size>=0&&entry.Size<=8L*1024*1024*1024&&(bytes+=entry.Size)<=16L*1024*1024*1024,"Archive exceeds expanded size limits.");}
        var done=0;
        void Extract(string name,bool directory,long size,Func<Stream> open){
            check();var path=Path.Combine(destination,Relative(name));Safe(path);progress("Unpacking mod archive",done++,entries.Length);
            if(directory){Directory.CreateDirectory(path);return;}
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);using var source=open();using var target=File.Create(path);Copy(source,target,check,size);
        }
        if(archive.Type==SharpCompress.Common.ArchiveType.SevenZip||archive.IsSolid){using var reader=archive.ExtractAllEntries();while(reader.MoveToNextEntry())Extract(reader.Entry.Key!,reader.Entry.IsDirectory,reader.Entry.Size,()=>reader.OpenEntryStream());}
        else foreach(var entry in entries)Extract(entry.Key!,entry.IsDirectory,entry.Size,()=>entry.OpenEntryStream());
    }
    internal static void Prepare(string requestFile,string destination,Action<string,int,int> progress,Action check){
        var request=JObject.Parse(File.ReadAllText(requestFile));var metadata=(JObject)request["Metadata"]!;var identity=Identity(metadata);
        var inputs=request["Inputs"]!.Values<string>().ToArray();Need(inputs.Length is >0 and <=64,"Choose 1..64 source files/folders.");Need(!Directory.Exists(destination),"Creation staging already exists.");Directory.CreateDirectory(destination);
        var scratch=Path.Combine(destination,".unpack");var index=0;var warnings=new List<string>();
        foreach(var input in inputs){check();Safe(input!);var source=input!;string? archiveRoot=null;var sourceName=Path.GetFileNameWithoutExtension(source);
            if(File.Exists(source)&&Path.GetExtension(source).ToLowerInvariant() is ".zip" or ".7z"){
                var expanded=Path.Combine(scratch,(index++).ToString());archiveRoot=expanded;Unpack(source,expanded,progress,check);source=expanded;
                var directories=Directory.EnumerateDirectories(source,"*",SearchOption.AllDirectories).Prepend(source).ToArray();
                var owners=directories.Where(d=>Directory.GetParent(d)?.Name.Equals("mods",StringComparison.OrdinalIgnoreCase)==true&&Directory.GetParent(d)?.Parent?.Name.Equals("RuneSchema",StringComparison.OrdinalIgnoreCase)==true).ToArray();
                Need(owners.Length<=1,"Archive contains multiple RuneSchema mods. Package one mod at a time.");
                var nativeRoots=directories.Where(d=>Path.GetFileName(d).Equals("Mods",StringComparison.OrdinalIgnoreCase)&&Directory.GetParent(d)?.Name.Equals("ue4ss",StringComparison.OrdinalIgnoreCase)==true).ToArray();
                foreach(var nativeRoot in nativeRoots)foreach(var companion in Directory.GetDirectories(nativeRoot).Where(d=>!Path.GetFileName(d).Equals("RuneSchema",StringComparison.OrdinalIgnoreCase))){
                    var native=NativeDll(companion);var name=Relative(Path.GetFileName(companion));CopyFolder(companion,Path.Combine(destination,native?"native-ue4ss":"mod/ue4ss",name),check);
                    if(native)warnings.Add("[RS-CACHE-015] "+name+": native UE4SS DLL retained in its normal ue4ss/Mods path. Verify relative paths and RuneSchema integration; DLL subloading is not assumed.");
                }
                var candidates=directories.Where(d=>Loaders.Any(l=>Directory.Exists(Path.Combine(d,l)))).ToArray();
                if(owners.Length==1)source=owners[0];
                else if(nativeRoots.Length>0){
                    foreach(var pak in Walk(expanded).Where(f=>new[]{".pak",".utoc",".ucas"}.Contains(Path.GetExtension(f).ToLowerInvariant())&&!nativeRoots.Any(n=>f.StartsWith(n+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))))CopyFile(pak,Path.Combine(destination,"mod/paks",Path.GetFileName(pak)),check);
                    continue;
                }
                else if(candidates.Length==1)source=candidates[0];
                else if(candidates.Length>1)throw new InvalidDataException("Archive contains multiple possible RuneSchema mods. Package one mod at a time.");
                else if(!Directory.Exists(Path.Combine(source,"Scripts"))&&!Directory.Exists(Path.Combine(source,"dlls"))){var roots=Directory.GetDirectories(source);if(roots.Length==1&&Directory.GetFiles(source).Length==0)source=roots[0];}
            }
            var mod=Path.Combine(destination,"mod");
            if(Directory.Exists(source)){
                var nativeRoot=Directory.Exists(Path.Combine(source,"Mods"))?Path.Combine(source,"Mods"):Path.GetFileName(source).Equals("Mods",StringComparison.OrdinalIgnoreCase)&&Directory.GetParent(source)?.Name.Equals("ue4ss",StringComparison.OrdinalIgnoreCase)==true?source:null;
                if(nativeRoot is not null){
                    foreach(var companion in Directory.GetDirectories(nativeRoot).Where(d=>!Path.GetFileName(d).Equals("RuneSchema",StringComparison.OrdinalIgnoreCase))){
                        var native=NativeDll(companion);var name=Relative(Path.GetFileName(companion));CopyFolder(companion,native?Path.Combine(destination,"native-ue4ss",name):Path.Combine(mod,"ue4ss",name),check);
                        if(native)warnings.Add("[RS-CACHE-015] "+name+": native UE4SS DLL kept in its normal loader folder. Verify file paths and RuneSchema integration before distributing.");
                    }
                    if(File.Exists(Path.Combine(source,"UE4SS.dll")))warnings.Add("The UE4SS runtime itself was left out. Only its mod folders were converted; existing runtime DLLs are unchanged.");
                    continue;
                }
                var lua=Directory.Exists(Path.Combine(source,"Scripts"))||Directory.Exists(Path.Combine(source,"dlls"))||File.Exists(Path.Combine(source,"main.lua"))||File.Exists(Path.Combine(source,"main.dll"));
                var leaf=Relative(source==archiveRoot?sourceName:Path.GetFileName(source));var target=lua?Path.Combine(mod,"ue4ss",leaf):Loaders.Contains(leaf,StringComparer.OrdinalIgnoreCase)?Path.Combine(mod,leaf):mod;
                CopyFolder(source,target,check);
            }else{Need(File.Exists(source),"Source file is missing.");var ext=Path.GetExtension(source).ToLowerInvariant();Need(ext is ".pak" or ".utoc" or ".ucas" or ".png","Drop a mod archive/folder or loose PAK trio/artwork.");CopyFile(source,Path.Combine(mod,ext==".png"?"background.png":"paks/"+Path.GetFileName(source)),check);}
        }
        var root=Path.Combine(destination,"mod");Directory.CreateDirectory(Path.Combine(root,"paks"));
        var scripts=Path.Combine(root,"ue4ss");
        if(Directory.Exists(scripts))foreach(var native in Directory.GetDirectories(scripts).Where(NativeDll)){
            var name=Path.GetFileName(native);Need(!name.Equals("RuneSchema",StringComparison.OrdinalIgnoreCase),"A native UE4SS mod cannot replace RuneSchema.");
            var target=Path.Combine(destination,"native-ue4ss",Relative(name));Directory.CreateDirectory(Path.GetDirectoryName(target)!);Directory.Move(native,target);
            warnings.Add("[RS-CACHE-015] "+name+": native UE4SS DLL preserved in ue4ss/Mods. DLL subloading is not assumed; verify its file paths and RuneSchema integration before distribution.");
        }
        // Normalize loose cooked files while preserving existing loader subfolders.
        foreach(var file in Walk(root).Where(f=>new[]{".pak",".utoc",".ucas"}.Contains(Path.GetExtension(f).ToLowerInvariant())).ToArray()){
            var relative=Path.GetRelativePath(root,file);if(relative.StartsWith("paks"+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase)||relative.StartsWith("ue4ss"+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))continue;
            var target=Path.Combine(root,"paks",Path.GetFileName(file));CopyFile(file,target,check);File.Delete(file);
        }
        var paks=Walk(Path.Combine(root,"paks")).ToArray();
        foreach(var companion in paks.Where(f=>Path.GetExtension(f).ToLowerInvariant() is ".utoc" or ".ucas"))foreach(var extension in new[]{".pak",".utoc",".ucas"})Need(paks.Contains(Path.ChangeExtension(companion,extension),StringComparer.OrdinalIgnoreCase),"Incomplete cooked PAK trio: "+Path.GetFileName(companion));
        File.WriteAllText(Path.Combine(root,"ID.txt"),identity);
        File.WriteAllText(Path.Combine(destination,"creation.json"),new JObject{{"Warnings",new JArray(warnings)}}.ToString());
        if(Directory.Exists(scratch))Directory.Delete(scratch,true);
    }
    internal static void Package(string mod,string platform,string output,string id,Action<string,int,int> progress,Action check){
        Need(platform is "Win64" or "WinGDK","Choose Steam/Epic/Server or Game Pass packaging.");Identity(new JObject{{"ModID",id},{"Author","validation"}});Safe(output);Need(!File.Exists(output),"Output ZIP already exists.");
        var files=Walk(mod).Select(file=>(File:file,Entry:$"Binaries/{platform}/ue4ss/Mods/RuneSchema/mods/{id}/{Relative(Path.GetRelativePath(mod,file))}")).ToList();
        var native=Path.Combine(Path.GetDirectoryName(mod)!,"native-ue4ss");if(Directory.Exists(native))files.AddRange(Walk(native).Select(file=>(File:file,Entry:$"Binaries/{platform}/ue4ss/Mods/{Relative(Path.GetRelativePath(native,file))}")));
        Need(files.Count<=16384&&files.Sum(f=>new FileInfo(f.File).Length)<=16L*1024*1024*1024,"Packaged mod exceeds authoring limits.");
        var temporary=output+".partial-"+Guid.NewGuid().ToString("N");
        try{using(var zip=System.IO.Compression.ZipFile.Open(temporary,ZipArchiveMode.Create)){
            for(var i=0;i<files.Count;i++){check();progress("Packaging RuneSchema mod",i,files.Count);var entry=zip.CreateEntry(files[i].Entry,CompressionLevel.Fastest);using var source=File.OpenRead(files[i].File);using var target=entry.Open();Copy(source,target,check,source.Length);}
            var marker=zip.CreateEntry("Content/Paks/~mods/runeschema.txt");using var writer=new StreamWriter(marker.Open());writer.Write("RuneSchema mod package. Cooked files are managed from Binaries/"+platform+"/ue4ss/Mods/RuneSchema/mods/. Keep both Binaries and Content roots when deploying with a mod manager.\n");
        }check();File.Move(temporary,output);}
        finally{if(File.Exists(temporary))File.Delete(temporary);}
    }
}
'@
[IO.File]::WriteAllText((Join-Path $stage 'CreationArchive.cs'), $builderSource, [Text.UTF8Encoding]::new($false))
$builderSource=@'
using System.IO.Compression;
using Newtonsoft.Json.Linq;
static class CreationArchiveTests
{
    static void Check(bool ok,string reason){if(!ok)throw new Exception(reason);}
    static void Refuse(Action action){try{action();}catch(InvalidDataException){return;}throw new Exception("Unsafe creation input accepted.");}
    internal static void Run(){
        var root=Path.Combine(Path.GetTempPath(),"RS-creation-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        var metadata=new JObject{{"ModID","Example"},{"Name","Example mod"},{"Author","Test author"},{"Version","informational"}};
        void Zip(string file,params string[] names){using var archive=ZipFile.Open(file,ZipArchiveMode.Create);foreach(var name in names){var entry=archive.CreateEntry(name);using var writer=new StreamWriter(entry.Open());writer.Write("inert fixture: "+name);}}
        try{
            foreach(var name in new[]{"../escape","/escape","C:/escape","a/../b","CON.txt","a:stream","a/","a\\..\\b"})if(name!="a/")Refuse(()=>CreationArchive.Relative(name));
            var bad=(JObject)metadata.DeepClone();bad["Author"]="Author\nModID=wrong";Refuse(()=>CreationArchive.Identity(bad));
            bad=(JObject)metadata.DeepClone();bad["ModID"]="CON";Refuse(()=>CreationArchive.Identity(bad));
            Check(CreationArchive.Identity(metadata).Contains("Version=informational"),"Version labels must remain informational");
            var input=Path.Combine(root,"input.zip");Zip(input,"Example/paks/Test.pak","Example/paks/Test.utoc","Example/paks/Test.ucas","Example/items/items.jsonc","Example/ue4ss/ScriptMod/Scripts/main.lua","Example/ue4ss/NativeMod/dlls/main.dll");
            var original=File.ReadAllBytes(input);var request=Path.Combine(root,"request.json");File.WriteAllText(request,new JObject{{"Inputs",new JArray(input)},{"Metadata",metadata}}.ToString());
            var prepared=Path.Combine(root,"prepared");CreationArchive.Prepare(request,prepared,(_,_,_)=>{},()=>{});
            Check(File.ReadAllText(Path.Combine(prepared,"mod/ID.txt")).Contains("ModID=Example"),"ID.txt output missing");
            Check(File.Exists(Path.Combine(prepared,"native-ue4ss/NativeMod/dlls/main.dll")),"Native mod must retain its own UE4SS path");
            Check(!Directory.Exists(Path.Combine(prepared,"mod/ue4ss/NativeMod")),"Native DLL must not be silently subloaded");
            Check(File.ReadAllText(Path.Combine(prepared,"creation.json")).Contains("RS-CACHE-015"),"Native DLL warning missing");
            foreach(var platform in new[]{"Win64","WinGDK"}){
                var output=Path.Combine(root,platform+".zip");CreationArchive.Package(Path.Combine(prepared,"mod"),platform,output,"Example",(_,_,_)=>{},()=>{});
                using var zip=ZipFile.OpenRead(output);var paths=zip.Entries.Select(e=>e.FullName).ToHashSet();
                Check(paths.Contains("Content/Paks/~mods/runeschema.txt"),"Content deployment marker missing");
                Check(paths.Contains($"Binaries/{platform}/ue4ss/Mods/RuneSchema/mods/Example/items/items.jsonc"),"Loader folder was lost");
                Check(paths.Contains($"Binaries/{platform}/ue4ss/Mods/NativeMod/dlls/main.dll"),"Native UE4SS package path is wrong");
                File.WriteAllText(request,new JObject{{"Inputs",new JArray(output)},{"Metadata",metadata}}.ToString());
                var roundtrip=Path.Combine(root,"roundtrip-"+platform);CreationArchive.Prepare(request,roundtrip,(_,_,_)=>{},()=>{});
                Check(File.Exists(Path.Combine(roundtrip,"mod/items/items.jsonc"))&&File.Exists(Path.Combine(roundtrip,"native-ue4ss/NativeMod/dlls/main.dll")),"Binaries/Content archives must round-trip without losing loaders or native companions");
            }
            var stopped=Path.Combine(root,"stopped.zip");var cancelled=false;
            try{CreationArchive.Package(Path.Combine(prepared,"mod"),"Win64",stopped,"Example",(_,_,_)=>{},()=>throw new OperationCanceledException());}catch(OperationCanceledException){cancelled=true;}
            Check(cancelled&&!File.Exists(stopped)&&!Directory.GetFiles(root,"*.partial-*").Any(),"Stopped packaging left a published or partial ZIP");
            Check(File.ReadAllBytes(input).SequenceEqual(original),"Original archive was changed");
            var loose=Path.Combine(root,"items");Directory.CreateDirectory(loose);File.WriteAllText(Path.Combine(loose,"loose.jsonc"),"{}");
            File.WriteAllText(request,new JObject{{"Inputs",new JArray(loose)},{"Metadata",metadata}}.ToString());
            var converted=Path.Combine(root,"converted");CreationArchive.Prepare(request,converted,(_,_,_)=>{},()=>{});
            Check(File.Exists(Path.Combine(converted,"mod/items/loose.jsonc")),"Loose loader folder must keep its loader name");
            var ue4ss=Path.Combine(root,"ue4ss");Directory.CreateDirectory(Path.Combine(ue4ss,"Mods/Lua/Scripts"));Directory.CreateDirectory(Path.Combine(ue4ss,"Mods/Native/dlls"));
            File.WriteAllText(Path.Combine(ue4ss,"Mods/Lua/Scripts/main.lua"),"inert Lua fixture");File.WriteAllText(Path.Combine(ue4ss,"Mods/Native/dlls/main.dll"),"inert DLL fixture");File.WriteAllText(Path.Combine(ue4ss,"UE4SS.dll"),"inert runtime");
            File.WriteAllText(request,new JObject{{"Inputs",new JArray(ue4ss)},{"Metadata",metadata}}.ToString());var convertedUe4ss=Path.Combine(root,"converted-ue4ss");CreationArchive.Prepare(request,convertedUe4ss,(_,_,_)=>{},()=>{});
            Check(File.Exists(Path.Combine(convertedUe4ss,"mod/ue4ss/Lua/Scripts/main.lua"))&&File.Exists(Path.Combine(convertedUe4ss,"native-ue4ss/Native/dlls/main.dll")),"Loose UE4SS runtime folders must consolidate script mods and retain native mods separately");
            Check(!File.Exists(Path.Combine(convertedUe4ss,"mod/UE4SS.dll")),"Conversion must not redistribute or replace the UE4SS runtime");
            foreach(var names in new[]{new[]{"../escape.pak"},new[]{"A.pak","a.PAK"},new[]{"a.pak","a.pak/child"}}){
                var malicious=Path.Combine(root,Guid.NewGuid()+".zip");Zip(malicious,names);File.WriteAllText(request,new JObject{{"Inputs",new JArray(malicious)},{"Metadata",metadata}}.ToString());
                var refused=false;try{CreationArchive.Prepare(request,Path.Combine(root,Guid.NewGuid().ToString()),(_,_,_)=>{},()=>{});}catch(InvalidDataException){refused=true;}catch(IOException){refused=true;}Check(refused,"Unsafe archive was accepted");
            }
            Console.WriteLine("Creation tests passed: metadata, inert loaders, native DLL preservation, both platforms, traversal, duplicates, cancellation and original retention.");
        }finally{Directory.Delete(root,true);}
    }
}
'@
[IO.File]::WriteAllText((Join-Path $stage 'CreationArchive.Tests.cs'), $builderSource, [Text.UTF8Encoding]::new($false))
$builderSource=@'
{
  "version": 1,
  "dependencies": {
    "net10.0": {
      "AssetRipper.TextureDecoder": {
        "type": "Direct",
        "requested": "[2.6.2, )",
        "resolved": "2.6.2",
        "contentHash": "amxJyySJVacy0vv+uPAmj6KoHlNuwZmZVd4TuBMCESQB2wwvH37gLzzo9/0zU6dPXbxXbSQDG1U7slOx50DtZw=="
      },
      "CUE4Parse": {
        "type": "Direct",
        "requested": "[1.2.2.202609, )",
        "resolved": "1.2.2.202609",
        "contentHash": "TyZXxkTsuYrJU17tB9ryfdpakvV1KRNpSbnVtlhdFVnO4jJ1ZNAb6lfAt2gWHNiuWNu3k9C+YSYVo51aP5hfWg==",
        "dependencies": {
          "Blake3": "3.0.2",
          "BouncyCastle.Cryptography": "2.6.2",
          "FixedMathSharp": "4.0.1",
          "Fmod5Sharp": "3.1.0",
          "GenericReader": "2.3.0",
          "Infrablack.UE4Config": "0.7.2.97",
          "K4os.Compression.LZ4.Streams": "1.3.8",
          "LZMA-SDK": "22.1.1",
          "Newtonsoft.Json": "13.0.4",
          "OffiUtils": "3.2.0",
          "Oodle.NET": "2.2.0",
          "OodleSharp": "0.0.1",
          "Serilog": "4.3.1",
          "Serilog.Sinks.Console": "6.1.1",
          "SubstreamSharp": "1.0.3",
          "System.IO.Hashing": "10.0.8",
          "System.Numerics.Tensors": "10.0.10",
          "VGAudio": "2.2.1",
          "Zlib-ng.NET": "1.2.0",
          "ZstdSharp.Port": "0.8.8"
        }
      },
      "Microsoft.Bcl.Memory": {
        "type": "Direct",
        "requested": "[10.0.10, )",
        "resolved": "10.0.10",
        "contentHash": "/PxGfVdy8P15x9TUyAyWfzR92DcRVRCMs4nMnVS6bldRaDTJJPG9ECJPlF0lbkeZdqwM5eSkOQ214YwvcimAWQ=="
      },
      "SharpCompress": {
        "type": "Direct",
        "requested": "[1.0.0, )",
        "resolved": "1.0.0",
        "contentHash": "bdBq9V2epw7KBX3TeA+lxnKU8dD61tsqVuyuo5cAXj4d+9+fcIQObTr0705oTWnPDRhjRx3TpgOFbYYAMObpaw=="
      },
      "SkiaSharp": {
        "type": "Direct",
        "requested": "[2.88.9, )",
        "resolved": "2.88.9",
        "contentHash": "3MD5VHjXXieSHCleRLuaTXmL2pD0mB7CcOB1x2kA1I4bhptf4e3R27iM93264ZYuAq6mkUyX5XbcxnZvMJYc1Q==",
        "dependencies": {
          "SkiaSharp.NativeAssets.Win32": "2.88.9",
          "SkiaSharp.NativeAssets.macOS": "2.88.9"
        }
      },
      "Blake3": {
        "type": "Transitive",
        "resolved": "3.0.2",
        "contentHash": "THvWhdj8ErSdrGamTqjshCU1ImANfr7ieKY6eFqeMsRenvXEKut9qGm5/MA4SUk8IYvAahf67jSh0yVj9jTPzg=="
      },
      "BouncyCastle.Cryptography": {
        "type": "Transitive",
        "resolved": "2.6.2",
        "contentHash": "7oWOcvnntmMKNzDLsdxAYqApt+AjpRpP2CShjMfIa3umZ42UQMvH0tl1qAliYPNYO6vTdcGMqnRrCPmsfzTI1w=="
      },
      "CommunityToolkit.HighPerformance": {
        "type": "Transitive",
        "resolved": "8.4.2",
        "contentHash": "2hmtrrU18x6NL3mbuAqwy0KpnoEEJOuK6sH5RKQfD/jHijn2pqvqBUC7WGy9dgNBL9BDA7eOC0kirsNPMp2f4Q=="
      },
      "FixedMathSharp": {
        "type": "Transitive",
        "resolved": "4.0.1",
        "contentHash": "eUZzaWGXfPitqyJvF+6IFGaYRwWfg65Ia6I7bRbcF7U8Yw2BKBQ1vFl6qE/NGKaapMBVjQ+liDkvRSkwMuRR3w==",
        "dependencies": {
          "MemoryPack": "1.21.4"
        }
      },
      "Fmod5Sharp": {
        "type": "Transitive",
        "resolved": "3.1.0",
        "contentHash": "DPM710gixvM3GrF5FTceXgaAZmH4B3amo92HNIJHiXHUJdeIF3R/cZKDrZzc4A5gzX/M1MbqWBphR90e/b6HxQ==",
        "dependencies": {
          "IndexRange": "1.1.0",
          "NAudio.Core": "2.2.1",
          "OggVorbisEncoder": "1.2.2"
        }
      },
      "GenericReader": {
        "type": "Transitive",
        "resolved": "2.3.0",
        "contentHash": "JeTTJenG7Ys+d+qSpO5xlg07oCC7Xg+tCNYuJOzBcw3v1IPivsbix9jF0odM8EtviL74vC/zRE3V/tm6WPnxZw=="
      },
      "IndexRange": {
        "type": "Transitive",
        "resolved": "1.1.0",
        "contentHash": "fG+2WP1e3Fg3ZVq85XAdiwG9t0twi3r0LvOrTjtbH/31Rd5gLnV5APOD2JHAowbZYDgj9D3rXRbUvIL+ew0WZQ==",
        "dependencies": {
          "Microsoft.Bcl.Memory": "9.0.0"
        }
      },
      "Infrablack.UE4Config": {
        "type": "Transitive",
        "resolved": "0.7.2.97",
        "contentHash": "ZjCEAqkSSr2Z8Rp82wo6ME98WJ0P2DyKrr+11X2KqZ/q/rzloyv6rJ+m45J2qWY1Piu2E9XcJ2gvRHQCapuTQQ=="
      },
      "K4os.Compression.LZ4": {
        "type": "Transitive",
        "resolved": "1.3.8",
        "contentHash": "LhwlPa7c1zs1OV2XadMtAWdImjLIsqFJPoRcIWAadSRn0Ri1DepK65UbWLPmt4riLqx2d40xjXRk0ogpqNtK7g=="
      },
      "K4os.Compression.LZ4.Streams": {
        "type": "Transitive",
        "resolved": "1.3.8",
        "contentHash": "P15qr8dZAeo9GvYbUIPEYFQ0MEJ0i5iqr37wsYeRC3la2uCldOoeCa6to0CZ1taiwxIV+Mk8NGuZi+4iWivK9w==",
        "dependencies": {
          "K4os.Compression.LZ4": "1.3.8",
          "K4os.Hash.xxHash": "1.0.8"
        }
      },
      "K4os.Hash.xxHash": {
        "type": "Transitive",
        "resolved": "1.0.8",
        "contentHash": "Wp2F7BamQ2Q/7Hk834nV9vRQapgcr8kgv9Jvfm8J3D0IhDqZMMl+a2yxUq5ltJitvXvQfB8W6K4F4fCbw/P6YQ=="
      },
      "LZMA-SDK": {
        "type": "Transitive",
        "resolved": "22.1.1",
        "contentHash": "lxG8/H53OCqebnDFlTStwzbgtmUN/DqnXN8kffa/ctHcSEHhIPDyEa3jLzpZxVX5F8bmJgvb1mna6gnhetykIw=="
      },
      "MemoryPack": {
        "type": "Transitive",
        "resolved": "1.21.4",
        "contentHash": "wy3JTBNBsO8LfQcBvvYsWr3lm2Oakolrfu0UQ3oSJSEiD+7ye0GUhYTaXuYYBowqsXBXWD9gf2218ae0JRiYVQ==",
        "dependencies": {
          "MemoryPack.Core": "1.21.4",
          "MemoryPack.Generator": "1.21.4"
        }
      },
      "MemoryPack.Core": {
        "type": "Transitive",
        "resolved": "1.21.4",
        "contentHash": "6RszGorZ0ejNmp37ZcboPBMvvPCuNW2jlrdQfcs/lMzE5b3pmPF6hsm/laDc34hRlbAST1ZxaX/DvYu2DF5sBQ=="
      },
      "MemoryPack.Generator": {
        "type": "Transitive",
        "resolved": "1.21.4",
        "contentHash": "g14EsSS85yn0lHTi0J9ivqlZMf09A2iI51fmI+0KkzIzyCbWOBWPi5mdaY7YWmXprk12aYh9u/qfWHQUYthlwg=="
      },
      "Microsoft.NETCore.Platforms": {
        "type": "Transitive",
        "resolved": "1.1.0",
        "contentHash": "kz0PEW2lhqygehI/d6XsPCQzD7ff7gUJaVGPVETX611eadGsA3A877GdSlU0LRVMCTH/+P3o2iDTak+S08V2+A=="
      },
      "NAudio.Core": {
        "type": "Transitive",
        "resolved": "2.2.1",
        "contentHash": "GgkdP6K/7FqXFo7uHvoqGZTJvW4z8g2IffhOO4JHaLzKCdDOUEzVKtveoZkCuUX8eV2HAINqi7VFqlFndrnz/g=="
      },
      "NETStandard.Library": {
        "type": "Transitive",
        "resolved": "1.6.1",
        "contentHash": "WcSp3+vP+yHNgS8EV5J7pZ9IRpeDuARBPN28by8zqff1wJQXm26PVU8L3/fYLBJVU7BtDyqNVWq2KlCVvSSR4A==",
        "dependencies": {
          "Microsoft.NETCore.Platforms": "1.1.0"
        }
      },
      "Newtonsoft.Json": {
        "type": "Transitive",
        "resolved": "13.0.4",
        "contentHash": "pdgNNMai3zv51W5aq268sujXUyx7SNdE2bj1wZcWjAQrKMFZV260lbqYop1d2GM67JI1huLRwxo9ZqnfF/lC6A=="
      },
      "OffiUtils": {
        "type": "Transitive",
        "resolved": "3.2.0",
        "contentHash": "sejwMr+CWLla0wzpvQSBSe+xCUk01XeKh39R/aG7a//ZHzp7twxPcnbz634w+Rk3bLRVsuL9Jp49RFw42MGjUw==",
        "dependencies": {
          "CommunityToolkit.HighPerformance": "8.4.2"
        }
      },
      "OggVorbisEncoder": {
        "type": "Transitive",
        "resolved": "1.2.2",
        "contentHash": "MZMa5URyED82nJUfGCbulS/1/Np+zhE2mt3h9TuAPOurVN3rYHEaVc7cKd7IK1vROB4UMdTgSoPCW3EJCNmHJw=="
      },
      "Oodle.NET": {
        "type": "Transitive",
        "resolved": "2.2.0",
        "contentHash": "6+rGT+Y2dBkACr2dQ0+NBm5mpDJ66ec8X2RFLyNjm1pRojMK5DcRiA8iqVdMgYue7OPMUhN7ViSn0iu0X4NmRQ=="
      },
      "OodleSharp": {
        "type": "Transitive",
        "resolved": "0.0.1",
        "contentHash": "8XioZSNsed2bv0ouBOqgHDH1guZepf844lCQGndo4jqzuFAiuurIlZ1my/3vXdN3aXdhe3jPrWQzDjXZqxZH7Q=="
      },
      "Serilog": {
        "type": "Transitive",
        "resolved": "4.3.1",
        "contentHash": "savYe7h5yRlkqBVOwP8cIRDOdqKiPmYCU4W87JH38sBmcKD5EBoXvQIw6bNEvZ/pTe1gsiye3VFCzBsoppGkXQ=="
      },
      "Serilog.Sinks.Console": {
        "type": "Transitive",
        "resolved": "6.1.1",
        "contentHash": "8jbqgjUyZlfCuSTaJk6lOca465OndqOz3KZP6Cryt/IqZYybyBu7GP0fE/AXBzrrQB3EBmQntBFAvMVz1COvAA==",
        "dependencies": {
          "Serilog": "4.0.0"
        }
      },
      "SkiaSharp.NativeAssets.macOS": {
        "type": "Transitive",
        "resolved": "2.88.9",
        "contentHash": "Nv5spmKc4505Ep7oUoJ5vp3KweFpeNqxpyGDWyeEPTX2uR6S6syXIm3gj75dM0YJz7NPvcix48mR5laqs8dPuA=="
      },
      "SkiaSharp.NativeAssets.Win32": {
        "type": "Transitive",
        "resolved": "2.88.9",
        "contentHash": "wb2kYgU7iy84nQLYZwMeJXixvK++GoIuECjU4ECaUKNuflyRlJKyiRhN1MAHswvlvzuvkrjRWlK0Za6+kYQK7w=="
      },
      "SubstreamSharp": {
        "type": "Transitive",
        "resolved": "1.0.3",
        "contentHash": "XV/cpJkajpqg2qMYAZzXFcklLy4KvFEiWQFE+amUAzLzQqk/nyAdQp0gXX2rGz5F/ALkvd8bgKBiDW+QuKOXDQ=="
      },
      "System.IO.Hashing": {
        "type": "Transitive",
        "resolved": "10.0.8",
        "contentHash": "+dJsbPJ3FyUbTZNplFj0RCKePFizmv6ewDV46JE9q/IVH4c3xTCftHfHelLsAKf0jryIPqgMb5GpS0x7TAY3mg=="
      },
      "System.Numerics.Tensors": {
        "type": "Transitive",
        "resolved": "10.0.10",
        "contentHash": "9Ildb4Y9maDztB0ZRGxsNShbpCQ2Tjv2dr4IjskBxpTGhH7aIz1+oC+Hl833ASs/htWwsH6myNe+sRpeyCzeMQ=="
      },
      "VGAudio": {
        "type": "Transitive",
        "resolved": "2.2.1",
        "contentHash": "eg8TusdIdla4XejVolG3da6UtshRQPmjCoTxejBff4sJf0wgI1Ic6vvNiYjb96gX21kvWLONRgyvc3NVHP6TYg==",
        "dependencies": {
          "NETStandard.Library": "1.6.1"
        }
      },
      "Zlib-ng.NET": {
        "type": "Transitive",
        "resolved": "1.2.0",
        "contentHash": "W97KdZxrekUMf9DjO9xSb7YzL0H642Hc3ACqABThY3z5M5+wPeFG3pbdL7ObsLL81ysR3swrXTEydla0tDyD5w=="
      },
      "ZstdSharp.Port": {
        "type": "Transitive",
        "resolved": "0.8.8",
        "contentHash": "UIDFkA5DC3en3tOJ3Mof11yBx9kUQLsaeo/oFdvN6Bbkr9E9+VCqfIadHci8cu+I51K2Sl7XYWVl2R/mA75NBQ=="
      }
    }
  }
}
'@
[IO.File]::WriteAllText((Join-Path $stage 'packages.lock.json'), $builderSource, [Text.UTF8Encoding]::new($false))
        $project=Join-Path $stage 'HelpyCache.csproj'
        Show-CacheProgress 'Restoring the locked builder dependencies' 25
        & $script:DotnetCommand restore $project --locked-mode --verbosity quiet;if($LASTEXITCODE){throw 'Dependency restore failed; current cache retained.'}
        Show-CacheProgress 'Compiling the cache builder' 30
        & $script:DotnetCommand build $project --no-restore --disable-build-servers --configuration Release -p:UseSharedCompilation=false --verbosity quiet;if($LASTEXITCODE){throw 'Builder compilation failed; current cache retained.'}
        $builder=Join-Path $stage 'bin/Release/net10.0/HelpyCache.dll'
        $controlArguments=@();if($ControlFile){$controlArguments=@('--control-file',$ControlFile)}
        if($CreationRequest){
            Show-CacheProgress 'Preparing the mod without executing its code' 32
            $prepared=Join-Path $stage 'prepared'
            & $script:DotnetCommand $builder --prepare-creation $CreationRequest --output $prepared --progress @controlArguments | ForEach-Object {
                if($_ -match '^RS_PROGRESS\|([^|]+)\|(\d+)\|(\d+)$'){Show-CacheProgress $Matches[1] ([int](32+3*[double]$Matches[2]/[Math]::Max(1.0,[double]$Matches[3])))}elseif($_ -eq 'RS_PAUSED'){Check-CacheControl}else{Write-Host $_;Check-CacheControl}
            }
            Check-CacheControl;if($LASTEXITCODE){throw 'Creation input could not be prepared; original files retained. See build.log.'}
            $mod=Join-Path $prepared 'mod';$script:CreationWarnings=@(([IO.File]::ReadAllText((Join-Path $prepared 'creation.json')) | ConvertFrom-Json).Warnings)
            foreach($warning in $script:CreationWarnings){Write-Host $warning -ForegroundColor Yellow}
        }
        $built=Join-Path $stage 'result';$arguments=@('--folder',$mod,'--mod-root',$mod,'--mod-id',$ModId,'--output',$built,'--input-root',$inputRoot,'--progress')
        if($ControlFile){$arguments+=@('--control-file',$ControlFile)}
        $previousBundle=Join-Path $output 'offline-helpy.rsmk-cache'
        if(Test-Path -LiteralPath $previousBundle -PathType Leaf){New-Item -ItemType Directory -Path $built | Out-Null;Copy-Item -LiteralPath $previousBundle -Destination $built}
        $providedMapping=$Mapping;if(!$providedMapping -and $plan.Mapping){$providedMapping=Join-Path $stage 'provided.usmap'}
        foreach($entry in @(@('--game',$Game),@('--mapping',$providedMapping),@('--oodle',$Oodle))){if($entry[1]){$arguments+=@($entry[0],$entry[1].Trim('"'))}}
        Show-CacheProgress 'Finding game files and preparing offline mappings automatically' 35
        & $script:DotnetCommand (Join-Path $stage 'bin/Release/net10.0/HelpyCache.dll') @arguments | ForEach-Object {
            if($_ -match '^RS_PROGRESS\|([^|]+)\|(\d+)\|(\d+)$'){
                $phase=$Matches[1];$fraction=[double]$Matches[2]/[Math]::Max(1.0,[double]$Matches[3])
                $percent=switch($phase){'Hashing inputs'{35+15*$fraction};'Reading items'{50+20*$fraction};'Caching pictures'{70+20*$fraction};default{92}}
                Show-CacheProgress "$phase ($($Matches[2]) / $($Matches[3]))" ([int]$percent)
            }elseif($_ -eq 'RS_PAUSED'){Check-CacheControl}else{Write-Host $_}
        }
        Check-CacheControl
        if($LASTEXITCODE){throw 'Cooked cache build failed; current cache retained. See build.log for details.'}
        if(!(Test-Path -LiteralPath (Join-Path $built 'offline-helpy.rsmk-cache'))){throw 'Builder did not produce a cache bundle.'}
        Show-CacheProgress 'Publishing the completed cache folder' 95
        if($ModDirectory -and !(Test-Path -LiteralPath $mod -PathType Container)){throw 'Mod folder was removed; cache was not published.'}
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
        if($CreationRequest){
            $preparedCache=Join-Path $mod 'cache';$null=Assert-CachePath $preparedCache $stage
            if(Test-Path -LiteralPath $preparedCache){Remove-Item -LiteralPath $preparedCache -Recurse -Force}
            Copy-Item -LiteralPath $output -Destination $mod -Recurse
            Show-CacheProgress 'Packaging for Downloads' 97
            & $script:DotnetCommand $builder --package-creation $mod --platform $Platform --mod-id $ModId --output $PackagePath --progress @controlArguments | ForEach-Object {
                if($_ -match '^RS_PROGRESS\|([^|]+)\|(\d+)\|(\d+)$'){Show-CacheProgress $Matches[1] ([int](97+2*[double]$Matches[2]/[Math]::Max(1.0,[double]$Matches[3])))}elseif($_ -eq 'RS_PAUSED'){Check-CacheControl}else{Write-Host $_;Check-CacheControl}
            }
            if($LASTEXITCODE){Check-CacheControl;throw 'Packaging did not finish; original files retained. See build.log.'}
        }
        Write-CacheStatus 'Succeeded' $(if($CreationRequest){'Mod ZIP ready in Downloads'}else{'Cache ready'}) 100;Write-Host "Cache ready: $output" -ForegroundColor Green
        if($backup){Write-Host "Previous cache preserved: $backup"}
    }finally{
        if($ProgressFile){Stop-Transcript -ErrorAction SilentlyContinue | Out-Null}
        Write-Progress -Id 1 -Activity 'RuneSchema mod cache' -Completed
        if(Test-Path -LiteralPath $stage){$null=Assert-CachePath $stage $tempParent;Remove-Item -LiteralPath $stage -Recurse -Force}
        if(Test-Path -LiteralPath $publish){$null=Assert-CachePath $publish $parent;Remove-Item -LiteralPath $publish -Recurse -Force}
    }
}
if($MyInvocation.InvocationName -ne '.') {try{Invoke-CacheZip}catch{$state=if($_.Exception -is [OperationCanceledException]){'Cancelled'}else{'Failed'};Write-CacheStatus $state $_.Exception.Message $script:CachePercent;Write-Error $_;exit 1}}
