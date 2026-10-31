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
