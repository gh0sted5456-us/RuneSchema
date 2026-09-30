#pragma once

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "Utility/Config.h"

namespace PS::SaveSnapshotRestore {
enum class Mode {
    MergeBaseline,
    ReplaceSelected,
};

struct Result {
    nlohmann::json Save;
    std::vector<std::string> Sections;
};

inline bool MergeMissing(nlohmann::json& live, const nlohmann::json& baseline)
{
    bool changed = false;
    if (live.is_object() && baseline.is_object()) {
        for (const auto& [key, value] : baseline.items()) {
            if (!live.contains(key)) { live[key] = value; changed = true; }
            else changed = MergeMissing(live[key], value) || changed;
        }
    } else if (live.is_array() && baseline.is_array()) {
        for (const auto& value : baseline)
            if (std::find(live.begin(), live.end(), value) == live.end()) {
                live.push_back(value);
                changed = true;
            }
    }
    // A present scalar is healthy live authority and is never overwritten.
    return changed;
}

inline bool MergeKeyedObject(nlohmann::json& live,
    const nlohmann::json& baseline)
{
    if (!live.is_object() || !baseline.is_object())
        throw std::runtime_error("selected keyed save section is not an object");
    bool changed = false;
    for (const auto& [key, value] : baseline.items())
        if (!live.contains(key)) { live[key] = value; changed = true; }
    return changed;
}

inline bool MergeIdentityArray(nlohmann::json& live,
    const nlohmann::json& baseline, const char* identity)
{
    if (!live.is_array() || !baseline.is_array())
        throw std::runtime_error("selected identity collection is not an array");
    std::set<std::string> identities;
    for (const auto& value : live)
        if (value.is_object() && value.contains(identity)
            && value.at(identity).is_string())
            identities.insert(value.at(identity).get<std::string>());
    bool changed = false;
    for (const auto& value : baseline) {
        if (!value.is_object() || !value.contains(identity)
            || !value.at(identity).is_string()) continue;
        if (identities.insert(value.at(identity).get<std::string>()).second) {
            live.push_back(value);
            changed = true;
        }
    }
    return changed;
}

inline bool MergeQuestProgress(nlohmann::json& live,
    const nlohmann::json& baseline)
{
    if (!live.is_object() || !baseline.is_object())
        throw std::runtime_error("QuestProgress is not an object");
    bool changed = false;
    for (const auto& [key, value] : baseline.items()) {
        if (!live.contains(key)) { live[key] = value; changed = true; continue; }
        if (key == "Quests") changed = MergeIdentityArray(
            live[key], value, "QuestId") || changed;
        else if (key == "QuestLocations")
            changed = MergeIdentityArray(
                live[key], value, "QuestLocationId") || changed;
        else changed = MergeMissing(live[key], value) || changed;
    }
    return changed;
}

inline Result Apply(const nlohmann::json& active,
    const nlohmann::json& snapshot,
    const DefaultRestorationSettings& settings, Mode mode)
{
    if (!active.is_object() || !active.contains("GameProgress")
        || !active.at("GameProgress").is_object())
        throw std::runtime_error("active character has no supported GameProgress object");
    if (!snapshot.is_object() || !snapshot.contains("GameProgress")
        || !snapshot.at("GameProgress").is_object())
        throw std::runtime_error("Default.json is not a complete gameplay character snapshot");

    Result result{active};
    auto& destination = result.Save.at("GameProgress");
    const auto& source = snapshot.at("GameProgress");
    const auto copy = [&](bool enabled, const char* key, const char* label,
                          bool keyed = false, bool quests = false) {
        if (!enabled) return;
        const auto& baseline = source.at(key);
        bool changed = false;
        if (mode == Mode::ReplaceSelected || !destination.contains(key)) {
            changed = !destination.contains(key) || destination.at(key) != baseline;
            if (changed) destination[key] = baseline;
        } else if (quests) changed = MergeQuestProgress(destination[key], baseline);
        else if (keyed) changed = MergeKeyedObject(destination[key], baseline);
        else changed = MergeMissing(destination[key], baseline);
        if (changed) result.Sections.emplace_back(label);
    };

    if (settings.appearance) {
        const auto& baseline = snapshot.at("Customization");
        bool changed = false;
        if (mode == Mode::ReplaceSelected || !result.Save.contains("Customization")) {
            changed = !result.Save.contains("Customization")
                || result.Save.at("Customization") != baseline;
            if (changed) result.Save["Customization"] = baseline;
        } else changed = MergeMissing(result.Save["Customization"], baseline);
        if (changed) result.Sections.emplace_back("Appearance");
    }
    copy(settings.inventory, "Inventory", "Inventory", true);
    copy(settings.personalInventory, "PersonalInventory", "PersonalInventory", true);
    copy(settings.loadout, "Loadout", "Loadout", true);
    copy(settings.unlockProgress, "Progress", "Progress");
    copy(settings.quests, "QuestProgress", "QuestProgress", false, true);
    copy(settings.journalLore, "Journal", "Journal/lore");

    if (settings.remainingGameProgress) {
        static const std::set<std::string> explicitSections{
            "Inventory", "PersonalInventory", "Loadout", "Progress",
            "QuestProgress", "Journal"
        };
        bool changed = false;
        for (const auto& [key, value] : source.items()) {
            if (explicitSections.contains(key)) continue;
            if (mode == Mode::ReplaceSelected || !destination.contains(key)) {
                const bool different = !destination.contains(key)
                    || destination.at(key) != value;
                if (different) { destination[key] = value; changed = true; }
            } else changed = MergeMissing(destination[key], value) || changed;
        }
        if (changed) result.Sections.emplace_back("Remaining GameProgress");
    }

    // meta_data, including char_guid and character name, always remains from
    // the active character. The snapshot can never replace account identity.
    return result;
}
}
