#include "Core/SaveSnapshotRestore.h"
#include <cassert>

int main()
{
    using Json = nlohmann::json;
    const Json active = {
        {"meta_data", {{"char_guid", "active-guid"}, {"char_name", "Active"}}},
        {"Customization", {{"CustomizationData", {{"Head", "active-head"}}}}},
        {"GameProgress", {
            {"Inventory", {{"0", {{"ItemData", "active-item"}}}}},
            {"PersonalInventory", Json::object()}, {"Loadout", Json::object()},
            {"Progress", {{"RecipesUnlocked", Json::array({"active-recipe"})}}},
            {"QuestProgress", {{"Quests", Json::array()}}},
            {"Journal", {{"UnlockedEntries", Json::array({"active-lore"})}}},
            {"WorldFlags", {{"active", true}}}
        }}
    };
    auto snapshot = active;
    snapshot["meta_data"] = {{"char_guid", "snapshot-guid"}, {"char_name", "Snapshot"}};
    snapshot["Customization"]["CustomizationData"]["Head"] = "snapshot-head";
    snapshot["GameProgress"]["Inventory"] = {{"4", {{"ItemData", "snapshot-item"}}}};
    snapshot["GameProgress"]["QuestProgress"] = {
        {"Quests", Json::array({{{"QuestId", "snapshot-quest"}}})}};
    snapshot["GameProgress"]["Journal"] = {{"UnlockedEntries", Json::array({"snapshot-lore"})}};
    snapshot["GameProgress"]["WorldFlags"] = {{"snapshot", true}};

    PS::DefaultRestorationSettings selected;
    selected.enabled = true;
    selected.inventory = true;
    selected.quests = true;
    auto restored = PS::SaveSnapshotRestore::Apply(active, snapshot, selected,
        PS::SaveSnapshotRestore::Mode::MergeBaseline);
    assert(restored.Save["meta_data"]["char_guid"] == "active-guid");
    assert(restored.Save["meta_data"]["char_name"] == "Active");
    assert(restored.Save["GameProgress"]["Inventory"].contains("4"));
    assert(restored.Save["GameProgress"]["Inventory"].contains("0"));
    assert(restored.Save["GameProgress"]["QuestProgress"]["Quests"][0]["QuestId"]
        == "snapshot-quest");
    assert(restored.Save["GameProgress"]["Journal"]["UnlockedEntries"][0] == "active-lore");
    assert(restored.Save["Customization"]["CustomizationData"]["Head"] == "active-head");

    const auto noOp = PS::SaveSnapshotRestore::Apply(active, active, selected,
        PS::SaveSnapshotRestore::Mode::MergeBaseline);
    assert(noOp.Save == active && noOp.Sections.empty());

    PS::DefaultRestorationSettings extended;
    extended.appearance = true;
    extended.remainingGameProgress = true;
    restored = PS::SaveSnapshotRestore::Apply(active, snapshot, extended,
        PS::SaveSnapshotRestore::Mode::MergeBaseline);
    assert(restored.Save["Customization"]["CustomizationData"]["Head"] == "active-head");
    assert(restored.Save["GameProgress"]["WorldFlags"].contains("snapshot"));
    assert(restored.Save["GameProgress"]["Inventory"].contains("0"));
    assert(restored.Save["meta_data"]["char_guid"] == "active-guid");

    auto missingAppearance = active;
    missingAppearance["Customization"]["CustomizationData"].erase("Head");
    restored = PS::SaveSnapshotRestore::Apply(
        missingAppearance, snapshot, extended,
        PS::SaveSnapshotRestore::Mode::MergeBaseline);
    assert(restored.Save["Customization"]["CustomizationData"]["Head"] == "snapshot-head");

    restored = PS::SaveSnapshotRestore::Apply(active, snapshot, selected,
        PS::SaveSnapshotRestore::Mode::ReplaceSelected);
    assert(restored.Save["GameProgress"]["Inventory"].contains("4"));
    assert(!restored.Save["GameProgress"]["Inventory"].contains("0"));
    assert(restored.Save["meta_data"]["char_guid"] == "active-guid");
}
