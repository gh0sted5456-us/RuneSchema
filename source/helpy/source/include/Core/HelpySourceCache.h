#pragma once
#include "Core/RuneSchemaDataRoot.h"
#include "Core/NativeContentProviders.h"
#include "Core/HelpyCacheSources.h"
#include "Core/ContentSource.h"
#include "Core/PakDiscovery.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <stdexcept>
#include <string_view>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <set>
#include <string>
#include <windows.h>
namespace PS::HelpySourceCache {
struct Snapshot {
    std::map<std::string,std::string> DisplayNames;
    std::string Key,CatalogKey;std::set<std::string> Files,NativePlugins;
    std::string FileKey;uint64_t IconBase=0;
    std::map<std::string,uint64_t> OwnerHashes;
    bool Authoritative=false,Provisional=false,Complete=true;uint64_t PlanRevision=0,ContentRevision=0;
    std::map<std::string,std::string> SourceKeys;
    std::map<std::string,std::set<std::string>> Owners;
    std::map<std::string,std::set<std::string>> DeclaredOwners;
    std::map<std::string,std::filesystem::path> FilePaths; // Worker-local; never sent to a client or exported.
    std::shared_ptr<const HelpyCacheSources::Plan::AssetOwnerMap> AuthoredAssets;
    std::shared_ptr<const HelpyCacheSources::Plan::AssetOwnerMap> AssetOwners;
    std::shared_ptr<const HelpyCacheSources::Plan::AssetOwnerMap> ContainerAssets,ContainerFiles;
    nlohmann::json Containers=nlohmann::json::object();
};
inline uint64_t Hash(std::string_view value,uint64_t seed=1469598103934665603ull){for(unsigned char c:value){seed^=c;seed*=1099511628211ull;}return seed;}
inline std::string Hex(uint64_t value){char text[17]{};std::snprintf(text,sizeof(text),"%016llx",static_cast<unsigned long long>(value));return text;}
// Icon references/labels can change without changing any verified content.
inline Snapshot Replan(Snapshot result,const HelpyCacheSources::Plan& plan){
    if(result.FileKey.empty()||result.ContentRevision!=plan.ContentRevision||result.Authoritative!=plan.Authoritative)
        throw std::runtime_error("Source content requires fresh discovery");
    result.PlanRevision=plan.Revision;result.Complete=plan.Complete;result.DisplayNames=plan.DisplayNames;
    result.AssetOwners=plan.AssetOwners;result.DeclaredOwners=plan.DeclaredOwners;
    for(const auto& [group,owners]:plan.Owners)result.Owners[group].insert(owners.begin(),owners.end());
    result.SourceKeys.clear();uint64_t hash=std::stoull(result.FileKey,nullptr,16);
    if(plan.Authoritative){auto registered=plan.Registered;if(registered.contains("game"))registered.insert("engine");
        for(const auto& group:registered){uint64_t key=Hash(group,result.IconBase);const auto owners=result.Owners.find(group);
            if(owners!=result.Owners.end())for(const auto& owner:owners->second){const auto found=result.OwnerHashes.find(owner);
                key=Hash(owner+"|"+(found==result.OwnerHashes.end()?"missing":Hex(found->second)),key);}
            result.SourceKeys.emplace(group,Hex(key));
        }
        for(const auto& [group,key]:result.SourceKeys)hash=Hash(group+"|"+key,hash);
    }
    result.Key=Hex(hash);return result;
}
inline std::filesystem::path NativePath(const std::filesystem::path& path){
    auto value=std::filesystem::absolute(path).lexically_normal().wstring();
    if(value.starts_with(L"\\\\?\\"))return value;
    if(value.starts_with(L"\\\\"))return L"\\\\?\\UNC\\"+value.substr(2);
    return L"\\\\?\\"+value;
}
inline Snapshot Inspect(const std::filesystem::path& root,const HelpyCacheSources::Plan& plan={}){
    const auto source=NativePath(root);std::map<std::string,std::string> records;size_t count=0;
    std::map<std::string,std::filesystem::path> filePaths;
    auto authored=std::make_shared<HelpyCacheSources::Plan::AssetOwnerMap>();
    auto activeOwners=HelpyCacheSources::ActiveOwners(plan);
    const auto pakRoots=PakDiscovery::ReadRoots();for(const auto& folder:pakRoots)if(!folder.Owner.empty())activeOwners.insert(folder.Owner);
    const auto binary=source.parent_path().parent_path().parent_path();
    const auto game=binary.parent_path().parent_path();
    const auto add=[&](const std::filesystem::directory_entry& entry){
        if(!entry.is_regular_file()||entry.is_symlink())return;
        const auto attr=GetFileAttributesW(entry.path().c_str());if(attr==INVALID_FILE_ATTRIBUTES||(attr&FILE_ATTRIBUTE_REPARSE_POINT))return;
        auto ext=entry.path().extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        const auto filename=entry.path().filename().string();
        const bool modList=filename=="enabled.txt"||filename=="mods.txt"||filename=="runeschema.txt";
        if(ext!=".json"&&ext!=".jsonc"&&ext!=".ini"&&ext!=".pak"&&ext!=".utoc"&&ext!=".ucas"&&ext!=".uplugin"&&ext!=".dll"&&ext!=".exe"&&ext!=".lua"&&!modList)return;
        // Game Pass can expose the same install via XboxGames and WindowsApps.
        // Compare logical game-relative names, not the caller's absolute alias.
        // The Win64/WinGDK lane remains part of every binary/mod record.
        auto name=entry.path().lexically_normal().lexically_relative(game).generic_string();
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        const auto owner=HelpyCacheSources::FileOwner(name);
        if(plan.Authoritative&&!owner.empty()&&!activeOwners.contains(owner))return;
        if(records.contains(name))return;
        if(++count>100000)throw std::runtime_error("Source cache manifest exceeds its file bound");
        auto record=std::to_string(entry.file_size())+":"+std::to_string(entry.last_write_time().time_since_epoch().count());
        // Small authored files include content, so same-size edits cannot reuse metadata.
        if((ext==".json"||ext==".jsonc"||ext==".ini"||ext==".lua"||ext==".uplugin"||modList)&&entry.file_size()<=4*1024*1024){std::ifstream input(entry.path(),std::ios::binary);if(!input)throw std::runtime_error("Source file is not readable");uint64_t hash=1469598103934665603ull;char data[8192];while(input){input.read(data,sizeof(data));hash=Hash(std::string_view(data,static_cast<size_t>(input.gcount())),hash);}record+=":"+Hex(hash);}
        records[name]=record;
        filePaths[name]=entry.path();
        // A top-level /assets target is an explicit RuneSchema definition,
        // unlike recipes/quests merely referring to a shared mounted asset.
        // Read only bounded authored JSON; never load assets or run mod code.
        if(!owner.empty()&&(ext==".json"||ext==".jsonc")&&name.find("/assets/")!=name.npos&&entry.file_size()<=4*1024*1024){
            try{std::ifstream input(entry.path(),std::ios::binary);const auto doc=nlohmann::json::parse(input,nullptr,true,true);
                if(doc.is_object())for(const auto& [path,body]:doc.items())if(body.is_object()){
                    const auto key=HelpyCacheSources::AssetKey(path);if(key.empty())continue;
                    if(authored->size()>=262144&&!authored->contains(key))break;
                    auto& owners=(*authored)[key];if(owners.size()<16)owners.insert(owner);
                }
            }catch(...){} // A rejected/malformed definition cannot supply attribution.
        }
    };
    const auto tree=[&](const std::filesystem::path& folder){if(!std::filesystem::exists(folder))return;
        for(auto it=std::filesystem::recursive_directory_iterator(folder);it!=std::filesystem::recursive_directory_iterator();++it){const auto& entry=*it;
            const auto attr=GetFileAttributesW(entry.path().c_str());if(attr==INVALID_FILE_ATTRIBUTES)throw std::runtime_error("Source path is unavailable: "+entry.path().lexically_relative(game).generic_string()+" (Windows error "+std::to_string(GetLastError())+")");
            if(entry.is_directory()){const auto name=HelpyCacheSources::Lower(entry.path().filename().string());
                const auto owner=HelpyCacheSources::FileOwner(entry.path().lexically_relative(game).generic_string()+"/");
                if((attr&FILE_ATTRIBUTE_REPARSE_POINT)||name=="runtime"||name=="cache"||name=="saved"||name=="logs"||name=="exports"||name=="settings"
                    ||(plan.Authoritative&&!owner.empty()&&!activeOwners.contains(owner)))it.disable_recursion_pending();continue;}add(entry);
        }
    };
    tree(source.parent_path()); // Enabled UE4SS mods, including RuneSchema content and plugins.
    for(const auto* name:{"settings.ini","settings.jsonc"})if(std::filesystem::is_regular_file(source/"settings"/name))add(std::filesystem::directory_entry(source/"settings"/name));
    if(std::filesystem::is_directory(binary))for(const auto& entry:std::filesystem::directory_iterator(binary))add(entry);
    tree(binary.parent_path().parent_path()/"Content"/"Paks");
    tree(game/"Plugins");tree(game/"GameFeatures");tree(game.parent_path()/"Engine"/"Plugins");
    for(const auto& folder:pakRoots)if(std::filesystem::is_directory(folder.Path))for(const auto& entry:std::filesystem::directory_iterator(folder.Path))add(entry);
    Snapshot result;result.Authoritative=plan.Authoritative;result.Complete=plan.Complete;result.PlanRevision=plan.Revision;result.ContentRevision=plan.ContentRevision;
    result.Owners=plan.Owners;result.DisplayNames=plan.DisplayNames;result.FilePaths=std::move(filePaths);
    result.AuthoredAssets=std::move(authored);
    result.AssetOwners=plan.AssetOwners;
    result.DeclaredOwners=plan.DeclaredOwners;
    // Feature descriptors may be packaged inside the game's own base .pak,
    // not loose .uplugin files. Read only that native container's manifest.
    for(const auto& [name,path]:result.FilePaths)if(name.starts_with("content/paks/")&&name.find('/',13)==name.npos&&name.ends_with(".pak")){
        const auto providers=NativeContentProviders::Read(path,game.filename().string());result.NativePlugins.insert(providers.begin(),providers.end());
    }
    for(const auto& [name,path]:result.FilePaths)if(result.NativePlugins.size()<512
        &&(name.starts_with("plugins/")||name.starts_with("gamefeatures/"))){
        std::string plugin;
        if(name.ends_with(".uplugin"))plugin=HelpyCacheSources::Lower(path.stem().string());
        else if(name.ends_with(".pak")){const auto content=name.find("/content/");
            if(content!=name.npos){const auto owner=name.substr(0,content);plugin=owner.substr(owner.find_last_of('/')+1);}}
        if(!plugin.empty()&&HelpyCacheSources::Source("/"+plugin+"/asset.asset")==plugin)result.NativePlugins.insert(plugin);
    }
    auto containers=std::make_shared<HelpyCacheSources::Plan::AssetOwnerMap>(),containerFiles=std::make_shared<HelpyCacheSources::Plan::AssetOwnerMap>();
    size_t packageReferences=0,containerCount=0;
    for(const auto& [name,path]:result.FilePaths)if(name.ends_with(".pak")||name.ends_with(".utoc")){
        if(++containerCount>4096)throw std::runtime_error("Container discovery exceeds its source limit");
        std::string owner=HelpyCacheSources::FileOwner(name);
        for(const auto& folder:pakRoots)if(path.parent_path().lexically_normal()==NativePath(folder.Path).lexically_normal()&&!folder.Owner.empty())owner=folder.Owner;
        auto discovery=PakDiscovery::Read(path,game.filename().string());
        if(discovery.MetadataOnly&&(!owner.empty()||name!="content/paks/global.utoc")){
            discovery.Complete=false;discovery.MetadataOnly=false;discovery.Error="Global metadata container is outside the native content path";
        }
        result.Containers[name]={{"Owner",owner.empty()?"core":ContentSource::Mod(owner)},{"Status",discovery.MetadataOnly?"MetadataOnly":discovery.Complete?"Indexed":"Unavailable"},
            {"Packages",discovery.Packages.size()},{"IndexKey",discovery.IndexKey},{"Reason",discovery.Error}};
        if(!discovery.Complete)continue;
        records[name]+="|directory-sha256:"+discovery.IndexKey;
        for(const auto& package:discovery.Packages){
            if(++packageReferences>1048576)throw std::runtime_error("Container discovery exceeds its package-reference limit");
            const auto group=HelpyCacheSources::Source(package);
            const auto provider=!owner.empty()?ContentSource::Mod(owner):result.NativePlugins.contains(group)?"plugins/"+group:"core";
            if((*containers)[package].size()>=32||(*containerFiles)[package].size()>=32)throw std::runtime_error("Too many containers supply the same package");
            (*containers)[package].insert(provider);(*containerFiles)[package].insert(name);
            if(!owner.empty())result.Owners[group].insert(owner);
        }
    }
    result.ContainerAssets=std::move(containers);result.ContainerFiles=std::move(containerFiles);
    uint64_t hash=Hash("helpy-cache-pak-v1|"+binary.filename().generic_string());
    for(const auto& [name,record]:records){result.Files.insert(name);hash=Hash(name+"|"+record+"\n",hash);}result.Key=Hex(hash);result.FileKey=result.Key;
    if(plan.Authoritative){
        uint64_t base=Hash("helpy-icons-v4|"+binary.filename().generic_string());
        std::map<std::string,uint64_t> ownerHashes;
        for(const auto& [name,record]:records){const auto owner=HelpyCacheSources::FileOwner(name);
            if(!owner.empty())ownerHashes[owner]=Hash(name+"|"+record+"\n",ownerHashes.contains(owner)?ownerHashes[owner]:Hash(owner));
            else if(ContentSource::NativeContentFile(name)||name.ends_with(".exe")||name.ends_with(".pak")||name.ends_with(".utoc")||name.ends_with(".ucas"))base=Hash(name+"|"+record+"\n",base);
        }
        uint64_t catalog=base;for(const auto& [owner,key]:ownerHashes)catalog=Hash(owner+"|"+Hex(key),catalog);
        result.IconBase=base;result.OwnerHashes=ownerHashes;
        // Retire persisted rows containing the old shared-mount owner list,
        // without invalidating source-local captured icon images.
        result.CatalogKey=Hex(Hash("pak-item-owners-v1",catalog));
    }
    return Replan(std::move(result),plan);
}
inline Snapshot Current;
inline HelpyCacheSources::Plan ItemPlan(){auto plan=HelpyCacheSources::Snapshot();plan.AuthoredAssets=Current.AuthoredAssets;plan.ContainerAssets=Current.ContainerAssets;plan.ContainerFiles=Current.ContainerFiles;
    for(const auto& [source,owners]:Current.Owners)plan.Owners[source].insert(owners.begin(),owners.end());return plan;}
inline std::future<Snapshot> Worker;
inline std::future<nlohmann::json> CachedSourcesWorker;
inline bool CachedSourcesAttempted=false;
inline std::chrono::steady_clock::time_point Next{};
inline uint64_t Revision=0,RemovalRevision=0,CatalogInvalidationRevision=0;
// Discovering another source must not discard the already visible catalogue.
inline bool InvalidatesCatalog(const Snapshot& before,const Snapshot& after){
    if(before.Key.empty())return false;
    if(!before.Authoritative||!after.Authoritative)return before.Key!=after.Key;
    for(const auto& [source,key]:before.SourceKeys)if(!after.SourceKeys.contains(source))return true;
    if(!before.CatalogKey.empty()&&!after.CatalogKey.empty())return before.CatalogKey!=after.CatalogKey;
    for(const auto& [source,key]:before.SourceKeys){const auto found=after.SourceKeys.find(source);
        if(found==after.SourceKeys.end()||found->second!=key)return true;}
    return false;
}
inline std::string Error;
inline std::function<bool(nlohmann::json&)> RemoteSnapshot;
inline std::mutex PublishedMutex;
inline nlohmann::json Published={{"Ready",false}};
inline std::shared_ptr<const HelpyCacheSources::Plan::AssetOwnerMap> PublishedAuthoredAssets;
inline std::shared_ptr<const HelpyCacheSources::Plan::AssetOwnerMap> PublishedContainerAssets,PublishedContainerFiles;
inline Snapshot OwnershipSnapshot(){
    std::lock_guard lock(PublishedMutex);const auto plan=HelpyCacheSources::Snapshot();
    if(!Published.value("Ready",false)||!plan.Complete||Published.value("ContentRevision",uint64_t{})!=plan.ContentRevision)
        throw std::runtime_error("Cache ownership is still being verified");
    Snapshot result;result.Key=Published.at("Key").get<std::string>();result.ContentRevision=plan.ContentRevision;
    result.SourceKeys=Published.at("Sources").get<std::map<std::string,std::string>>();
    result.AssetOwners=plan.AssetOwners;result.AuthoredAssets=PublishedAuthoredAssets;
    result.ContainerAssets=PublishedContainerAssets;result.ContainerFiles=PublishedContainerFiles;return result;
}
inline std::set<std::string> NativePluginRoots(){
    std::lock_guard lock(PublishedMutex);std::set<std::string> result;
    if(const auto values=Published.find("NativePlugins");values!=Published.end()&&values->is_array()&&values->size()<=512)
        for(const auto& value:*values)if(value.is_string())result.insert(value.get<std::string>());
    return result;
}
inline std::string PublishedFingerprint(){std::lock_guard lock(PublishedMutex);return Published.value("Key",std::string{})+"|"+std::to_string(Published.value("PlanRevision",uint64_t{}));}
inline std::atomic<bool> Requested{false};
inline std::string PublishedCatalogKey(){Requested=true;std::lock_guard lock(PublishedMutex);return Published.value("CatalogKey",std::string{});}
inline uint64_t InspectingRevision=0;
inline bool RefreshingFiles=true;
inline bool HexKey(const std::string& key){return key.size()==16&&key.find_first_not_of("0123456789abcdef")==key.npos;}
inline nlohmann::json ForClient(){Requested=true;std::lock_guard lock(PublishedMutex);auto result=Published;
    const auto plan=HelpyCacheSources::Snapshot();const bool checking=result.value("PlanRevision",uint64_t{})!=plan.Revision;
    if(!plan.Complete||(checking&&result.value("ContentRevision",uint64_t{})!=plan.ContentRevision))result["Ready"]=false;
    result["CheckingChanges"]=checking;return result;}
inline nlohmann::json Document(const Snapshot& value){return {{"Version",4},{"Ready",value.Complete&&!value.Key.empty()&&(!value.Authoritative||!value.SourceKeys.empty())},
    {"Authoritative",value.Authoritative},{"PlanRevision",value.PlanRevision},{"ContentRevision",value.ContentRevision},{"Key",value.Key},{"CatalogKey",value.CatalogKey},{"Sources",value.SourceKeys},{"Owners",value.Owners},{"DeclaredOwners",value.DeclaredOwners},{"NativePlugins",value.NativePlugins},
    {"Discovery","ContainerDirectoryIndex"},{"Containers",value.Containers},{"ModNames",value.DisplayNames}};}
inline bool AcceptRemote(const nlohmann::json& value){
    if(!value.is_object()||!value.value("Ready",false))return false;
    if(value.value("Version",0)!=4||!value.value("Authoritative",false))throw std::runtime_error("Icon source manifest requires a matching RuneSchema core");
    Snapshot next;next.Key=value.at("Key").get<std::string>();next.Authoritative=true;next.PlanRevision=value.at("PlanRevision").get<uint64_t>();next.ContentRevision=value.value("ContentRevision",uint64_t{});
    if(!HexKey(next.Key)||!value.at("Sources").is_object()||value.at("Sources").size()>512)throw std::runtime_error("Icon source manifest is invalid");
    if(value.contains("CatalogKey")){if(!value["CatalogKey"].is_string())throw std::runtime_error("Invalid catalogue source key");
        next.CatalogKey=value["CatalogKey"].get<std::string>();if(!next.CatalogKey.empty()&&!HexKey(next.CatalogKey))throw std::runtime_error("Invalid catalogue source key");}
    for(const auto& [group,key]:value.at("Sources").items()){
        if(HelpyCacheSources::Source("/"+group+"/asset.asset")!=group||!key.is_string()||!HexKey(key.get<std::string>()))throw std::runtime_error("Invalid registered icon source");
        next.SourceKeys.emplace(group,key.get<std::string>());
    }
    for(const auto* field:{"Owners","DeclaredOwners"})if(value.contains(field)){
        if(!value[field].is_object()||value[field].size()>512)throw std::runtime_error("Invalid icon source ownership");
        for(const auto& [group,owners]:value[field].items()){
            if(HelpyCacheSources::Source("/"+group+"/asset.asset")!=group||!owners.is_array()||owners.size()>256)throw std::runtime_error("Invalid icon source ownership");
            for(const auto& owner:owners){if(!owner.is_string())throw std::runtime_error("Invalid icon source owner");const auto name=owner.get<std::string>();
                if(name.empty()||name.size()>128||name=="."||name==".."||name.find_first_of("/\\\r\n\t:")!=name.npos)throw std::runtime_error("Invalid icon source owner");(std::string_view(field)=="Owners"?next.Owners:next.DeclaredOwners)[group].insert(name);}
        }
    }
    if(value.contains("NativePlugins")){
        const auto& plugins=value["NativePlugins"];if(!plugins.is_array()||plugins.size()>512)throw std::runtime_error("Invalid native plugin sources");
        for(const auto& plugin:plugins){
            if(!plugin.is_string())throw std::runtime_error("Invalid native plugin source");
            const auto name=plugin.get<std::string>();
            if(name.find('/')!=name.npos||HelpyCacheSources::Source("/"+name+"/asset.asset")!=name)throw std::runtime_error("Invalid native plugin source");
            next.NativePlugins.insert(name);
        }
    }
    if(value.contains("ModNames")){
        if(!value["ModNames"].is_object()||value["ModNames"].size()>512)throw std::runtime_error("Invalid source display names");
        for(const auto& [owner,label]:value["ModNames"].items()){
            if(!ContentSource::Valid(ContentSource::Mod(owner))||!label.is_string()||label.get_ref<const std::string&>().empty()||label.get_ref<const std::string&>().size()>256
                ||std::any_of(label.get_ref<const std::string&>().begin(),label.get_ref<const std::string&>().end(),[](unsigned char c){return c<32||c==127;}))throw std::runtime_error("Invalid source display name");
            next.DisplayNames.emplace(HelpyCacheSources::Lower(owner),label.get<std::string>());
        }
    }
    if(value.contains("Containers")){
        const auto& containers=value.at("Containers");
        if(!containers.is_object()||containers.size()>4096)throw std::runtime_error("Invalid container discovery manifest");
        for(const auto& [file,entry]:containers.items()){
            if(file.empty()||file.size()>2048||file.find_first_of("\\:\r\n\t")!=file.npos||(!file.ends_with(".pak")&&!file.ends_with(".utoc"))
                ||!entry.is_object()||entry.dump().size()>4096||!ContentSource::Valid(entry.value("Owner",std::string{}))
                ||(entry.value("Status",std::string{})!="Indexed"&&entry.value("Status",std::string{})!="Unavailable"&&entry.value("Status",std::string{})!="MetadataOnly")
                ||!entry.at("Packages").is_number_unsigned()||entry.at("Packages").get<uint64_t>()>PakDiscovery::MaxEntries)
                throw std::runtime_error("Invalid container discovery source");
            if(entry.value("Status",std::string{})=="MetadataOnly"&&(file!="content/paks/global.utoc"||entry.at("Owner")!="core"||entry.at("Packages")!=0))
                throw std::runtime_error("Invalid native metadata-only container");
        }
        next.Containers=containers;
    }
    if(next.Key==Current.Key&&next.NativePlugins==Current.NativePlugins&&next.DeclaredOwners==Current.DeclaredOwners&&next.DisplayNames==Current.DisplayNames){
        Current.PlanRevision=next.PlanRevision;Current.ContentRevision=next.ContentRevision;Current.Containers=std::move(next.Containers);return false;
    }
    bool removed=false;for(const auto& [group,key]:Current.SourceKeys)if(!next.SourceKeys.contains(group)){removed=true;break;}
    const bool invalidate=InvalidatesCatalog(Current,next);
    Current=std::move(next);++Revision;if(removed)RemovalRevision=Revision;if(invalidate)CatalogInvalidationRevision=Revision;return true;
}
inline std::filesystem::path Directory(bool liveOnly);
inline void Persist(const Snapshot& value) noexcept {
    try {if(!value.Authoritative||!value.Complete)return;const auto folder=Directory(false);std::filesystem::create_directories(folder);
        const auto target=folder/"sources.json",temporary=folder/("sources-"+std::to_string(GetCurrentProcessId())+".tmp");
        {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);file<<Document(value).dump(2);file.close();if(!file)throw std::runtime_error("Icon source manifest write failed");}
        if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::error_code ec;std::filesystem::remove(temporary,ec);}
    }catch(...) {/* Disk cache failures must not fail the game or register assets. */}
}
// Called only by the owning UI/game thread. The worker only reads files and
// returns a value; it never accesses Unreal, ImGui, or mutable cache state.
inline bool Poll(const std::filesystem::path& root){
    bool changed=false;const auto now=std::chrono::steady_clock::now();
    if(RemoteSnapshot){
        if(!CachedSourcesAttempted){CachedSourcesAttempted=true;const auto file=Directory(false)/"sources.json";
            CachedSourcesWorker=std::async(std::launch::async,[file]{try{if(std::filesystem::file_size(file)>2*1024*1024)return nlohmann::json{};std::ifstream input(file);return nlohmann::json::parse(input);}catch(...){return nlohmann::json{};}});}
        if(CachedSourcesWorker.valid()&&CachedSourcesWorker.wait_for(std::chrono::seconds(0))==std::future_status::ready){
            try{auto value=CachedSourcesWorker.get();if(Current.Key.empty()&&AcceptRemote(value)){Current.Provisional=true;changed=true;}}catch(...){}
        }
        if(now<Next)return changed;Next=now+std::chrono::seconds(2);try{nlohmann::json value;
        if(!RemoteSnapshot(value)||!value.value("Ready",false))return changed;
        const bool provisional=Current.Provisional;const bool accepted=AcceptRemote(value);Current.Provisional=false;
        if(provisional&&!accepted)++Revision;changed=changed||accepted||provisional;Error.clear();return changed;
    }catch(const std::exception& error){Error=error.what();return false;}}
    const auto plan=HelpyCacheSources::Snapshot();
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{
        auto result=Worker.get();if(result.PlanRevision!=plan.Revision){
            if(result.ContentRevision!=plan.ContentRevision||result.FileKey.empty()){Next={};return false;}
            result=Replan(std::move(result),plan);
        }
        if(result.Key!=Current.Key){bool removed=false;for(const auto& path:Current.Files)if(!result.Files.contains(path)){removed=true;break;}
            const bool invalidate=InvalidatesCatalog(Current,result);
            Current=std::move(result);++Revision;if(removed)RemovalRevision=Revision;if(invalidate)CatalogInvalidationRevision=Revision;changed=true;}
        else Current=std::move(result); // A metadata-only revision still completes live source verification.
        Error.clear();
        {std::lock_guard lock(PublishedMutex);Published=Document(Current);PublishedAuthoredAssets=Current.AuthoredAssets;PublishedContainerAssets=Current.ContainerAssets;PublishedContainerFiles=Current.ContainerFiles;}
    }catch(const std::exception& error){Error=error.what();}if(RefreshingFiles||!Error.empty())Next=now+std::chrono::seconds(Error.empty()?60:5);}
    if(!Worker.valid()&&(now>=Next||InspectingRevision!=plan.Revision)){
        const bool reuse=Error.empty()&&now<Next&&!Current.FileKey.empty()&&Current.ContentRevision==plan.ContentRevision&&Current.Authoritative==plan.Authoritative;
        RefreshingFiles=!reuse;
        InspectingRevision=plan.Revision;
        if(reuse)Worker=std::async(std::launch::async,[snapshot=Current,plan]{auto value=Replan(std::move(snapshot),plan);Persist(value);return value;});
        else Worker=std::async(std::launch::async,[root,plan]{auto value=Inspect(root,plan);Persist(value);return value;});}return changed;
}
inline bool Ready(){return Current.Complete&&!Current.Key.empty()&&Error.empty()&&(!Current.Authoritative||!Current.SourceKeys.empty());}
// Call after the UI thread stops, before unloading the DLL. A std::async
// future must not outlive plugin shutdown and wait from a static destructor
// under Windows' loader lock.
inline void Shutdown() noexcept {
    if(Worker.valid())try{(void)Worker.get();}catch(...){}
    if(CachedSourcesWorker.valid())try{(void)CachedSourcesWorker.get();}catch(...){}CachedSourcesAttempted=false;
    Next={};
}
inline std::string Fingerprint(const std::string& icon){if(!Current.Authoritative)return Current.Key;
    const auto found=Current.SourceKeys.find(HelpyCacheSources::Source(icon));return found==Current.SourceKeys.end()?std::string{}:found->second;}
inline std::string Key(const std::string& icon){return Hex(Hash(Fingerprint(icon)+"|"+icon));}
inline std::filesystem::path Directory(bool liveOnly){
    return RuneSchemaData::SharedDirectory("HELPY")/"runtime"/"saved"/"cache"/(liveOnly?"item-icons-live":"item-icons");
}
inline std::filesystem::path IconDirectory(const std::string& icon,bool liveOnly){
    const auto fingerprint=Fingerprint(icon);if(fingerprint.empty())return {};
    if(!Current.Authoritative)return Directory(liveOnly);
    const auto source=HelpyCacheSources::Source(icon);return Directory(liveOnly)/(HelpyCacheSources::Folder(source)+"-"+Hex(Hash(source)))/fingerprint;
}
}
