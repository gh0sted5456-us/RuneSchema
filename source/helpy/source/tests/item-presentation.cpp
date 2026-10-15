#include "Core/ItemPresentation.h"
#include "Core/ItemBadges.h"
#include <cassert>
int main(){
    using namespace PS::ItemPresentation;
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
