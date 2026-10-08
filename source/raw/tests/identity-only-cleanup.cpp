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
    registry.QuestsComplete = true;

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
    if (AppearanceProfile(defaultAppearance).at("Customization")
            != defaultAppearance.at("Customization"))
        throw std::runtime_error("Appearance baseline extraction changed the profile");
}
