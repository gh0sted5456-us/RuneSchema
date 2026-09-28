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
}
