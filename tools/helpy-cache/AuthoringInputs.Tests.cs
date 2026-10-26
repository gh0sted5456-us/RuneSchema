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
