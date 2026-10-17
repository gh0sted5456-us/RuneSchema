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
struct Info {Kind Group=Kind::Unknown;std::string Name,Root;};
inline Info Describe(const std::string& path,const HelpyCacheSources::Plan& plan={},const std::set<std::string>& nativePlugins={},const std::string& exactOwner={},bool declaredModded=false){
    const auto root=ContentSource::Root(path),lower=HelpyCacheSources::Lower(root);
    if(root.empty())return {};
    const auto leaf=root.substr(root.find_last_of('/')+1),mount=HelpyCacheSources::Source(path);
    if(!exactOwner.empty()&&!ContentSource::Mod(exactOwner).empty())return {Kind::Modded,exactOwner,root};
    if(lower.starts_with("/game/mods/")||lower.starts_with("/game/runeschema/"))return {Kind::Modded,leaf,root};
    if(declaredModded)return {Kind::Modded,leaf,root};
    const auto full=HelpyCacheSources::Lower(path);
    if(full.starts_with("/engine/transient")||full.starts_with("/temp/"))return {Kind::Unknown,leaf,root};
    if(lower=="/game"||lower=="/engine")return {Kind::Vanilla,"Dragonwilds",root};
    // Installed native plugin descriptors are collected by the existing source
    // worker. Never inspect the filesystem or guess from item/icon names here.
    if(nativePlugins.contains(HelpyCacheSources::Lower(leaf)))return {Kind::Expansion,leaf,root};
    if(const auto found=plan.DeclaredOwners.find(mount);found!=plan.DeclaredOwners.end()&&!found->second.empty()){
        std::string names;for(const auto& owner:found->second){if(!names.empty())names+=", ";names+=owner;}
        return {Kind::Modded,std::move(names),root};
    }
    if(lower.starts_with("/gamefeatures/plugins/")||lower.starts_with("/plugins/gamefeatures/"))return {Kind::Expansion,leaf,root};
    return {Kind::Unknown,leaf,root};
}
inline nlohmann::json Fields(const Info& info){return {{"SourceKind",Token(info.Group)},{"SourceName",info.Name},{"SourceRoot",info.Root}};}
inline Info Row(const nlohmann::json& row){
    if(!row.is_object())return {};
    const auto text=[&](const char* key){const auto found=row.find(key);return found!=row.end()&&found->is_string()?found->get<std::string>():std::string{};};
    const auto flag=[&](const char* key){const auto found=row.find(key);return found!=row.end()&&found->is_boolean()&&found->get<bool>();};
    auto info=Describe(text("Path"),{},{},{},flag("RuntimeClone")||flag("DeclaredModded"));
    const auto kind=text("SourceKind");if(!kind.empty())info.Group=Decode(kind);
    const auto name=text("SourceName");if(!name.empty())info.Name=name;
    const auto root=text("SourceRoot");if(!root.empty())info.Root=root;
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
