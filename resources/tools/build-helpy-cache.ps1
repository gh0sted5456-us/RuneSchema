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
    static void Progress(string phase, int done, int total)
    {
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
                    Need(new[] { "--game", "--mapping", "--oodle", "--folder", "--mod-root", "--mod-id", "--output", "--base-game", "--include-mods", "--locale", "--progress", "--input-root", "--check-inputs" }.Contains(arg), "Unknown option " + arg);
                    Need(!options.ContainsKey(arg), "Repeated option " + arg);
                    if (arg is "--base-game" or "--include-mods" or "--progress" or "--check-inputs") options[arg] = "true";
                    else { Need(index + 1 < args.Length && !args[index + 1].StartsWith("--", StringComparison.Ordinal), "Missing value for " + arg); options[arg] = args[index + 1]; }
                }
            EmitProgress = options.ContainsKey("--progress");
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
            if (!baseMode) Need(Directory.Exists(Path.Combine(modRoot, "paks")), "Mod root must contain paks/. Stage the final mod/paks/<package>/ layout first; --mod-root selects it.");
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

            void CheckInputs() { Closed(game); Unchanged(before, nativeRoots.SelectMany(r => Files(r, NativeExtensions)).Concat(mods.SelectMany(m => Files(m.Root, OwnerExtensions))).Append(mapping).Append(oodle)); }
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
