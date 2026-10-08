#pragma once
#include <array>
#include <string_view>
#include <stdexcept>

namespace PS {
struct LoaderCapability {
    const char* Name;
    const char* DisplayName;
    bool AuthoredSearch, StarterPatch, Clone, Append;
    const char* Purpose;
};
inline constexpr std::array<LoaderCapability,22> LoaderCapabilities{{
    {"assets","Assets",false,true,true,true,"Edit or clone item data, icons, stats and supported appearance metadata."},
    {"blueprints","Blueprints",false,true,false,true,"Edit reflected class/component defaults and supported appearance rules; no Blueprint class cloning."},
    {"buildings","Buildings",false,false,true,false,"Register or clone BuildingPieceData and patch authored definitions; not a workstation Blueprint compiler."},
    {"courses","Courses",true,false,false,false,"Register courses and patch existing authored course IDs."},
    {"dialogue","Dialogue",false,false,false,false,"Native conversations, story rewards, bound shops, quest/event choices, replicated animation cues and action-driven Niagara. Requires npc; restart to apply."},
    {"effects","Effects",true,false,false,false,"Reusable aliases for cooked GameplayEffect Blueprint classes referenced by compatible asset fields."},
    {"enums","Enums",true,false,false,false,"Extend supported loaded enums with names; no patch or clone directives."},
    {"equipment","Equipment",true,false,false,false,"Validated Surge and Shadowveil runtime behavior; no generic patch or clone directives."},
    {"events","Events",false,false,false,false,"Server-authoritative temporary AI waves using EventOnly /spawns templates. Dialogue starts or cancels; event identity supports quest kill credit. Requires npc, dialogue and spawns. Restart to apply."},
    {"journal","Journal",true,false,false,false,"Add or patch journal definitions and discovery pages."},
    {"lore","Lore",true,false,false,false,"Create or edit native lore entries, text pages and images using the shared journal registry."},
    {"nameplates","Nameplates",true,false,false,false,"Reusable player nameplate definitions, icons, states and activity events referenced by /players."},
    {"niagara","Niagara",true,false,false,false,"Reusable cooked Niagara attachments and user parameters referenced by visual effects. Restart to apply changes."},
    {"npc","NPCs",true,false,false,false,"Persistent AI, Human or Resource visuals. DialogueID starts Talk; an optional VendorID binds the shop its dialogue can open. VendorID alone opens Trade. Restart to apply."},
    {"players","Players",false,false,false,false,"Player profiles, archetypes, nameplates and activity rules; patch existing authored rule IDs."},
    {"quests","Quests",false,false,false,false,"Per-character native fetch quests; local save/reload tested. Requires npc and dialogue for interaction. Optional objective locations are experimental. Restart to apply."},
    {"raw","Raw",false,true,false,true,"Add or patch DataTable rows, including loot and stat data."},
    {"recipes","Recipes",true,false,false,false,"Station recipes and patches; VendorID offers feed shared stores with verified purchase/reload support."},
    {"registry","Registry",false,false,false,true,"Merge JSON/JSONC and mounted cooked registry entries into the authority-validated action bridge."},
    {"spawns","Spawns",true,false,false,false,"World actors and AI, resources, respawn and supported visuals; patch existing definitions."},
    {"strings","Strings",false,false,false,false,"Replace source text globally or by table; last loaded wins within a scope."},
    {"vendors","Vendors",true,false,false,false,"Shared stores referenced by NPC VendorID. Supports recipe contributions and banner override. Requires npc and recipes; restart to apply."}
}};
inline const LoaderCapability& Capability(std::string_view name) {
    for (const auto& entry:LoaderCapabilities) if(name==entry.Name)return entry;
    throw std::runtime_error("Unknown loader");
}
}
