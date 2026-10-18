#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <stdexcept>
namespace DragonWilds::LoreBookPresentation {
using Json=nlohmann::json;
inline constexpr const char* Closed="/Game/Art/Env/Props/Gameplay_Props/Lore_Book/SM_Lore_Book_01.SM_Lore_Book_01";
inline constexpr const char* Open="/Game/Art/Env/Props/Gameplay_Props/Lore_Book/SM_Lore_Book_02.SM_Lore_Book_02";
inline constexpr const char* Glow="/Game/Art/VFX/Library/Lore/NS_LoreItem_Book.NS_LoreItem_Book";
struct Config {bool Enabled=false;std::string UnreadMesh,ReadMesh,GlowSystem;};
inline std::string Path(const Json& value){
    if(!value.is_string())throw std::runtime_error("LorePresentation assets require texture/mesh/system object paths");
    auto path=value.get<std::string>();
    if(path.empty()||path.size()>1024||path.front()!='/'||path.find('.')==path.npos||path.find("..")!=path.npos||path.find_first_of("\r\n\t\\:\0",0,6)!=path.npos)
        throw std::runtime_error("LorePresentation requires a mounted Unreal object path");
    return path;
}
inline Config Parse(const Json& data){
    const auto mesh=data.value("Mesh",std::string{});
    const bool hasLore=data.contains("LoreID")||data.contains("LoreEntry");
    Config result{hasLore&&data.value("Type",std::string{})=="Prop"&&(mesh==Closed||mesh==Open),mesh,Open,Glow};
    if(data.contains("LorePresentation")){
        const auto& value=data.at("LorePresentation");if(!value.is_object())throw std::runtime_error("LorePresentation must be an object");
        for(const auto& [key,entry]:value.items()){
            if(key=="Enabled"){if(!entry.is_boolean())throw std::runtime_error("LorePresentation.Enabled must be boolean");result.Enabled=entry.get<bool>();}
            else if(key=="UnreadMesh")result.UnreadMesh=Path(entry);
            else if(key=="ReadMesh")result.ReadMesh=Path(entry);
            else if(key=="GlowSystem")result.GlowSystem=entry==""?std::string{}:Path(entry);
            else throw std::runtime_error("Unsupported LorePresentation field: "+key);
        }
    }
    if(result.Enabled&&(!hasLore||data.value("Type",std::string{})!="Prop"||result.UnreadMesh.empty()))
        throw std::runtime_error("LorePresentation requires a Type Prop Mesh with LoreID/LoreEntry");
    return result;
}
inline std::optional<bool> ReadState(bool ready,bool unlocked,bool unread){if(!ready)return {};return unlocked&&!unread;}
}
