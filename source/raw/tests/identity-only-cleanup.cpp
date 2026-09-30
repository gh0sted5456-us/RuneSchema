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

    const Json source = {{"GameProgress", {
        {"Inventory", {
            {"0", {{"ItemData", "item0"}}},
            {"1", {{"ItemData", "missing-item"}}},
            {"broken", "opaque record"}}},
        {"Progress", {
            {"RecipesUnlocked", {"recipe0", "OadLWKUdVipKGcLFngpvFw"}},
            {"RecipesNew", {"recipe1"}}}},
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
}
