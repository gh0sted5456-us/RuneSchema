#include "Core/ItemPresentation.h"
#include "Core/ItemBadges.h"
#include <cassert>
int main(){
    using namespace PS::ItemPresentation;
    assert(Name({{"Name","Iron Bar"},{"Path","/Game/Items/ITEM_Iron.ITEM_Iron"}})=="Iron Bar");
    assert(Name({{"Name","  "},{"DisplayName","Iron Bar"}})=="Iron Bar");
    assert(Name({{"Name",nullptr},{"Path","/Game/Items/ITEM_Iron_Bar.ITEM_Iron_Bar"}})=="Iron Bar");
    assert(Name({{"Name","[Missing String Table Entry]"},{"Path","/Game/Items/ITEM_HeavySteelHelm.ITEM_HeavySteelHelm"}})=="Heavy Steel Helm");
    assert(Name({{"Name",Json::object()},{"InternalName","IronBar"}})=="Iron Bar");
    assert(Name({{"Name","Épée"}})=="Épée");
    assert(Name(nullptr)=="Unnamed item");
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
