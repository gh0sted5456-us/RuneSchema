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
