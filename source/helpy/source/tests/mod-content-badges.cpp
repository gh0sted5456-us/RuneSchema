#include "Core/ModContentBadges.h"
#include <cassert>
#include <iostream>
// RuneSchema mod rows become short, ordered, de-duplicated content badges.
using namespace PS;using Json=nlohmann::json;
int main(){
    const Json row={{"Name","Dye_Quests"},{"HasCache",true},{"Loaders",Json::array({"RuneSchema: quests","Unreal PAK","RuneSchema: raw","RuneSchema: RAWS","RuneSchema: assets","LogicMods","RuneSchema: unknown-folder",7})}};
    const auto badges=ModContentBadges::ForRow(row);
    std::vector<std::string> labels;for(const auto& badge:badges)labels.emplace_back(badge.Label);
    // Table order, not folder order; raw/raws collapse; unknown folders and non-strings are ignored; cache last.
    assert((labels==std::vector<std::string>{"Items","Quests","Data tables","Cooked PAK","LogicMods","Helpy cache"}));
    assert(badges[0].Kind==ModContentBadges::Category::Content&&badges[1].Kind==ModContentBadges::Category::Story);
    assert(ModContentBadges::Summary(badges)=="Items, Quests, Data tables, Cooked PAK, LogicMods, Helpy cache");
    for(const auto& badge:badges)assert(!badge.Description.empty());
    assert(ModContentBadges::ForRow(Json{{"Name","Empty"}}).empty());
    {const auto withAchievements=ModContentBadges::ForRow(Json{{"HasAchievements",true},{"HasCache",true}});
     assert(withAchievements.size()==2&&withAchievements[0].Label=="Achievements"&&withAchievements[1].Label=="Helpy cache");}
    assert(ModContentBadges::Summary({})=="No RuneSchema content folders");
    assert(ModContentBadges::ForRow(Json{{"Loaders","not an array"}}).empty());
    std::cout<<"Mod content badges are readable, ordered and de-duplicated\n";
}
