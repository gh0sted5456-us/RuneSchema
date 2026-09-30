#pragma once

#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "Utility/Config.h"

namespace PS::SaveSnapshotRestore {
struct Result {
    nlohmann::json Save;
    std::vector<std::string> Sections;
};

inline Result Apply(const nlohmann::json& active,
    const nlohmann::json& snapshot,
    const DefaultRestorationSettings& settings)
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
    const auto copy = [&](bool enabled, const char* key, const char* label) {
        if (!enabled) return;
        destination[key] = source.at(key);
        result.Sections.emplace_back(label);
    };

    if (settings.appearance) {
        result.Save["Customization"] = snapshot.at("Customization");
        result.Sections.emplace_back("Appearance");
    }
    copy(settings.inventory, "Inventory", "Inventory");
    copy(settings.personalInventory, "PersonalInventory", "PersonalInventory");
    copy(settings.loadout, "Loadout", "Loadout");
    copy(settings.unlockProgress, "Progress", "Progress");
    copy(settings.quests, "QuestProgress", "QuestProgress");
    copy(settings.journalLore, "Journal", "Journal/lore");

    if (settings.remainingGameProgress) {
        static const std::set<std::string> explicitSections{
            "Inventory", "PersonalInventory", "Loadout", "Progress",
            "QuestProgress", "Journal"
        };
        for (const auto& [key, value] : source.items()) {
            if (explicitSections.contains(key)) continue;
            destination[key] = value;
        }
        result.Sections.emplace_back("Remaining GameProgress");
    }

    // meta_data, including char_guid and character name, always remains from
    // the active character. The snapshot can never replace account identity.
    return result;
}
}
