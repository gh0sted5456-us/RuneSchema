#include "Core/SaveCleanup.h"
#include <stdexcept>
using namespace PS::SaveCleanup;
void Require(bool b){if(!b)throw std::runtime_error("Save cleanup regression");}
template<class F>bool Rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
int main() {
    const std::string id="xBicTfzWYkeOCtOLKtSRFQ";
    auto ints=DragonWilds::Quests::OwnershipVariables("Example",id);
    ints.push_back({{"QuestVariableName","RuneSchema.Location:owned-location"},{"QuestVariableValue",7001}});
    Json source={{"GameProgress",{{"Character",{{"Name","Preserve me"}}},{"Inventory",{{"0",{{"ItemData","unrelated"}}}}},
        {"QuestProgress",{{"QuestTracked",id},{"Quests",Json::array({{{"QuestId",id},{"QuestInts",ints}},{{"QuestId","vanilla"},{"QuestInts",Json::array()}}})},
            {"QuestLocations",Json::array({{{"QuestLocationId","owned-location"}},{{"QuestLocationId","vanilla-location"}}})}}},
        {"Journal",{{"UnlockedEntries",Json::array({"book","vanilla"})},{"UnreadEntries",Json::array({"book"})},
            {"RuneSchemaOwnership",DragonWilds::JournalSave::EncodeNative({{"book","Example"}})},{"FutureNativeField",42}}}}}};
    Require(Plan(source,{}).Save==source);
    Require(Plan(source,{"Example"}).Save==source);
    auto cleaned=Plan(source,{"Example"},true);
    Require(cleaned.Owners.at("Example")==2 && cleaned.Removed.size()==3);
    const auto& game=cleaned.Save.at("GameProgress");
    Require(game.at("QuestProgress").at("Quests").size()==1 && game.at("QuestProgress").at("QuestTracked")=="");
    Require(game.at("QuestProgress").at("QuestLocations").size()==1);
    Require(game.at("Journal").at("UnlockedEntries")==Json::array({"vanilla"}));
    Require(game.at("Journal").at("RuneSchemaOwnership")==Json::array());
    Require(game.at("Journal").at("FutureNativeField")==42);
    Require(game.at("Character")==source.at("GameProgress").at("Character"));
    Require(game.at("Inventory")==source.at("GameProgress").at("Inventory"));
    Require(Plan(cleaned.Save,{"Example"}).Save==cleaned.Save);
    auto pending=source;pending["GameProgress"]["QuestProgress"]["Quests"][0]["QuestInts"].push_back({{"QuestVariableName","RuneSchema.Phase"},{"QuestVariableValue",3}});
    Require(Rejects([&]{Plan(pending,{"Example"},true);}));
    Require(Plan(pending,{}).Save==pending);
    auto malformed=source;malformed["GameProgress"]["Journal"]["RuneSchemaOwnership"]={"garbage"};
    Require(Rejects([&]{Plan(malformed,{"Example"});}));
    Require(Rejects([&]{Plan(Json::array(),{});}));
    RegistrySnapshot registry;
    for(int i=0;i<500;++i)registry.Items.insert("item"+std::to_string(i));
    for(int i=0;i<300;++i)registry.Recipes.insert("recipe"+std::to_string(i));
    auto assetSave=source;
    assetSave["GameProgress"]["Inventory"]["1"]={{"ItemData","item1"}};
    assetSave["GameProgress"]["Loadout"]={{"Head",{{"PlayerInventoryItemIndex",0}}},{"Body",{{"PlayerInventoryItemIndex",1}}}};
    assetSave["GameProgress"]["Progress"]={{"ItemsPickedUp",{"unrelated","item1"}},{"RecipesUnlocked",{"missing","recipe1"}}};
    const auto preserved=Plan(assetSave,{},false,&registry);
    Require(preserved.Save["GameProgress"]["Inventory"].size()==1);
    Require(preserved.Save["GameProgress"]["Loadout"].contains("Body") && !preserved.Save["GameProgress"]["Loadout"].contains("Head"));
    Require(preserved.Save["GameProgress"]["Progress"]==assetSave["GameProgress"]["Progress"]);
    Require(preserved.Save["GameProgress"]["QuestProgress"]==assetSave["GameProgress"]["QuestProgress"]);
    const auto erased=Plan(assetSave,{},true,&registry);
    Require(erased.Save["GameProgress"]["Progress"]["RecipesUnlocked"]==Json::array({"recipe1"}));
    Require(erased.Save["GameProgress"]["Progress"]["ItemsPickedUp"]==Json::array({"item1"}));
    Require(Plan(preserved.Save,{},false,&registry).Save==preserved.Save);
    RegistrySnapshot incomplete;
    Require(Rejects([&]{Plan(assetSave,{},false,&incomplete);}));
    PublishRegistry(registry);auto snapshot=ReadRegistry();Require(snapshot && snapshot->Ready());
    PublishRegistry({});Require(!ReadRegistry() && snapshot->Ready());
}
