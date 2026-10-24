#pragma once
#include "Core/HelpyCacheSources.h"
#include <set>
#include <string>
#include <vector>
namespace PS::ContentSource {
inline std::string Root(const std::string& path) {
    if(path.empty()||path.front()!='/'||path.size()>4096||path.find("..")!=path.npos||path.find_first_of("\\\r\n\t")!=path.npos||std::any_of(path.begin(),path.end(),[](unsigned char c){return c<32||c==127;}))return {};
    const auto lower=HelpyCacheSources::Lower(path);
    size_t start=1;
    for(const std::string prefix:{"/game/mods/","/game/runeschema/","/game/gameplay/mods/","/gamefeatures/plugins/","/plugins/gamefeatures/","/plugins/"})
        if(lower.starts_with(prefix)){start=prefix.size();break;}
    const auto end=path.find('/',start);return path.substr(0,end);
}
inline std::string Mod(const std::string& name) {
    if(name.empty()||name.size()>128||name=="."||name==".."||name.find_first_of("/\\\r\n\t:")!=name.npos)return {};
    return "mods/"+HelpyCacheSources::Lower(name);
}
inline bool Valid(const std::string& value) {
    if(value=="core")return true;
    if(!value.starts_with("mods/")&&!value.starts_with("plugins/"))return false;
    const auto name=value.substr(value.find('/')+1);
    return !name.empty()&&name.size()<=192&&name.find("..")==name.npos&&name.find_first_of("/\\\r\n\t:")==name.npos&&std::none_of(name.begin(),name.end(),[](unsigned char c){return c<32||c==127;});
}
inline std::set<std::string> Origins(const std::string& asset,const HelpyCacheSources::Plan& plan,const std::string& exactOwner={}) {
    if(!exactOwner.empty()){const auto mod=Mod(exactOwner);if(!mod.empty())return {mod};}
    if(plan.ContainerAssets){const auto found=plan.ContainerAssets->find(HelpyCacheSources::AssetKey(asset));
        if(found!=plan.ContainerAssets->end())return found->second;}
    const auto group=HelpyCacheSources::Source(asset);if(group.empty())return {};
    std::set<std::string> values;
    if(const auto found=plan.Owners.find(group);found!=plan.Owners.end())for(const auto& owner:found->second){const auto mod=Mod(owner);if(!mod.empty())values.insert(mod);}
    const auto root=Root(asset),lower=HelpyCacheSources::Lower(root);
    if(lower.starts_with("/game/mods/")||lower.starts_with("/game/runeschema/")||lower.starts_with("/game/gameplay/mods/")){
        if(values.empty())values.insert("mods/"+lower.substr(lower.find_last_of('/')+1));return values;
    }
    if(lower=="/game"||lower=="/engine")return {"core"};
    else if(values.empty()&&!lower.empty())values.insert("plugins/"+lower.substr(lower.find_last_of('/')+1));
    return values;
}
inline std::set<std::string> Choices(const HelpyCacheSources::Plan& plan) {
    std::set<std::string> values;for(const auto& source:plan.Registered){const auto origins=Origins("/"+source+"/asset.asset",plan);values.insert(origins.begin(),origins.end());
        if(const auto found=plan.Owners.find(source);found!=plan.Owners.end())for(const auto& owner:found->second){const auto mod=Mod(owner);if(!mod.empty())values.insert(mod);}}
    return values;
}
inline bool UiTexture(const std::string& path,const std::string& type) {
    const auto p=HelpyCacheSources::Lower(path),t=HelpyCacheSources::Lower(type);
    return (t.find("texture")!=t.npos||t.find("sprite")!=t.npos)&&(p.find("/ui/")!=p.npos||p.find("/icons/")!=p.npos||p.find("/icon/")!=p.npos||p.find("/t_icon_")!=p.npos);
}
inline bool NativeContentFile(const std::string& relative) {
    const auto name=HelpyCacheSources::Lower(relative);
    const auto dot=name.find_last_of('.');if(dot==name.npos)return false;const auto ext=name.substr(dot);
    return (ext==".pak"||ext==".utoc"||ext==".ucas"||ext==".uplugin")
        &&(name.starts_with("content/paks/")||name.starts_with("plugins/")||name.starts_with("gamefeatures/")||name.starts_with("../engine/plugins/"));
}
}
