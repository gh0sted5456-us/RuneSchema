#include "Core/SaveCleanup.h"
#include <stdexcept>

int main()
{
    using namespace PS::SaveCleanup;
    RegistrySnapshot registry;
    for (int i = 0; i < 500; ++i)
        registry.Items.insert("item" + std::to_string(i));
    for (int i = 0; i < 300; ++i)
        registry.Recipes.insert("recipe" + std::to_string(i));
    registry.Journals.insert("known-journal");
    registry.JournalsComplete = true;
    registry.Quests.insert("baseline-quest");
    registry.QuestsComplete = true; registry.QuestCompletedState=2;

    const Json source = {{"GameProgress", {
        {"Inventory", {
            {"0", {{"ItemData", "item0"}}},
            {"1", {{"ItemData", "missing-item"}}},
            {"broken", "opaque record"}}},
        {"Progress", {
            {"RecipesUnlocked", {"recipe0", "OadLWKUdVipKGcLFngpvFw"}},
            {"RecipesNew", {"recipe1"}}}},
        {"QuestProgress", {
            {"QuestTracked", "missing-dialogue-state"},
            {"Quests", Json::array({{
                {"QuestId", "missing-dialogue-state"},
                {"QuestInts", Json::array()}}})},
            {"QuestLocations", Json::array()}}},
        {"Journal", {
            {"UnlockedEntries", {"known-journal", "missing-journal"}},
            {"UnreadEntries", {"known-journal"}},
            {"RuneSchemaOwnership", Json::array({"keep metadata"})}}}
    }}};
    const auto cleaned = Plan(source, {}, false, &registry, false, true, true);
    const auto& game = cleaned.Save.at("GameProgress");
    if (game.at("Inventory").contains("1")
        || !game.at("Inventory").contains("broken")
        || game.at("Progress").at("RecipesUnlocked") != Json::array({"recipe0"})
        || game.at("Progress").at("RecipesNew") != Json::array({"recipe1"})
        || game.at("Journal").at("UnlockedEntries") != Json::array({"known-journal"})
        || game.at("Journal").at("RuneSchemaOwnership")
            != source.at("GameProgress").at("Journal").at("RuneSchemaOwnership"))
        throw std::runtime_error("Identity-only cleanup changed unrelated character data");
    if (Plan(cleaned.Save, {}, false, &registry, false, true, true).Save != cleaned.Save)
        throw std::runtime_error("Identity-only cleanup is not idempotent");
    if (!Plan(cleaned.Save, {}, false, &registry, false, true, true).Removed.empty())
        throw std::runtime_error("Cleanup scheduled writes without orphaned IDs");
    {
        const std::string ashhide="yeS5-wOLGnik8eiTrnEodA";
        for(int phase:{1,2,3}){
            auto counters=DragonWilds::Quests::OwnershipVariables("JobBoardStarterQuests",ashhide);
            counters.push_back({{"QuestVariableName","RuneSchema.Phase"},{"QuestVariableValue",phase}});
            counters.push_back({{"QuestVariableName","RuneSchema.Location:ashhide-arena"},{"QuestVariableValue",7001}});
            auto active=source;active["GameProgress"]["QuestProgress"]={{"QuestTracked",ashhide},
                {"Quests",Json::array({{{"QuestId",ashhide},{"QuestState",1},{"QuestInts",counters}},{{"QuestId","baseline-quest"},{"QuestInts",Json::array()}}})},
                {"QuestLocations",Json::array({{{"QuestLocationId","ashhide-arena"}},{{"QuestLocationId","native-location"}}})}};
            const auto pruned=Plan(active,{},false,&registry,false,true,true);const auto& progress=pruned.Save["GameProgress"]["QuestProgress"];
            if(progress["Quests"].size()!=1||progress["Quests"][0]["QuestId"]!="baseline-quest"||progress["QuestTracked"]!=""||progress["QuestLocations"].size()!=1)
                throw std::runtime_error("Disabled Ashhide quest remained active or unrelated progress was removed");
            auto enabled=registry;enabled.Quests.insert(ashhide);
            if(Plan(active,{},false,&enabled,false,true,true).Save["GameProgress"]["QuestProgress"]!=active["GameProgress"]["QuestProgress"])
                throw std::runtime_error("Enabled active Ashhide quest was pruned");
            PS::RegistryEnrollment::Await("quest:JobBoardStarterQuests:bounty_ashhide",PS::RegistryEnrollment::IdentityKind::Quest,ashhide);
            if(IsOrphanQuest(ashhide,registry))throw std::runtime_error("Pending enabled quest treated as orphan");
            PS::RegistryEnrollment::Complete("quest:JobBoardStarterQuests:bounty_ashhide");
        }
    }

    // A failure discovered after a snapshot was published invalidates that
    // snapshot too. Missing registrations must never look like removed mods.
    PublishRegistry(registry);
    const auto previouslyPublished=ReadRegistry();
    if(!previouslyPublished || !previouslyPublished->Ready())
        throw std::runtime_error("Fixture did not publish a complete registry");
    PS::RegistryEnrollment::Reject("pak:/Example.RSREG_Test","enabled journal failed to register");
    if(registry.Ready() || previouslyPublished->Ready())
        throw std::runtime_error("Registration failure did not veto cleanup readiness");
    bool blocked=false;
    try{Plan(source,{},false,previouslyPublished.get(),false,true,true);}
    catch(const std::exception&){blocked=true;}
    if(!blocked)throw std::runtime_error("Incomplete enrollment authorized orphan pruning");
    PublishRegistry(registry);
    if(ReadRegistry())throw std::runtime_error("Unhealthy enrollment published a cleanup authority");
    PS::RegistryEnrollment::Reset();
    if(!registry.Ready())throw std::runtime_error("Fresh initialization did not reset the failure veto");

    {
        auto mixed=source;
        auto& quests=mixed["GameProgress"]["QuestProgress"];
        quests["QuestTracked"]="completed";
        const auto completed=Json{{"QuestId","completed"},{"QuestState",2},{"QuestInts",Json::array()}};
        quests["Quests"]=Json::array({completed,"opaque row",{{"QuestId","orphan"},{"QuestState",1},{"QuestInts",Json::array()}},
            {{"QuestId","finished-phase"},{"QuestInts",Json::array({{{"QuestVariableName","RuneSchema.Phase"},{"QuestVariableValue",4}}})}}});
        const auto result=PlanAvailable(mixed,registry);
        const auto& progress=result.Save["GameProgress"]["QuestProgress"];
        if(progress["Quests"].size()!=3||progress["Quests"][0]!=completed||progress["QuestTracked"]!="completed")
            throw std::runtime_error("Completed or uncertain rows stalled incomplete quest pruning or were erased");
        if(PlanAvailable(result.Save,registry).Save!=result.Save)throw std::runtime_error("Partial cleanup is not idempotent");

        PS::RegistryEnrollment::Reject("recipe","unavailable recipe registry",PS::RegistryEnrollment::IdentityKind::Recipe);
        const auto partial=PlanAvailable(source,registry);
        if(partial.Save["GameProgress"]["Inventory"].contains("1")||partial.Save["GameProgress"]["Progress"]!=source["GameProgress"]["Progress"]||partial.Skipped.empty())
            throw std::runtime_error("Unrelated recipe failure stopped item cleanup or authorized recipe deletion");
        PS::RegistryEnrollment::Reset();
        auto malformed=source;malformed["GameProgress"]["QuestProgress"]="unsupported";
        malformed["GameProgress"]["Progress"]["RecipesNew"]="unsupported";
        const auto independent=PlanAvailable(malformed,registry);
        if(independent.Save["GameProgress"]["Inventory"].contains("1")||independent.Save["GameProgress"]["QuestProgress"]!="unsupported"
            ||independent.Save["GameProgress"]["Progress"]["RecipesNew"]!="unsupported"||independent.Save["GameProgress"]["Progress"]["RecipesUnlocked"]!=Json::array({"recipe0"}))
            throw std::runtime_error("Malformed section rolled back another verified cleanup");
        auto itemOnly=registry;itemOnly.RecipesComplete=false;itemOnly.QuestsComplete=false;
        if(!itemOnly.AnyCleanable()||PlanAvailable(source,itemOnly).Save["GameProgress"]["Inventory"].contains("1"))
            throw std::runtime_error("Incomplete unrelated registry remained a global cleanup barrier");
    }

    {
        auto unusual=source;
        unusual["FutureRoot"]={{"Opaque",Json::array({42,"unchanged"})}};
        auto& game=unusual["GameProgress"];
        game["FutureData"]={{"Opaque",Json::array({1,"unchanged"})}};
        game["PersonalInventory"]="future storage layout";
        game["Loadout"]={{"0",{{"PlayerInventoryItemIndex",1}}},
            {"1",{{"PlayerInventoryItemIndex",uint64_t(4294967297ULL)}}},
            {"2",{{"PlayerInventoryItemIndex",-1}}},
            {"3",{{"PlayerInventoryItemIndex",uint64_t(-1)}}}};
        const auto cleaned=PlanAvailable(unusual,registry);
        const auto& result=cleaned.Save["GameProgress"];
        if(result["Inventory"].contains("1")||result["Loadout"].contains("0")||result["Loadout"].size()!=3
            ||result["PersonalInventory"]!=game["PersonalInventory"]||result["FutureData"]!=game["FutureData"]
            ||cleaned.Save["FutureRoot"]!=unusual["FutureRoot"]||cleaned.Skipped.empty())
            throw std::runtime_error("Unfamiliar storage blocked verified cleanup or an out-of-range equipment index was truncated");
        if(PlanAvailable(cleaned.Save,registry).Save!=cleaned.Save)throw std::runtime_error("Unfamiliar storage cleanup was not idempotent");
        game["Inventory"]="future inventory layout";
        game["PersonalInventory"]={{"0",{{"ItemData","missing-item"}}},{"1",{{"ItemData","item0"}}}};
        const auto personal=PlanAvailable(unusual,registry).Save["GameProgress"];
        if(personal["Inventory"]!=game["Inventory"]||personal["PersonalInventory"].contains("0")||!personal["Loadout"].contains("0"))
            throw std::runtime_error("Opaque inventory blocked independent storage cleanup or fabricated an equipment dependency");

        game["QuestProgress"]={{"QuestTracked","future-quest"},{"Quests",Json::array({
            {{"QuestId","future-quest"},{"QuestState","future state"}},
            {{"QuestId","orphan"},{"QuestState",1},{"QuestInts",Json::array({
                {{"QuestVariableName","RuneSchema.Location:shared"},{"QuestVariableValue",DragonWilds::Quests::OwnershipVersion}}})}}})},
            {"QuestLocations",Json::array({{{"QuestLocationId","shared"}}})}};
        const auto quests=PlanAvailable(unusual,registry);
        const auto& progress=quests.Save["GameProgress"]["QuestProgress"];
        if(progress["Quests"].size()!=1||progress["Quests"][0]!=game["QuestProgress"]["Quests"][0]
            ||progress["QuestTracked"]!="future-quest"||progress["QuestLocations"]!=game["QuestProgress"]["QuestLocations"]||quests.Skipped.empty())
            throw std::runtime_error("Unfamiliar quest lost tracking/locations or blocked a verified orphan removal");
    }

    auto earlyRegistry = registry;
    earlyRegistry.QuestsComplete = false;
    earlyRegistry.JournalsComplete = false;
    const auto early = Plan(source, {}, false, &earlyRegistry, false, true, true);
    if (early.Save.at("GameProgress").at("QuestProgress")
            != source.at("GameProgress").at("QuestProgress")
        || early.Save.at("GameProgress").at("Journal")
            != source.at("GameProgress").at("Journal"))
        throw std::runtime_error(
            "Early registry cleanup changed quest/dialogue or journal state");

    const Json defaultAppearance = {
        {"meta_data", Json::object()},
        {"Customization", {{"CustomizationData", {
            {"BodyType", {{"dataTable", "body"}, {"rowName", "male_A_01"}}},
            {"HairPreset", {{"dataTable", "hair"}, {"rowName", "default_hair"}}}
        }}}}
    };
    auto appearanceSource = source;
    appearanceSource["Customization"]["CustomizationData"] = {
        {"BodyType", {{"dataTable", "body"}, {"rowName", "female_B_02"}}},
        {"HairPreset", {{"dataTable", "hair"}, {"rowName", "missing_mod_hair"}}}
    };
    const auto repaired = RepairInvalidAppearance(appearanceSource, defaultAppearance,
        [](const std::string&, const std::string& row) {
            return row == "male_A_01" || row == "female_B_02"
                || row == "default_hair";
        });
    const auto& appearance = repaired.Save.at("Customization").at("CustomizationData");
    if (appearance.at("BodyType").at("rowName") != "female_B_02"
        || appearance.at("HairPreset").at("rowName") != "default_hair"
        || repaired.Removed.size() != 1)
        throw std::runtime_error("Appearance repair changed a valid custom field");

    appearanceSource["Customization"]["CustomizationData"].erase("BodyType");
    const auto missingBody = RepairInvalidAppearance(appearanceSource, defaultAppearance,
        [](const std::string&, const std::string& row) {
            return row == "male_A_01" || row == "default_hair";
        });
    if (missingBody.Save.at("Customization").at("CustomizationData").contains("BodyType"))
        throw std::runtime_error("Missing BodyType was rewritten without evidence of a custom value");
    auto missingAppearance = source;
    missingAppearance.erase("Customization");
    const auto rebuiltAppearance = RepairInvalidAppearance(missingAppearance,
        defaultAppearance, [](const std::string&, const std::string& row) {
            return row == "male_A_01" || row == "default_hair";
        });
    if (rebuiltAppearance.Save.at("Customization").at("CustomizationData")
            .at("HairPreset").at("rowName") != "default_hair"
        || rebuiltAppearance.Save.at("Customization").at("CustomizationData")
            .contains("BodyType"))
        throw std::runtime_error("Missing appearance container was not rebuilt conservatively");

    auto live = source;
    live["GameProgress"]["Inventory"].erase("1");
    live["GameProgress"]["QuestProgress"] = {
        {"QuestTracked", ""}, {"Quests", Json::array()},
        {"QuestLocations", Json::array()}};
    auto baseline = live;
    baseline["GameProgress"]["Inventory"]["0"] = {
        {"ItemData", "item3"}, {"Count", 99}};
    baseline["GameProgress"]["Inventory"]["2"] = {
        {"ItemData", "item2"}, {"Count", 1}};
    baseline["GameProgress"]["Inventory"]["3"] = {
        {"ItemData", "missing-item"}, {"Count", 1}};
    baseline["GameProgress"]["Progress"]["RecipesUnlocked"] = {
        "recipe0", "recipe2", "missing-recipe"};
    baseline["GameProgress"]["QuestProgress"]["Quests"].push_back({
        {"QuestId", "baseline-quest"}, {"QuestInts", Json::array()}});
    baseline["GameProgress"]["QuestProgress"]["QuestTracked"] = "baseline-quest";
    const auto merged = MergeBaseline(live, baseline, registry,
        {.Items = true, .Quests = true, .Progress = true});
    if (merged.Save["GameProgress"]["Inventory"]["0"]
            != live["GameProgress"]["Inventory"]["0"]
        || !merged.Save["GameProgress"]["Inventory"].contains("2")
        || merged.Save["GameProgress"]["Inventory"].contains("3")
        || merged.Save["GameProgress"]["Progress"]["RecipesUnlocked"]
            != Json::array({"recipe0", "OadLWKUdVipKGcLFngpvFw", "recipe2"})
        || merged.Save["GameProgress"]["QuestProgress"]["Quests"].size() != 1
        || merged.Save["GameProgress"]["QuestProgress"]["QuestTracked"]
            != "baseline-quest")
        throw std::runtime_error("Baseline recovery overwrote live data or accepted an unresolved identity");
    if (MergeBaseline(merged.Save, baseline, registry,
            {.Items = true, .Quests = true, .Progress = true}).Save != merged.Save)
        throw std::runtime_error("Baseline recovery is not idempotent");
    if (MergeBaseline(live, baseline, registry, {}).Save != live)
        throw std::runtime_error("Disabled baseline categories changed the live save");
    {
        auto invalid=baseline;
        invalid["GameProgress"]["Loadout"]={{"Head",{{"PlayerInventoryItemIndex",uint64_t(4294967298ULL)}}},
            {"Hand",{{"ItemData","item0"},{"PlayerInventoryItemIndex",-1}}}};
        const auto restored=MergeBaseline(live,invalid,registry,{.Items=true});
        if(!restored.Save["GameProgress"]["Loadout"].empty())
            throw std::runtime_error("Baseline restore accepted an invalid equipment index");
        auto owned=source;owned["GameProgress"]["Loadout"]={{"Head",{{"PlayerInventoryItemIndex",uint64_t(4294967297ULL)}}},
            {"Hand",{{"PlayerInventoryItemIndex",1}}}};
        const auto pruned=PlanOwned(owned,{{"missing-item","Example"}},{});
        if(!pruned.Save["GameProgress"]["Loadout"].contains("Head")||pruned.Save["GameProgress"]["Loadout"].contains("Hand"))
            throw std::runtime_error("Owned cleanup truncated an equipment index to an unrelated retired slot");
    }
    if (AppearanceProfile(defaultAppearance).at("Customization")
            != defaultAppearance.at("Customization"))
        throw std::runtime_error("Appearance baseline extraction changed the profile");
}
