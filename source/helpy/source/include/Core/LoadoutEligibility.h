#pragma once
#include <array>
#include <string>
#include <string_view>
#include <stdexcept>
#include <nlohmann/json.hpp>
namespace PS::LoadoutEligibility {
inline constexpr std::array<std::string_view,12> Slots={"Head","Body","Legs","Cape","Trinket","Trinket2","Ammo","MagicAmmo1","HeldRight","HeldLeft","CrossbowBolts","FishingBait"};
inline bool Valid(std::string_view slot){for(const auto value:Slots)if(value==slot)return true;return false;}
// Read-only fitting compatibility, not permission to equip/use on a player.
// Strategy is copied from the reflected native enum, never from an item name.
inline bool Matches(const nlohmann::json& row,std::string_view slot){
    if(!Valid(slot)||!row.value("NativeSlotVerified",false))return false;
    if(row.value("NativeMagicAmmo",false))return slot=="MagicAmmo1";
    const auto strategy=row.value("SlotStrategy",std::string{});
    if(strategy==slot)return true;
    if(slot=="Trinket"||slot=="Trinket2")return strategy=="Trinket"||strategy=="Necklace";
    if(slot=="HeldRight")return strategy=="HeldOnlyRight"||strategy=="HeldTwoHanded"||strategy=="HeldAny";
    if(slot=="HeldLeft")return strategy=="HeldOnlyLeft"||strategy=="HeldAny";
    return false;
}
}
