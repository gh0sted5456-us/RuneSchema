#pragma once
// Readable content badges for a RuneSchema mod row. The core reports each mod's content folders as
// "Loaders" ("RuneSchema: quests", "Unreal PAK", "LogicMods", "UE4SS"); this turns them into short,
// ordered, de-duplicated labels with a category for colour and a tooltip explaining each.
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace PS::ModContentBadges {
enum class Category { Content, World, Story, Presentation, Technical, Packaging };
// Views into static literals: building badges every frame allocates nothing beyond the vector.
struct Badge { std::string_view Key, Label, Description; Category Kind = Category::Content; int Order = 0; };

namespace Detail {
struct Entry { std::string_view Folder, Label, Description; Category Kind; };
// Order inside this table is display order. Unknown folders are ignored rather than shown raw.
inline constexpr std::array<Entry, 26> Known{{
    {"assets", "Items", "Adds or patches items and assets (assets folder).", Category::Content},
    {"equipment", "Equipment", "Defines wearable equipment rules (equipment folder).", Category::Content},
    {"recipes", "Recipes", "Adds or changes crafting recipes (recipes folder).", Category::Content},
    {"vendors", "Vendors", "Adds or changes vendor offers (vendors folder).", Category::Content},
    {"buildings", "Buildings", "Adds or changes buildable pieces (buildings folder).", Category::World},
    {"spawns", "Spawns", "Places creatures, NPCs or resources in the world (spawns folder).", Category::World},
    {"npc", "NPCs", "Adds or changes NPCs (npc folder).", Category::World},
    {"events", "Events", "Adds world or activity events (events folder).", Category::World},
    {"courses", "Courses", "Adds courses (courses folder).", Category::World},
    {"players", "Players", "Changes player rules or defaults (players folder).", Category::World},
    {"quests", "Quests", "Adds quests or questboard entries (quests folder).", Category::Story},
    {"dialogue", "Dialogue", "Adds NPC dialogue (dialogue folder).", Category::Story},
    {"journal", "Journal", "Adds journal entries (journal folder).", Category::Story},
    {"lore", "Lore", "Adds lore entries (lore folder).", Category::Story},
    {"strings", "Text", "Replaces or adds in-game text (strings folder).", Category::Presentation},
    {"nameplates", "Nameplates", "Changes nameplates (nameplates folder).", Category::Presentation},
    {"effects", "Effects", "Defines gameplay effects (effects folder).", Category::Presentation},
    {"niagara", "VFX", "Adds Niagara visual-effect presets (niagara folder).", Category::Presentation},
    {"raw", "Data tables", "Patches game data tables directly (raw folder).", Category::Technical},
    {"raws", "Data tables", "Patches game data tables directly (raws folder).", Category::Technical},
    {"registry", "Registry", "Registers cooked content with the game's registries (registry folder).", Category::Technical},
    {"enums", "Enums", "Extends game enumerations (enums folder).", Category::Technical},
    {"blueprints", "Blueprints", "Patches blueprint defaults or widgets (blueprints folder).", Category::Technical},
    {"paks", "Cooked PAK", "Ships cooked Unreal content in a paks folder.", Category::Packaging},
    {"logicmods", "LogicMods", "Ships Blueprint logic mods in a logicmods folder.", Category::Packaging},
    {"ue4ss", "UE4SS", "Ships a UE4SS companion in a ue4ss folder.", Category::Packaging},
}};
inline bool Same(std::string_view left, std::string_view right) {
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) {
        return (a >= 'A' && a <= 'Z' ? a + 32 : a) == (b >= 'A' && b <= 'Z' ? b + 32 : b); });
}
// Maps one Loaders entry to its folder name (case as reported), or empty when the entry is not recognised.
inline std::string_view Folder(std::string_view loader) {
    constexpr std::string_view prefix = "RuneSchema: ";
    if (loader.starts_with(prefix)) return loader.substr(prefix.size());
    if (loader == "Unreal PAK") return "paks";
    if (loader == "LogicMods") return "logicmods";
    if (loader == "UE4SS") return "ue4ss";
    return {};
}
}

inline std::vector<Badge> ForRow(const nlohmann::json& row) {
    std::vector<Badge> badges;
    const auto loaders = row.find("Loaders");
    if (loaders != row.end() && loaders->is_array()) {
        for (const auto& loader : *loaders) {
            if (!loader.is_string()) continue;
            const auto folder = Detail::Folder(loader.get_ref<const std::string&>());
            for (size_t index = 0; index < Detail::Known.size(); ++index) {
                const auto& known = Detail::Known[index];
                if (!Detail::Same(known.Folder, folder)) continue;
                // raw and raws share a label; keep the first.
                if (std::none_of(badges.begin(), badges.end(), [&](const Badge& badge) { return badge.Label == known.Label; }))
                    badges.push_back({known.Folder, known.Label, known.Description, known.Kind, static_cast<int>(index)});
                break;
            }
        }
    }
    if (row.value("HasAchievements", false))
        badges.push_back({"achievements", "Achievements", "Includes Helpy achievements (achievements.json). Earn points on your profile.", Category::Story, 999});
    if (row.value("HasCache", false))
        badges.push_back({"cache", "Helpy cache", "Ships a cache folder with item names and icons for offline browsing.", Category::Packaging, 1000});
    std::stable_sort(badges.begin(), badges.end(), [](const Badge& left, const Badge& right) { return left.Order < right.Order; });
    return badges;
}
// One readable line for tooltips and accessibility: "Items, Quests, Cooked PAK".
inline std::string Summary(const std::vector<Badge>& badges) {
    std::string text;
    for (const auto& badge : badges) { if (!text.empty()) text += ", "; text.append(badge.Label); }
    return text.empty() ? std::string("No RuneSchema content folders") : text;
}
}
