#include "Loader/JournalUnlockCondition.h"
#include <stdexcept>

using nlohmann::json;

static void Require(bool value) {
    if (!value) throw std::runtime_error("journal unlock condition contract failed");
}
static bool Accepts(const json& value) {
    try { DragonWilds::JournalUnlockCondition::Validate(value); return true; }
    catch (const std::exception&) { return false; }
}

int main() {
    using DragonWilds::JournalUnlockCondition::Automatic;
    Require(Automatic(json::object(), false));
    Require(!Automatic(json::object(), true));
    Require(!Automatic({{"UnlockCondition",{{"UnlockType","PlayerStart"}}}}, false));
    Require(!Automatic({{"UnlockOnAcquire",true}}, false));
    Require(Automatic({{"UnlockCondition",{{"UnlockType","PlayerStart"}}},
        {"Unlock",true}}, false));
    Require(!Automatic({{"Unlock",false}}, false));
    Require(Accepts(json::object()));
    Require(Accepts({{"UnlockCondition",{{"UnlockType","EJournalEntryUnlockType::ItemFirstPickup"},
        {"UnlockingItemData","/MyMod/Items/ITEM_Pigment.ITEM_Pigment"}}}}));
    Require(Accepts({{"UnlockCondition",{{"UnlockType","RecipeUnlocked"},
        {"UnlockingRecipe","/MyMod/Recipes/RECIPE_Dye.RECIPE_Dye"}}}}));
    Require(Accepts({{"UnlockCondition",{{"UnlockType","Interaction"},
        {"UnlockingInteractionActor","/MyMod/NPC/BP_Dyer.BP_Dyer_C"}}}}));
    Require(Accepts({{"UnlockCondition",{{"UnlockType","AIKill"},
        {"AIData","/MyMod/AI/BP_Data.BP_Data_C"}}}}));
    Require(Accepts({{"UnlockCondition",{{"UnlockType","PlayerStart"}}}}));
    Require(!Accepts({{"UnlockCondition",42}}));
    Require(!Accepts({{"UnlockCondition",{{"UnlockType","ItemFirstPickup"}}}}));
    Require(!Accepts({{"UnlockCondition",{{"UnlockType","RecipeUnlocked"},
        {"UnlockingRecipe","RECIPE_Dye"}}}}));
    Require(!Accepts({{"UnlockCondition",{{"UnlockType","AIKill"},
        {"AIData",false}}}}));
}
