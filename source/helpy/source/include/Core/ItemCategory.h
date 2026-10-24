#pragma once
#include <array>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace PS::ItemCategory {
inline constexpr std::array<const char*,11> Categories={"All","Weapons","Armor","Consumables","Ammo","Runes","Resources","Tools","Building","Quest","Misc"};
inline std::string Lower(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return value;}
inline bool Tag(const std::string& tags,std::string_view parent){
    for(size_t at=tags.find(parent);at!=tags.npos;at=tags.find(parent,at+1)){
        const auto end=at+parent.size();
        if((at==0||tags[at-1]==' ')&&(end==tags.size()||tags[end]==' '||tags[end]=='.'))return true;
    }return false;
}
inline std::string Classify(const std::string& path,const std::string& name,const std::string& internal,const std::string& tags,const std::string& classes){
    const auto t=Lower(tags),c=Lower(classes);
    const auto tagged=[&](std::initializer_list<std::string_view> parents){for(const auto parent:parents)if(Tag(t,parent))return true;return false;};
    // Native gameplay tags outrank names and mod directories. BuildingResource is a material, not a placeable.
    if(tagged({"item.questitem","itemfilter.type.questitem"}))return "Quest";
    if(tagged({"itemfilter.type.equipment.ammo.rune","item.equipment.ammo.rune"}))return "Runes";
    if(tagged({"item.equipment.ammo","itemfilter.type.equipment.ammo"}))return "Ammo";
    if(tagged({"item.equipment.tool","itemfilter.type.equipment.tool"}))return "Tools";
    if(tagged({"item.equipment.weapon","itemfilter.type.equipment.weapon"}))return "Weapons";
    if(tagged({"item.equipment.armour","item.equipment.armor","item.equipment.shield","item.equipment.cape",
        "itemfilter.type.equipment.armour","itemfilter.type.equipment.armor","itemfilter.type.equipment.shield","itemfilter.type.equipment.cape"}))return "Armor";
    if(tagged({"item.food","item.consumable","itemfilter.type.consumable"}))return "Consumables";
    if(tagged({"item.buildingresource","item.resource","itemfilter.material"}))return "Resources";
    if(tagged({"item.building","item.buildable","item.placeable"}))return "Building";
    if(c.find("magicammodata")!=c.npos)return "Runes";
    if(c.find("wearableequipmentdata")!=c.npos)return "Armor";
    if(c.find("consumable")!=c.npos)return "Consumables";
    if(c.find("buildingpiecedata")!=c.npos)return "Building";
    // Unloaded references have no tags: use whole words from the asset leaf, never a mod's parent folder.
    const auto leaf=path.substr(path.find_last_of("/.")==path.npos?0:path.find_last_of("/.")+1);
    auto words=Lower(leaf+" "+name+" "+internal);for(auto& ch:words)if(!((ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')))ch=' ';
    words=" "+words+" ";
    const auto any=[&](std::initializer_list<std::string_view> tokens){for(const auto token:tokens)if(words.find(" "+std::string(token)+" ")!=words.npos)return true;return false;};
    if(any({"quest","questitem"}))return "Quest";
    if(any({"pickaxe","hatchet","fishingrod","hammer","shovel","sickle","tool","tools"}))return "Tools";
    if(any({"armor","armour","helmet","helm","chestplate","cuirass","gauntlet","glove","gloves","boots","greaves","shield","cape"}))return "Armor";
    if(any({"weapon","weapons","sword","dagger","mace","spear","halberd","bow","shortbow","longbow","crossbow","staff","wand","pistol","musket","battleaxe"}))return "Weapons";
    if(any({"rune","runes"}))return "Runes";
    if(any({"ammo","ammunition","arrow","arrows","bolt","bolts","bullet","bullets","shot"}))return "Ammo";
    if(any({"consumable","food","potion","drink","elixir","brew","ration","cooked","meal"}))return "Consumables";
    if(any({"resource","resources","material","materials","ore","ingot","bar","plank","log","logs","timber","hide","leather","fibre","fiber","cloth","stone","crystal","essence","shard","coal"}))return "Resources";
    if(any({"building","buildable","furniture","structure","construction","placeable"}))return "Building";
    return {};
}
inline std::string Text(const nlohmann::json& row,const char* key){const auto it=row.find(key);return it!=row.end()&&it->is_string()?it->get<std::string>():std::string{};}
inline std::string Resolve(const nlohmann::json& row){
    const auto category=Classify(Text(row,"Path"),Text(row,"Name"),Text(row,"InternalName"),Text(row,"Tags"),Text(row,"Class"));
    if(!category.empty())return category;
    const auto previous=Text(row,"Category");for(const auto* known:Categories)if(previous==known&&previous!="All")return previous;
    return "Misc";
}
inline bool Repair(nlohmann::json& row){
    if(!row.is_object())return false;const auto category=Resolve(row);if(Text(row,"Category")==category)return false;row["Category"]=category;return true;
}
}
