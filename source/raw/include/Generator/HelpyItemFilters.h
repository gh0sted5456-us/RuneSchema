#pragma once
#include "Generator/QuickMenuDecorations.h"
#include <string>
#include <string_view>
#include <vector>
namespace PS::HelpyItemFilters {
inline const std::vector<std::string>& Types() {
    static const std::vector<std::string> values{"All types","Armour","Weapons","Food","Potions","Consumables","Quest items","Farming","Capes","Jewellery","Tools","Resources","Runes & ammo","Other"};return values;
}
inline const std::vector<std::string>& Sources() {
    static const std::vector<std::string> values{"All sources","Dragonwilds","RuneSchema","Cooked mods","Combined mods","Unknown"};return values;
}
// Tags and reflected class/slot information only. Display names and power levels
// are deliberately excluded: neither establishes type, quality or provenance.
inline bool Type(std::string_view filter,std::string_view tags,std::string_view classes,
                 std::string_view appearance,bool consumable,bool quest) {
    if(filter=="All types")return true;
    const auto t=QuickDecorations::Fold(std::string(tags)),c=QuickDecorations::Fold(std::string(classes));
    const auto has=[&](std::string_view v){return QuickDecorations::Token(t,v);};
    const bool cape=has("cape")||appearance=="wearable:Cape";
    const bool armour=cape||has("armour")||has("armor")||c.find("wearableequipment")!=c.npos;
    const bool jewellery=has("jewellery")||has("jewelry")||has("trinket")||has("amulet")||has("ring");
    const bool weapon=has("weapon");
    const bool tool=has("tool")||has("tools");
    const bool resource=has("resource")||has("resources")||has("material")||has("materials");
    const bool farming=has("seed")||has("seeds")||has("farming")||has("crop");
    const bool ammo=has("ammo")||has("ammunition")||has("rune")||has("runes");
    const bool food=has("food")||has("meal")||has("cooking")||has("edible");
    const bool potion=has("potion")||has("potions")||has("elixir")||has("brew");
    const bool pack=has("pack")||has("packs")||has("bundle")||has("crate");
    if(filter=="Weapons")return weapon;
    if(filter=="Armour")return armour;
    if(filter=="Capes")return cape;
    if(filter=="Jewellery")return jewellery;
    if(filter=="Tools")return tool;
    if(filter=="Consumables")return consumable||pack;
    if(filter=="Resources")return resource;
    if(filter=="Farming")return farming;
    if(filter=="Runes & ammo")return ammo;
    if(filter=="Food")return food;
    if(filter=="Potions")return potion;
    if(filter=="Quest items")return quest;
    return filter=="Other"&&!weapon&&!armour&&!jewellery&&!tool&&!resource&&!farming&&!ammo&&!food&&!potion&&!pack&&!consumable&&!quest;
}
inline bool Source(std::string_view filter,std::string_view path,bool cooked,bool runtime,
                   bool managed,bool declaredModded,bool declaredCooked=false) {
    if(filter=="All sources")return true;
    const bool rs=runtime||managed;
    // Cooked declarations explain author intent, but do not prove installed content.
    const bool mod=QuickDecorations::ModPath(path)||declaredModded;
    const bool cookedMod=cooked&&(mod||declaredCooked);
    if(filter=="RuneSchema")return rs;
    if(filter=="Cooked mods")return cookedMod;
    if(filter=="Combined mods")return rs&&cookedMod;
    if(filter=="Dragonwilds")return cooked&&!runtime&&!mod&&!declaredCooked;
    return filter=="Unknown"&&!rs&&!cookedMod&&(!cooked||mod);
}
}
