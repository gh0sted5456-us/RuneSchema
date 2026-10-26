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
    static string Ask(string prompt) { Console.Write(prompt + ": "); return Console.ReadLine()?.Trim().Trim('"') ?? ""; }
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
            if (args.Contains("--help")) { Console.WriteLine("Helpy offline cache builder (Windows, .NET 10)\n--game GAME/RSDragonwilds --mapping matching.usmap --oodle oo2core_9_win64.dll\n[--folder PAK-FOLDER] [--mod-root MOD-ROOT] [--mod-id MOD-ID] [--output DIRECTORY]\n[--base-game [--include-mods]] [--locale en-US]\nWithout arguments, prompts for game/mapping/Oodle. Mod mode is the default.\nNo game, save, PAK or installed Helpy cache is modified. Output is presentation only."); return 0; }
            Need(OperatingSystem.IsWindows(), "This release supports Windows/Git Bash only (native texture decoder and game process checks).");
            var options = new Dictionary<string, string>();
            foreach (var (arg, index) in args.Select((s, i) => (s, i)))
                if (arg.StartsWith("--", StringComparison.Ordinal))
                {
                    Need(new[] { "--game", "--mapping", "--oodle", "--folder", "--mod-root", "--mod-id", "--output", "--base-game", "--include-mods", "--locale" }.Contains(arg), "Unknown option " + arg);
                    Need(!options.ContainsKey(arg), "Repeated option " + arg);
                    if (arg is "--base-game" or "--include-mods") options[arg] = "true";
                    else { Need(index + 1 < args.Length && !args[index + 1].StartsWith("--", StringComparison.Ordinal), "Missing value for " + arg); options[arg] = args[index + 1]; }
                }
            string Input(string key, string prompt) => options.GetValueOrDefault(key) ?? Ask(prompt);
            var game = Full(Input("--game", "Dragonwilds project folder (contains Content/Paks)")); Safe(game); Need(Directory.Exists(Path.Combine(game, "Content", "Paks")), "Game folder must contain Content/Paks");
            Closed(game);
            var mapping = Full(Input("--mapping", "Matching game-version .usmap path")); var oodle = Full(Input("--oodle", "Your local oo2core_9_win64.dll path"));
            Safe(mapping); Safe(oodle); Need(File.Exists(mapping) && Path.GetExtension(mapping).Equals(".usmap", StringComparison.OrdinalIgnoreCase), "A matching .usmap is required"); Need(File.Exists(oodle), "Oodle library not found");
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
            foreach (var file in before.Keys)
            {
                var stamp = before[file]; var previous = memo[file]; var digest = previous?.Value<string>("SHA256") ?? "";
                if (previous?.Value<long>("Size") != stamp.Size || previous?.Value<long>("Stamp") != stamp.Stamp || !Regex.IsMatch(digest, "^[a-f0-9]{64}$"))
                { using var input = File.OpenRead(file); digest = Convert.ToHexStringLower(SHA256.HashData(input)); }
                digests[file] = digest; memo[file] = new JObject { ["Size"] = stamp.Size, ["Stamp"] = stamp.Stamp, ["SHA256"] = digest };
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
                // Include selected cooked UI textures even if this partial catalogue did not reference them.
                foreach (var file in assets.Where(Selected)) if (file.Name.StartsWith("T_", StringComparison.OrdinalIgnoreCase) && (file.Path.Contains("/UI/", StringComparison.OrdinalIgnoreCase) || file.Path.Contains("/Icons/", StringComparison.OrdinalIgnoreCase) || file.Name.StartsWith("T_Icon_", StringComparison.OrdinalIgnoreCase))) try { wantedIcons.Add(Asset(file.Path)); } catch { }
                // Native item/journal providers only; unrelated engine/plugin UI is not an item cache.
                if (mod is null) { var groups = records.Select(r => r.Value<string>("Source")!).ToHashSet(StringComparer.Ordinal); wantedIcons.RemoveWhere(path => !groups.Contains(Source(path))); }
                var icons = new JArray(); using var payload = new MemoryStream();
                foreach (var icon in wantedIcons)
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
                }
                var banner = mod is null ? null : new[] { Path.Combine(mod.Value.Root, "mod.png"), Path.Combine(mod.Value.Root, "banner.png"), Path.Combine(folder, "mod.png"), Path.Combine(folder, "banner.png") }.FirstOrDefault(File.Exists);
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
                    ["CacheVersion"] = revision, ["FingerprintVersion"] = 2, ["Records"] = records.Count, ["Icons"] = icons.Count, ["RecordCoverage"] = "Partial", ["Issues"] = report };
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
