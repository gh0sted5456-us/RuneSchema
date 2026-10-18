#pragma once
#include "Core/ContentSource.h"
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <stdexcept>
namespace PS::ItemSource {
enum class Kind { All, Vanilla, Expansion, Modded, Unknown };
inline constexpr const char* Labels[]{"All sources","Vanilla","Expansion","Modded","Unknown"};
inline const char* Label(Kind kind){return Labels[static_cast<unsigned>(kind)];}
inline const char* Token(Kind kind){switch(kind){case Kind::Vanilla:return "vanilla";case Kind::Expansion:return "expansion";case Kind::Modded:return "modded";case Kind::Unknown:return "unknown";default:return "all";}}
inline Kind Decode(std::string_view value){for(const auto kind:{Kind::All,Kind::Vanilla,Kind::Expansion,Kind::Modded,Kind::Unknown})if(value==Token(kind)||value==Label(kind))return kind;return Kind::Unknown;}
struct Info {Kind Group=Kind::Unknown;std::string Name,Root,Evidence;bool Modified=false;std::string ModifiedBy;};
inline Info Describe(const std::string& path,const HelpyCacheSources::Plan& plan={},const std::set<std::string>& nativePlugins={},const std::string& exactOwner={},bool declaredModded=false,bool patched=false,const std::string& patchOwner={}){
    const auto root=ContentSource::Root(path),lower=HelpyCacheSources::Lower(root);
    if(root.empty())return {};
    const auto leaf=root.substr(root.find_last_of('/')+1),mount=HelpyCacheSources::Source(path);
    if(!exactOwner.empty()&&!ContentSource::Mod(exactOwner).empty())return {Kind::Modded,exactOwner,root,"Native clone/definition provenance"};
    const auto full=HelpyCacheSources::Lower(path);
    if(full.starts_with("/engine/transient")||full.starts_with("/temp/"))return {Kind::Unknown,leaf,root};
    // Creation provenance can introduce a clone anywhere. Patching an existing
    // core/official-plugin asset cannot change its original content provider.
    if(lower=="/game"||lower=="/engine")return {Kind::Vanilla,"Dragonwilds",root,"Base-game package namespace",patched,patchOwner};
    if(nativePlugins.contains(HelpyCacheSources::Lower(leaf)))return {Kind::Expansion,leaf,root,"Official game content plugin",patched,patchOwner};
    if(plan.AssetOwners){const auto found=plan.AssetOwners->find(HelpyCacheSources::AssetKey(path));
        if(found!=plan.AssetOwners->end()&&!found->second.empty())return {Kind::Modded,found->second.size()==1?*found->second.begin():std::string{},root,found->second.size()==1?"Exact registry declaration":"Conflicting registry declarations"};
    }
    if(plan.AuthoredAssets&&lower!="/game"&&lower!="/engine"){const auto found=plan.AuthoredAssets->find(HelpyCacheSources::AssetKey(path));
        if(found!=plan.AuthoredAssets->end()&&!found->second.empty())return {Kind::Modded,found->second.size()==1?*found->second.begin():std::string{},root,found->second.size()==1?"Exact RuneSchema asset definition":"Conflicting asset definitions"};
    }
    if(lower.starts_with("/game/mods/")||lower.starts_with("/game/runeschema/"))return {Kind::Modded,leaf,root};
    if(declaredModded)return {Kind::Modded,leaf,root};
    // Installed native plugin descriptors are collected by the existing source
    // worker. Never inspect the filesystem or guess from item/icon names here.
    if(const auto found=plan.DeclaredOwners.find(mount);found!=plan.DeclaredOwners.end()&&!found->second.empty()){
        // A mount may be borrowed official content. Dependency membership
        // alone proves neither creation nor ownership of this particular item.
        return {Kind::Unknown,{},root,"Mount dependency only; item provider unverified"};
    }
    if(lower.starts_with("/gamefeatures/plugins/")||lower.starts_with("/plugins/gamefeatures/"))return {Kind::Expansion,leaf,root};
    return {Kind::Unknown,leaf,root};
}
inline nlohmann::json Fields(const Info& info){return {{"SourceKind",Token(info.Group)},{"SourceName",info.Name},{"SourceRoot",info.Root},{"SourceEvidence",info.Evidence},{"ModifiedByRuneSchema",info.Modified},{"ModifiedBy",info.ModifiedBy}};}
inline Info Row(const nlohmann::json& row){
    if(!row.is_object())return {};
    const auto text=[&](const char* key){const auto found=row.find(key);return found!=row.end()&&found->is_string()?found->get<std::string>():std::string{};};
    const auto flag=[&](const char* key){const auto found=row.find(key);return found!=row.end()&&found->is_boolean()&&found->get<bool>();};
    auto info=Describe(text("Path"),{},{},{},flag("RuntimeClone")||flag("DeclaredModded"));
    if(flag("RuntimeClone")){info.Group=Kind::Modded;info.Evidence="Native clone/definition provenance";}
    const bool nativeCore=info.Group==Kind::Vanilla&&!flag("RuntimeClone");
    const auto kind=text("SourceKind");if(!kind.empty()&&!nativeCore)info.Group=Decode(kind);
    if(row.contains("SourceName")&&row["SourceName"].is_string())info.Name=text("SourceName"); // an explicit unknown owner must stay unknown
    const auto root=text("SourceRoot");if(!root.empty())info.Root=root;
    info.Evidence=text("SourceEvidence");
    if(nativeCore){info.Name="Dragonwilds";info.Evidence="Base-game package namespace";}
    info.Modified=(info.Group==Kind::Vanilla||info.Group==Kind::Expansion)&&(flag("ModifiedByRuneSchema")||(flag("RuneSchemaManaged")&&!flag("RuntimeClone")));
    info.ModifiedBy=text("ModifiedBy");
    return info;
}
inline Info Reattribute(const nlohmann::json& row,const HelpyCacheSources::Plan& plan,const std::set<std::string>& nativePlugins){
    if(row.value("RuntimeClone",false))return Row(row);
    auto info=Describe(row.value("Path",std::string{}),plan,nativePlugins,{},false,
        row.value("ModifiedByRuneSchema",false)||row.value("RuneSchemaManaged",false),row.value("ModifiedBy",std::string{}));
    if(info.Group==Kind::Unknown){
        const auto previous=Row(row);
        if(previous.Group==Kind::Modded&&(previous.Evidence=="Exact registry declaration"||previous.Evidence=="Exact RuneSchema asset definition"))return previous;
    }
    return info;
}
inline bool Matches(Kind filter,const Info& info){return filter==Kind::All||filter==info.Group;}
inline std::string Query(Kind source,const std::string& category,const std::string& text){
    std::string query;if(source!=Kind::All)query="__helpysource:"+std::string(Token(source))+" ";
    if(category!="All")query+="__helpycat:"+HelpyCacheSources::Lower(category)+" ";
    query+=text;while(!query.empty()&&query.back()==' ')query.pop_back();return query;
}
inline Kind TakeFilter(std::string& query){
    constexpr std::string_view prefix="__helpysource:";if(!query.starts_with(prefix))return Kind::All;
    const auto end=query.find(' ');const auto token=query.substr(prefix.size(),end==query.npos?std::string::npos:end-prefix.size());
    const auto kind=Decode(token);if(token!=Token(kind))throw std::runtime_error("Invalid item source filter");
    query=end==query.npos?std::string{}:query.substr(end+1);return kind;
}
}
