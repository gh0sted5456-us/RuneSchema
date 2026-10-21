#include "Core/SaveCleanup.h"
#include "Generator/ClonePresentation.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace PS::SaveCleanup;
void RequireAt(bool b,int line){if(!b)throw std::runtime_error("Save cleanup regression at line "+std::to_string(line));}
#define Require(value) RequireAt((value),__LINE__)
template<class F>bool Rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
Json WithoutRetiredJournalMetadata(Json source){source.at("GameProgress").at("Journal").erase(DragonWilds::JournalSave::Manifest);return source;}
int Run() {
    const std::string id="xBicTfzWYkeOCtOLKtSRFQ";
    for(size_t at=0;at<id.size();++at)for(int character=0;character<128;++character){auto candidate=id;candidate[at]=static_cast<char>(character);
        Require(PS::ClonePresentation::ValidId(candidate,"AnyMod")==DragonWilds::IsCanonicalPersistenceId(candidate));}
    for(int i=0;i<2048;++i)Require(DragonWilds::IsCanonicalPersistenceId(PS::ClonePresentation::NewId("AnyMod")));
    auto ints=DragonWilds::Quests::OwnershipVariables("Example",id);
    ints.push_back({{"QuestVariableName","RuneSchema.Location:owned-location"},{"QuestVariableValue",7001}});
    Json source={{"GameProgress",{{"Character",{{"Name","Preserve me"}}},{"Inventory",{{"0",{{"ItemData","unrelated"}}}}},
        {"QuestProgress",{{"QuestTracked",id},{"Quests",Json::array({{{"QuestId",id},{"QuestInts",ints}},{{"QuestId","vanilla"},{"QuestInts",Json::array()}}})},
            {"QuestLocations",Json::array({{{"QuestLocationId","owned-location"}},{{"QuestLocationId","vanilla-location"}}})}}},
        {"Journal",{{"UnlockedEntries",Json::array({"book","vanilla"})},{"UnreadEntries",Json::array({"book"})},
            {"RuneSchemaOwnership",DragonWilds::JournalSave::EncodeNative({{"book","Example"}})},{"FutureNativeField",42}}}}}};
    const Json profileOnly={{"Version",83},{"meta_data",{{"char_guid","profile"}}},
        {"Customization",{{"CustomizationData",Json::object()}}},{"IsNewCharacter",true}};
    Require(ClassifyCharacterDocument(source)==CharacterDocumentKind::Gameplay);
    Require(ClassifyCharacterDocument(profileOnly)==CharacterDocumentKind::ProfileOnly);
    Require(ClassifyCharacterDocument(Json::array())==CharacterDocumentKind::Unsupported);
    const auto normalized=WithoutRetiredJournalMetadata(source);
    Require(Plan(source,{}).Save==normalized);
    Require(Plan(source,{"Example"}).Save==normalized);
    auto cleaned=Plan(source,{"Example"},true);
    Require(cleaned.Owners.at("Example")==1 && cleaned.Removed.size()==2);
    const auto& game=cleaned.Save.at("GameProgress");
    Require(game.at("QuestProgress").at("Quests").size()==1 && game.at("QuestProgress").at("QuestTracked")=="");
    Require(game.at("QuestProgress").at("QuestLocations").size()==1);
    // Owner selection is not evidence that a journal ID is unresolved. Only
    // the complete live journal registry can certify orphaned discovery IDs.
    Require(game.at("Journal").at("UnlockedEntries")==Json::array({"book","vanilla"}));
    Require(game.at("Journal").at("UnreadEntries")==Json::array({"book"}));
    Require(!game.at("Journal").contains("RuneSchemaOwnership"));
    Require(game.at("Journal").at("FutureNativeField")==42);
    Require(game.at("Character")==source.at("GameProgress").at("Character"));
    Require(game.at("Inventory")==source.at("GameProgress").at("Inventory"));
    Require(Plan(cleaned.Save,{"Example"}).Save==cleaned.Save);
    auto pending=source;pending["GameProgress"]["QuestProgress"]["Quests"][0]["QuestInts"].push_back({{"QuestVariableName","RuneSchema.Phase"},{"QuestVariableValue",3}});
    Require(Rejects([&]{Plan(pending,{"Example"},true);}));
    Require(Plan(pending,{}).Save==WithoutRetiredJournalMetadata(pending));
    auto malformed=source;malformed["GameProgress"]["Journal"]["RuneSchemaOwnership"]={"garbage"};
    Require(Plan(malformed,{"Example"}).Save==normalized); // Retired metadata is not a cleanup authority.
    Require(Rejects([&]{Plan(Json::array(),{});}));
    Require(AbsentOwners({{"Enabled",1},{"Missing",2}},{{"enabled"}})==std::set<std::string>{"Missing"});
    RegistrySnapshot registry;
    for(int i=0;i<500;++i)registry.Items.insert("item"+std::to_string(i));
    for(int i=0;i<300;++i)registry.Recipes.insert("recipe"+std::to_string(i));
    registry.Quests={id,"vanilla"};registry.QuestsComplete=true;
    auto movedOwners=source;
    movedOwners["GameProgress"]["QuestProgress"]["Quests"][0]["QuestInts"].push_back({{"QuestVariableName","RuneSchema.Owner:OldMod"},{"QuestVariableValue",DragonWilds::Quests::OwnershipVersion}});
    auto earlyRegistry=registry;earlyRegistry.QuestsComplete=false;
    const auto earlyPlan=Plan(movedOwners,{},false,&earlyRegistry);
    Require(earlyPlan.Save["GameProgress"]["QuestProgress"]==movedOwners["GameProgress"]["QuestProgress"]);
    auto assetSave=source;
    assetSave["GameProgress"]["Inventory"]["1"]={{"ItemData","item1"}};
    assetSave["GameProgress"]["Loadout"]={{"Head",{{"PlayerInventoryItemIndex",0}}},{"Body",{{"PlayerInventoryItemIndex",1}}}};
    assetSave["GameProgress"]["Progress"]={{"ItemsPickedUp",{"unrelated","item1"}},{"RecipesUnlocked",{"missing","recipe1"}}};
    assetSave["GameProgress"]["Progress"]["BuildingsUnlocked"]={"unresolved-but-no-complete-building-registry"};
    assetSave["GameProgress"]["Progress"]["SpellsUnlocked"]={"unresolved-but-no-complete-spell-registry"};
    assetSave["GameProgress"]["Progress"]["PlayerHooksTriggered"]={"unresolved-but-no-complete-hook-registry"};
    // The automatic ownership-only mode must preserve every unowned record,
    // including unresolved third-party assets and equipment.
    Require(Plan(assetSave,{}).Save==WithoutRetiredJournalMetadata(assetSave));
    const auto preserved=Plan(assetSave,{},false,&registry);
    Require(preserved.Save["GameProgress"]["Inventory"].size()==1);
    Require(preserved.Save["GameProgress"]["Loadout"].contains("Body") && !preserved.Save["GameProgress"]["Loadout"].contains("Head"));
    Require(preserved.Save["GameProgress"]["Progress"]==assetSave["GameProgress"]["Progress"]);
    Require(preserved.Save["GameProgress"]["QuestProgress"]==assetSave["GameProgress"]["QuestProgress"]);
    const auto erased=Plan(assetSave,{},true,&registry);
    Require(erased.Save["GameProgress"]["Progress"]["RecipesUnlocked"]==Json::array({"recipe1"}));
    Require(erased.Save["GameProgress"]["Progress"]["ItemsPickedUp"]==Json::array({"item1"}));
    const auto orphanPruned=Plan(assetSave,{},false,&registry,false,true);
    Require(orphanPruned.Save["GameProgress"]["Progress"]["RecipesUnlocked"]==Json::array({"recipe1"}));
    Require(orphanPruned.Save["GameProgress"]["Progress"]["ItemsPickedUp"]==Json::array({"item1"}));
    Require(orphanPruned.Save["GameProgress"]["Progress"]["BuildingsUnlocked"]==assetSave["GameProgress"]["Progress"]["BuildingsUnlocked"]);
    Require(orphanPruned.Save["GameProgress"]["Progress"]["SpellsUnlocked"]==assetSave["GameProgress"]["Progress"]["SpellsUnlocked"]);
    Require(orphanPruned.Save["GameProgress"]["Progress"]["PlayerHooksTriggered"]==assetSave["GameProgress"]["Progress"]["PlayerHooksTriggered"]);
    Require(orphanPruned.Save["GameProgress"]["QuestProgress"]==assetSave["GameProgress"]["QuestProgress"]);
    auto missingQuest=assetSave;
    missingQuest["GameProgress"]["QuestProgress"]["Quests"][0]["QuestId"]="missing-quest";
    missingQuest["GameProgress"]["QuestProgress"]["Quests"][0]["QuestInts"]=Json::array();
    const auto questErased=Plan(missingQuest,{},true,&registry);
    Require(questErased.Save["GameProgress"]["QuestProgress"]["Quests"].size()==1);
    Require(questErased.Removed.end()!=std::find_if(questErased.Removed.begin(),questErased.Removed.end(),
        [](const auto& row){return row.value("Kind","")=="Quest/dialogue" && row.value("Id","")=="missing-quest";}));
    auto trackedOnly=assetSave;
    trackedOnly["GameProgress"]["QuestProgress"]["QuestTracked"]="missing-tracked";
    const auto trackedCleaned=Plan(trackedOnly,{},false,&registry,false,true,true);
    Require(trackedCleaned.Save["GameProgress"]["QuestProgress"]["QuestTracked"]=="");
    Require(trackedCleaned.Removed.end()!=std::find_if(trackedCleaned.Removed.begin(),trackedCleaned.Removed.end(),
        [](const auto& row){return row.value("Kind","")=="Quest tracked" && row.value("Id","")=="missing-tracked";}));
    auto duplicateMissing=missingQuest;
    duplicateMissing["GameProgress"]["QuestProgress"]["Quests"].push_back(
        duplicateMissing["GameProgress"]["QuestProgress"]["Quests"][0]);
    Require(Plan(duplicateMissing,{},false,&registry,false,true,true).Save
        ["GameProgress"]["QuestProgress"]["Quests"].size()==1);
    auto unresolvedPending=missingQuest;
    unresolvedPending["GameProgress"]["QuestProgress"]["Quests"][0]["QuestInts"]={
        {{"QuestVariableName","RuneSchema.Phase"},{"QuestVariableValue",3}}};
    Require(Plan(unresolvedPending,{},false,&registry,false,true).Save
        ["GameProgress"]["QuestProgress"]["Quests"].size()==1);
    Require(Plan(preserved.Save,{},false,&registry).Save==preserved.Save);
    auto ownedSave=assetSave;
    ownedSave["GameProgress"]["Inventory"]["7"]={{"ItemData","missing-helmet"},{"Count",1}};
    ownedSave["GameProgress"]["PersonalInventory"]["9"]={{"ItemData","missing-shield"}};
    ownedSave["GameProgress"]["Loadout"]["Helmet"]={{"PlayerInventoryItemIndex",7}};
    ownedSave["GameProgress"]["Loadout"]["Chest"]={{"ItemData","missing-body"}};
    ownedSave["GameProgress"]["Loadout"]["Cape"]={{"ItemData","active-cape"}};
    ownedSave["GameProgress"]["Progress"]={{"ItemsPickedUp",{"unrelated","missing-helmet"}},
        {"MilestoneMaterialsPickedUp",{"missing-shield"}},
        {"RecipesUnlocked",{"missing-recipe","recipe1"}},
        {"RecipesNew",{"missing-recipe"}}};
    const std::unordered_map<std::string,std::string> retiredItems={
        {"missing-helmet","BlackG"},{"missing-shield","BlackG"},{"missing-body","BlackG"}};
    const std::unordered_map<std::string,std::string> retiredRecipes={{"missing-recipe","BlackG"}};
    const auto ownedCleaned=PlanOwned(ownedSave,retiredItems,retiredRecipes);
    Require(!ownedCleaned.Save["GameProgress"]["Inventory"].contains("7"));
    Require(!ownedCleaned.Save["GameProgress"]["PersonalInventory"].contains("9"));
    Require(!ownedCleaned.Save["GameProgress"]["Loadout"].contains("Helmet"));
    Require(!ownedCleaned.Save["GameProgress"]["Loadout"].contains("Chest"));
    Require(ownedCleaned.Save["GameProgress"]["Loadout"].contains("Cape"));
    Require(ownedCleaned.Save["GameProgress"]["Inventory"].contains("0"));
    Require(ownedCleaned.Save["GameProgress"]["Progress"]["ItemsPickedUp"]==Json::array({"unrelated"}));
    Require(ownedCleaned.Save["GameProgress"]["Progress"]["RecipesUnlocked"]==Json::array({"recipe1"}));
    Require(PlanOwned(ownedCleaned.Save,retiredItems,retiredRecipes).Save==ownedCleaned.Save);
    Require(PlanOwned(ownedSave,{}, {}).Save==ownedSave);
    auto pendingOwned=ownedSave;
    const std::string retiredQuest="BBBBBBBBBBBBBBBBBBBBBA";
    auto pendingInts=DragonWilds::Quests::OwnershipVariables("BlackG",retiredQuest);
    pendingInts.push_back({{"QuestVariableName","RuneSchema.Phase"},{"QuestVariableValue",3}});
    pendingOwned["GameProgress"]["QuestProgress"]["Quests"].push_back(
        {{"QuestId",retiredQuest},{"QuestInts",pendingInts}});
    const std::unordered_map<std::string,std::string> retiredQuests={{retiredQuest,"BlackG"}};
    const auto pendingRemoved=PlanOwned(pendingOwned,retiredItems,retiredRecipes,retiredQuests);
    Require(pendingRemoved.Save["GameProgress"]["QuestProgress"]["Quests"].end()==
        std::find_if(pendingRemoved.Save["GameProgress"]["QuestProgress"]["Quests"].begin(),
            pendingRemoved.Save["GameProgress"]["QuestProgress"]["Quests"].end(),
            [&](const auto& row){return row.value("QuestId","")==retiredQuest;}));
    auto damaged=assetSave;
    damaged["GameProgress"]["Inventory"]["broken"]="not-an-item";
    damaged["GameProgress"]["Loadout"]["broken"]={{"PlayerInventoryItemIndex","not-an-index"}};
    damaged["GameProgress"]["QuestProgress"]["Quests"].push_back(
        damaged["GameProgress"]["QuestProgress"]["Quests"][0]);
    const auto repaired=Plan(damaged,{},true,&registry);
    Require(!repaired.Save["GameProgress"]["Inventory"].contains("broken"));
    Require(!repaired.Save["GameProgress"]["Loadout"].contains("broken"));
    Require(repaired.Save["GameProgress"]["QuestProgress"]["Quests"].size()==3);
    Require(repaired.Removed.end()==std::find_if(repaired.Removed.begin(),repaired.Removed.end(),
        [](const auto& row){return row.value("Mod","")=="Duplicate native record";}));
    auto strictIdentity=damaged;
    strictIdentity["GameProgress"]["Progress"]={{"RecipesUnlocked",{"missing","recipe1"}}};
    const auto identityOnly=Plan(strictIdentity,{},false,&registry,false,true,true);
    Require(identityOnly.Save["GameProgress"]["Inventory"].contains("broken"));
    Require(identityOnly.Save["GameProgress"]["Loadout"].contains("broken"));
    Require(identityOnly.Save["GameProgress"]["Progress"]["RecipesUnlocked"]==Json::array({"recipe1"}));
    Require(identityOnly.Save["GameProgress"]["Journal"]["RuneSchemaOwnership"]
        ==strictIdentity["GameProgress"]["Journal"]["RuneSchemaOwnership"]);
    auto strictDamaged=damaged;
    strictDamaged["GameProgress"]["QuestProgress"]["Quests"].push_back("opaque-third-party-row");
    Require(Plan(strictDamaged,{}).Save==WithoutRetiredJournalMetadata(strictDamaged));
    auto journalRegistry=registry;
    journalRegistry.Journals={"vanilla"};journalRegistry.JournalsComplete=true;
    const auto orphanJournal=Plan(source,{},false,&journalRegistry,false,true,true);
    Require(orphanJournal.Save["GameProgress"]["Journal"]["UnlockedEntries"]==Json::array({"vanilla"}));
    Require(orphanJournal.Save["GameProgress"]["Journal"]["UnreadEntries"].empty());
    Require(orphanJournal.Save["GameProgress"]["Journal"]["FutureNativeField"]==42);
    journalRegistry.Journals.insert("book");
    const auto activeJournal=Plan(source,{},false,&journalRegistry,false,true,true);
    Require(activeJournal.Save["GameProgress"]["Journal"]==source["GameProgress"]["Journal"]);
    RegistrySnapshot incomplete;
    Require(Rejects([&]{Plan(assetSave,{},false,&incomplete);}));
    PublishRegistry(registry);auto snapshot=ReadRegistry();Require(snapshot && snapshot->Ready());
    {
        using Kind=PS::RegistryEnrollment::IdentityKind;
        PS::RegistryEnrollment::Await("quest:late",Kind::Quest,"late-quest");PS::RegistryEnrollment::Await("clone:late",Kind::Item,"late-clone");
        PS::RegistryEnrollment::Await("quest:late",Kind::Quest,"late-quest"); // duplicate discovery is one obligation
        PS::RegistryEnrollment::Await("journal:late",Kind::Journal,"late-journal");
        PS::RegistryEnrollment::Await("recipe:late",Kind::Recipe,"late-recipe");
        Require(PS::RegistryEnrollment::PendingCount()==4 && snapshot->Ready());
        PublishRegistry(registry);auto waiting=ReadRegistry();Require(waiting&&waiting->Ready());
        auto duringLoad=assetSave;
        auto& game=duringLoad["GameProgress"];
        game["Inventory"]["0"]["ItemData"]="late-clone";game["Inventory"]["8"]={{"ItemData","orphan-item"}};
        game["PersonalInventory"]["0"]={{"ItemData","late-clone"}};game["PersonalInventory"]["1"]={{"ItemData","orphan-item"}};
        game["QuestProgress"]["Quests"].push_back({{"QuestId","late-quest"}});
        game["QuestProgress"]["Quests"].push_back({{"QuestId","orphan-quest"}});
        game["QuestProgress"]["QuestTracked"]="late-quest";
        game["Journal"]={{"UnlockedEntries",{"late-journal","orphan-journal"}},{"UnreadEntries",{"late-journal","orphan-journal"}}};
        game["Progress"]={{"RecipesUnlocked",{"late-recipe","orphan-recipe"}},{"ItemsPickedUp",{"late-clone","orphan-item"}}};
        auto live=*waiting;live.JournalsComplete=true;
        const auto general=Plan(duringLoad,{},false,&live,false,true,true);const auto& cleaned=general.Save["GameProgress"];
        Require(cleaned["Inventory"].contains("0")&&!cleaned["Inventory"].contains("8"));
        Require(cleaned["PersonalInventory"].contains("0")&&!cleaned["PersonalInventory"].contains("1"));
        Require(cleaned["QuestProgress"]["QuestTracked"]=="late-quest");
        bool pendingQuest=false,orphanQuest=false;for(const auto& q:cleaned["QuestProgress"]["Quests"]){pendingQuest|=q.value("QuestId",std::string{})=="late-quest";orphanQuest|=q.value("QuestId",std::string{})=="orphan-quest";}
        Require(pendingQuest&&!orphanQuest);
        Require(cleaned["Journal"]["UnlockedEntries"]==Json::array({"late-journal"}));
        Require(cleaned["Progress"]["RecipesUnlocked"]==Json::array({"late-recipe"}));
        Require(cleaned["Progress"]["ItemsPickedUp"]==Json::array({"late-clone"}));
        Require(!general.Removed.empty()&&Plan(general.Save,{},false,&live,false,true,true).Removed.empty());
        // Protection is registry-specific: a pending quest cannot preserve an item.
        Require(!PS::RegistryEnrollment::IsPending(Kind::Item,"late-quest"));
        Require(IncludeVerifiedIdentity(VerifiedKind::Quest,"late-quest"));
        Require(IncludeVerifiedIdentity(VerifiedKind::Item,"late-clone"));
        PS::RegistryEnrollment::Complete("quest:late");Require(ReadRegistry()->Ready());
        PS::RegistryEnrollment::Complete("unknown");Require(ReadRegistry()->Ready());
        PS::RegistryEnrollment::Complete("clone:late");auto settled=ReadRegistry();Require(settled->Ready());
        Require(settled->Quests.contains("late-quest")&&settled->Items.contains("late-clone"));
        auto beforeWorld=assetSave;beforeWorld["GameProgress"]["Inventory"]["0"]["ItemData"]="late-clone";
        auto afterWorld=Plan(beforeWorld,{},false,settled.get(),false,true,true);
        Require(afterWorld.Save["GameProgress"]["Inventory"].contains("0"));
        // Another character in the same world also receives complete cleanup.
        auto another=assetSave;another["GameProgress"]["Inventory"]["9"]={{"ItemData","orphan-from-removed-mod"}};
        Require(!Plan(another,{},false,settled.get(),false,true,true).Save["GameProgress"]["Inventory"].contains("9"));
        // On the next launch after mod removal there are no pending declarations.
        // Their old IDs must not be exempt from orphan cleanup.
        PS::RegistryEnrollment::Reset();PublishRegistry(registry);
        auto removedMod=Plan(general.Save,{},false,&live,false,true,true);
        Require(!removedMod.Save["GameProgress"]["Inventory"].contains("0"));
        Require(removedMod.Save["GameProgress"]["QuestProgress"]["QuestTracked"]=="");
        Require(removedMod.Save["GameProgress"]["Journal"]["UnlockedEntries"].empty());
        Require(removedMod.Save["GameProgress"]["Progress"]["RecipesUnlocked"].empty());
    }
    {
        const auto clone=PS::ClonePresentation::NewId("AnyMod");Require(DragonWilds::IsCanonicalPersistenceId(clone));
        Require(PS::ClonePresentation::ValidId(id,"NotTheOriginalMod"));
        Require(IncludeVerifiedIdentity(VerifiedKind::Item,clone));Require(!snapshot->Items.contains(clone));
        auto live=ReadRegistry();Require(live->Items.contains(clone)&&live->Ready());
        Require(IncludeVerifiedIdentity(VerifiedKind::Quest,clone));Require(IncludeVerifiedIdentity(VerifiedKind::Journal,clone));
        live=ReadRegistry();Require(live->Quests.contains(clone)&&live->Journals.contains(clone));
        Require(Plan(movedOwners,{},false,live.get(),false,true,true).Save["GameProgress"]["QuestProgress"]==movedOwners["GameProgress"]["QuestProgress"]);
        auto mixed=assetSave;mixed["GameProgress"]["Inventory"]["0"]["ItemData"]=clone;
        mixed["GameProgress"]["Inventory"]["7"]={{"ItemData","removed-mod-item"}};
        mixed["GameProgress"]["PersonalInventory"]["0"]={{"ItemData","removed-mod-item"}};
        mixed["GameProgress"]["Loadout"]["Head"]={{"PlayerInventoryItemIndex",7}};
        auto& quests=mixed["GameProgress"]["QuestProgress"]["Quests"];
        quests.push_back({{"QuestId","removed-mod-without-ints"}});
        auto locationInts=Json::array({{{"QuestVariableName","RuneSchema.Location:removed-mod-location"},{"QuestVariableValue",7001}}});
        quests.push_back({{"QuestId","removed-mod-location-quest"},{"QuestInts",locationInts}});
        mixed["GameProgress"]["QuestProgress"]["QuestLocations"].push_back({{"QuestLocationId","removed-mod-location"}});
        quests.push_back({{"QuestId","removed-mod-broken-markers"},{"QuestInts",{{{"QuestVariableName","RuneSchema.Owner:"},{"QuestVariableValue",-1}}}}});
        auto cleaned=Plan(mixed,{},false,live.get(),false,true,true);
        Require(cleaned.Save["GameProgress"]["Inventory"]["0"]["ItemData"]==clone);
        Require(!cleaned.Save["GameProgress"]["Inventory"].contains("7"));
        Require(cleaned.Save["GameProgress"]["PersonalInventory"].empty());
        Require(!cleaned.Save["GameProgress"]["Loadout"].contains("Head"));
        Require(cleaned.Save["GameProgress"]["QuestProgress"]["Quests"].size()==2);
        Require(cleaned.Save["GameProgress"]["QuestProgress"]["QuestLocations"].size()==2);
        Require(Plan(cleaned.Save,{},false,live.get(),false,true,true).Save==cleaned.Save);
        // Simulate next launch after the clone mod is removed: no prefix/owner
        // exception may keep an identity that no longer resolves.
        PublishRegistry(registry);live=ReadRegistry();
        auto retired=Plan(cleaned.Save,{},false,live.get(),false,true,true);
        Require(!retired.Save["GameProgress"]["Inventory"].contains("0"));
        PublishRegistry({});Require(!IncludeVerifiedIdentity(VerifiedKind::Item,clone)&&!ReadRegistry());
    }
    PublishRegistry({});Require(!ReadRegistry() && snapshot->Ready());
    return 0;
}
int main(int argc,char** argv){try{
    if(argc>1){std::ifstream input(argv[1],std::ios::binary);Json save;input>>save;
        const auto preview=Plan(save,{"RuneSchema2VendorTest","TravellingMerchants"},true);
        std::cout<<preview.Removed.dump()<<'\n';return 0;}
    return Run();
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
