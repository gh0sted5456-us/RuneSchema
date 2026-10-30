#pragma once
#include "Core/CookedPakRegistryManifest.h"
#include "Core/MountedModRegistryOwners.h"
#include <functional>
#include <set>

namespace PS::CookedRegistryShards {
using Json=nlohmann::json;
// Cooked declarations may grow after source-side splitting (for example,
// additional discovery operands). Keep the bound independent of group limits.
inline constexpr std::size_t MaxDocumentBytes=512*1024;
inline constexpr std::size_t MaxShards=256;
inline constexpr std::size_t MaxGroupBytes=64*1024*1024;
inline constexpr std::size_t MaxGroupReferences=262144;
struct Asset {std::string Owner;Json Document;std::size_t Bytes=0;};
struct Group {
    std::string Owner,Root;
    std::vector<std::string> Paths;
    std::vector<CookedPakRegistryManifest::Manifest> Manifests;
    Json RootEntries=Json::array();
    std::size_t Bytes=0,References=0;
};
inline void Require(bool valid,const std::string& reason){if(!valid)throw std::runtime_error("Cooked registry shards: "+reason);}
inline std::string Fold(std::string text){for(auto& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return text;}
inline std::string RootPath(const std::string& owner){
    Require(MountedModRegistryOwners::ConventionalName(owner),"root owner must match an enabled conventional mod folder");
    const auto name="DA_RuneSchemaRegistry_"+owner;
    return "/Game/Mods/"+owner+"/Registry/"+name+"."+name;
}
inline bool ReservedShardPath(const std::string& path){
    const auto folded=Fold(path);constexpr std::string_view prefix="/game/mods/";
    if(!folded.starts_with(prefix))return false;
    const auto end=folded.find('/',prefix.size());
    return end!=std::string::npos&&folded.substr(end).starts_with("/registry/shards/");
}
inline std::string RegistryFolderOwner(const std::string& path){
    const auto folded=Fold(path);constexpr std::string_view prefix="/game/mods/";
    if(!folded.starts_with(prefix))return {};
    const auto end=folded.find('/',prefix.size());
    if(end==std::string::npos||!folded.substr(end).starts_with("/registry/"))return {};
    const auto owner=path.substr(prefix.size(),end-prefix.size());
    return MountedModRegistryOwners::ConventionalName(owner)?owner:std::string{};
}
inline void ValidateRootPath(const std::string& path,const std::string& owner){
    const auto prefix="/Game/Mods/"+owner+"/Registry/";
    Require(MountedModRegistryOwners::ConventionalName(owner)&&Fold(path).starts_with(Fold(prefix))&&path.size()<=1024,
        "root path must stay in the owning Registry folder");
    const auto suffix=path.substr(prefix.size());const auto dot=suffix.find('.');
    Require(dot!=std::string::npos&&dot>0&&suffix.substr(0,dot)==suffix.substr(dot+1),"root package and object names must match");
    const auto name=suffix.substr(0,dot);
    Require(name.size()<=128&&std::all_of(name.begin(),name.end(),[](unsigned char c){return c<128&&(std::isalnum(c)||c=='_');}),
        "root name must use only letters, digits and underscores");
}
inline void ValidateChildPath(const std::string& path,const std::string& owner){
    const auto prefix="/Game/Mods/"+owner+"/Registry/Shards/";
    Require(Fold(path).starts_with(Fold(prefix))&&path.size()<=1024,"child path must stay in the owning Registry/Shards folder");
    const auto suffix=path.substr(prefix.size());const auto dot=suffix.find('.');
    Require(dot!=std::string::npos&&dot>0&&suffix.substr(0,dot)==suffix.substr(dot+1),"child package and object names must match");
    const auto name=suffix.substr(0,dot);
    Require(!name.empty()&&name.size()<=128
        &&std::all_of(name.begin(),name.end(),[](unsigned char c){return c<128&&(std::isalnum(c)||c=='_');}),
        "child name must use only letters, digits and underscores; ownership comes from its folder and RegistryOwner");
}
inline Json ParseJson(const std::string& text){
    Require(!text.empty()&&text.size()<=MaxDocumentBytes,"document is empty or exceeds the 512 KiB UTF-8 limit (bytes="
        +std::to_string(text.size())+", maximum="+std::to_string(MaxDocumentBytes)+")");
    std::vector<std::set<std::string>> keys;
    const auto check=[&](int depth,Json::parse_event_t event,Json& value){
        Require(depth<=64,"JSON nesting exceeds the safe bound");
        if(event==Json::parse_event_t::object_start)keys.emplace_back();
        else if(event==Json::parse_event_t::key)Require(!keys.empty()&&keys.back().insert(value.get<std::string>()).second,"duplicate JSON field");
        else if(event==Json::parse_event_t::object_end&&!keys.empty())keys.pop_back();
        return true;
    };
    return Json::parse(text,check);
}
inline void Envelope(const Json& document,bool shard){
    Require(document.is_object(),"declaration must be an object");
    for(const auto& [key,value]:document.items()){
        (void)value;
        Require(key=="SchemaVersion"||key=="Entries"||key=="NativeRegistries"||key=="LoaderDefinitions"
            ||(shard?key=="RegistryShard":key=="RegistryShards"),"unsupported document field: "+key);
    }
    Require(document.contains("Entries")&&document.at("Entries").is_array()&&document.at("Entries").size()<=256,"A bounded Entries array is required");
    if(shard)Require(document.contains("RegistryShard")&&document.at("RegistryShard").is_boolean()
        &&document.at("RegistryShard")==true&&document.at("Entries").empty(),"shards require RegistryShard:true and Entries:[]");
}
inline std::size_t References(const CookedPakRegistryManifest::Manifest& manifest){
    std::size_t count=manifest.WearableMeshData.size()+manifest.MeleeAttackClasses.size()+manifest.RangedAttackClasses.size()+manifest.CacheAssets.size();
    for(const auto& lane:manifest.AssetLanes)count+=lane.Paths.size();
    for(const auto& [lane,definitions]:manifest.LoaderDefinitions.items()){(void)lane;count+=definitions.size();}
    return count;
}
inline Group Resolve(const std::string& rootPath,const Asset& root,const std::function<Asset(const std::string&)>& load){
    ValidateRootPath(rootPath,root.Owner);
    Envelope(root.Document,false);
    Require(root.Document.contains("RegistryShards")&&root.Document.at("RegistryShards").is_array()
        &&!root.Document.at("RegistryShards").empty()&&root.Document.at("RegistryShards").size()<=MaxShards,
        "RegistryShards requires 1 to 256 child paths");
    Group result;result.Owner=root.Owner;result.Root=rootPath;result.RootEntries=root.Document.at("Entries");
    std::vector<std::string> children;std::set<std::string> seen;
    // Validate the whole index before invoking any child-loading callback.
    for(const auto& entry:root.Document.at("RegistryShards")){
        Require(entry.is_string(),"child references must be full cooked object paths");
        auto path=entry.get<std::string>();ValidateChildPath(path,root.Owner);
        Require(seen.insert(Fold(path)).second,"duplicate child path (case-insensitive)");children.push_back(std::move(path));
    }
    const auto append=[&](const std::string& path,const Asset& asset,bool child){
        Require(asset.Owner==root.Owner,"child RegistryOwner does not match the root");
        Require(asset.Bytes>0&&asset.Bytes<=MaxDocumentBytes&&asset.Document.dump().size()<=MaxDocumentBytes,"document byte budget is invalid");
        Require(asset.Bytes<=MaxGroupBytes-result.Bytes,"catalogue exceeds 64 MiB");result.Bytes+=asset.Bytes;
        auto payload=asset.Document;payload.erase(child?"RegistryShard":"RegistryShards");
        auto manifest=CookedPakRegistryManifest::Parse(payload,root.Owner,"pak:"+path);
        if(child)Require(manifest.has_value(),"a shard must declare native assets or loader definitions");
        if(manifest){const auto count=References(*manifest);Require(count<=MaxGroupReferences-result.References,"catalogue exceeds 262144 references");
            result.References+=count;result.Manifests.push_back(std::move(*manifest));}
        result.Paths.push_back(path);
    };
    append(rootPath,root,false);
    for(const auto& path:children){
        try{auto child=load(path);Envelope(child.Document,true);append(path,child,true);}
        catch(const std::exception& error){throw std::runtime_error("Registry shard '"+path+"': "+error.what());}
    }
    for(const auto* lane:{"Recipes","Journals","Lore","Buildings"})(void)CookedPakRegistryManifest::MergeDefinitions(result.Manifests,lane);
    return result;
}
}
