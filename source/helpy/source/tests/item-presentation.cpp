#include "Core/ItemPresentation.h"
#include "Core/ItemBadges.h"
#include <cassert>
int main(){
    assert(PS::ItemPresentation::OriginalName({{"Name","ITEM_colors_of_money_armour_body"},{"DisplayName","Ranger Body (Dyed Black)"}})=="Ranger Body (Dyed Black)");
    assert(PS::ItemPresentation::OriginalName({{"Name","ITEM_unresolved"},{"Path","/Game/Items/ITEM_RangerBody.ITEM_RangerBody"}})=="Ranger Body");
    assert(PS::ItemPresentation::OriginalName({{"Name","DA_RangerBody"},{"DisplayName","Ranger Body (Dyed Black)"},{"Path","/Game/Items/DA_RangerBody.DA_RangerBody"}})=="Ranger Body (Dyed Black)");
    using namespace PS::ItemPresentation;
    assert(Name({{"Name","Iron Bar"},{"Path","/Game/Items/ITEM_Iron.ITEM_Iron"}})=="Iron Bar");
    assert(Name({{"Name","  "},{"DisplayName","Iron Bar"}})=="Iron Bar");
    assert(Name({{"Name",nullptr},{"Path","/Game/Items/ITEM_Iron_Bar.ITEM_Iron_Bar"}})=="Iron Bar");
    assert(Name({{"Name","[Missing String Table Entry]"},{"Path","/Game/Items/ITEM_HeavySteelHelm.ITEM_HeavySteelHelm"}})=="Heavy Steel Helm");
    assert(Name({{"Name",Json::object()},{"InternalName","IronBar"}})=="Iron Bar");
    assert(Name({{"Name","Épée"}})=="Épée");
    assert(Name(nullptr)=="Unnamed item");
    const Json cached={{"Name","RangerBodyBlack"},{"DisplayName","Ranger Body (Dyed Black)"},{"InternalName","armour_ranger_body_black"},{"Path","/Example/ITEM_RangerBody.ITEM_RangerBody"}};
    assert(Name(cached)=="Ranger Body (Dyed Black)"&&Name(cached,true)=="armour_ranger_body_black");
    assert(Name({{"Name","Ranger Body"},{"Path","/Example/ITEM_RangerBody.ITEM_RangerBody"}},true)=="ITEM_RangerBody");
    auto repaired=cached;PS::ItemName::Repair(repaired);assert(repaired["Name"]==cached["DisplayName"]&&repaired["InternalName"]==cached["InternalName"]);
    const auto renamed=PS::ItemName::Merge(repaired,{{"Name","Ranger Body (Updated)"}});assert(renamed["Name"]=="Ranger Body (Updated)"&&renamed["DisplayName"]==renamed["Name"]);
    const auto unresolved=PS::ItemName::Merge(renamed,{{"Name","ITEM_unresolved"},{"DisplayName","[Missing String Table Entry]"}});assert(Name(unresolved)=="Ranger Body (Updated)");
    assert(Placeholder({{"Name"," [PH] Unfinished helm"}}));
    assert(Placeholder({{"DisplayName","[ph] Test"}}));
    assert(!Placeholder({{"Name","A [PH] souvenir"}})&&!Placeholder({{"Name","Phantom helm"}}));
    Json cloned={{"Path","/Game/RuneSchema/MyClones/HeavySteelHelm.HeavySteelHelm"},{"Icon","/Game/UI/Source.Source"}};
    Json nativeIcon={{"Source",cloned["Path"]},{"Icon","/CustomMod/UI/ClonedHelm.ClonedHelm"}};
    assert(Icon(cloned,&nativeIcon)=="/CustomMod/UI/ClonedHelm.ClonedHelm");
    nativeIcon["Source"]="/Game/Other.Other";assert(Icon(cloned,&nativeIcon)=="/Game/UI/Source.Source");
    nativeIcon["Source"]=cloned["Path"];nativeIcon["Icon"]="";assert(Icon(cloned,&nativeIcon).empty());
    cloned["_HelpyIconPreview"]=true;cloned["Icon"]="/Game/UI/Donor.Donor";assert(Icon(cloned,&nativeIcon)=="/Game/UI/Donor.Donor");
    assert(Icon(nullptr).empty());
    Json details={{"Fields",Json::array({
        {{"NativeName","Description"},{"Value","A native item description."},{"HasValue",true}},
        {{"NativeName","BaseDamage"},{"Value",42.5},{"HasValue",true}},
        {{"NativeName","MaxDurability"},{"Value",120},{"HasValue",true}},
        {{"NativeName","PersistenceID"},{"Value","technical-id"},{"HasValue",true}},
        {{"NativeName","DamageTable"},{"Value","/Game/Stats.Stats"},{"HasValue",true}},
        {{"NativeName","UnsupportedField"},{"Value",13},{"HasValue",false}},
        nullptr
    })}};
    assert(Description(details)=="A native item description.");
    const auto facts=Facts(details);assert(facts.size()==2&&facts[0].Label=="Base Damage"&&facts[0].Value=="42.5"&&facts[1].Label=="Max Durability");
    assert(Description(nullptr).empty()&&Facts(Json::array()).empty());
    assert(StationIcon({{"Icon","/Game/UI/Anvil.Anvil"}})=="/Game/UI/Anvil.Anvil");
    assert(StationIcon({{"Icon",nullptr},{"StationIcon","/Game/UI/Forge.Forge"}})=="/Game/UI/Forge.Forge");
    assert(StationIcon({{"Icon","https://unverified.example/icon.png"}}).empty());
    assert(!PS::ItemBadges::Power({{"PowerLevel",nullptr}}));
    assert(PS::ItemBadges::Power({{"PowerLevel",8}})==8);
}
