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
