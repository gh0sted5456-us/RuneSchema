#pragma once
#include <array>
#include <string_view>

namespace PS::HelpyNavigation {
enum class Page { Items, Creation, Recipes, Spawns, Map, Tools, Authoring, Settings, LiveLog, DedicatedServer };
enum class SpawnSection { Resources, Npcs, Ai };
enum class ToolSection { Settings, Maintenance, Cleanup, Buildings };
inline constexpr std::array Pages{Page::Items, Page::Creation, Page::Recipes, Page::Spawns, Page::Map, Page::Authoring, Page::Tools, Page::Settings, Page::LiveLog, Page::DedicatedServer};
inline constexpr const char* Title(Page page) {
    switch(page) {
    case Page::Items:return "Item Service";
    case Page::Creation:return "Transmog & Creation";
    case Page::Recipes:return "Recipes";
    case Page::Spawns:return "Spawns";
    case Page::Map:return "Map";
    case Page::Tools:return "Tools";
    case Page::Settings:return "Settings";
    case Page::Authoring:return "Authoring Tools";
    case Page::LiveLog:return "Live Log";
    case Page::DedicatedServer:return "Dedicated Server";
    }
    return "RSMK";
}
inline constexpr const char* Description(Page page) {
    switch(page) {
    case Page::Items:return "Right-click an item for actions, or drag it into your cart.";
    case Page::Creation:return "Choose a source, customize its appearance or properties, and create your item.";
    case Page::Recipes:return "Connect ingredients, a crafting station, and outputs. Drag items from the bay below.";
    case Page::Spawns:return "Choose resources, NPCs, or enemies, then place them near a player.";
    case Page::Map:return "Read coordinates, place resources and buildings, or teleport a player to a map point.";
    case Page::Tools:return "Maintain catalogs and manage session tools.";
    case Page::Settings:return "Configure RuneSchema for the next launch, or adjust RSMK appearance.";
    case Page::Authoring:return "Create content, inspect contracts, and export reviewed drafts and reports.";
    case Page::LiveLog:return "Search, filter and export RuneSchema diagnostics, with linked troubleshooting codes.";
    case Page::DedicatedServer:return "Review local server settings and stage changes for a safe offline apply.";
    }
    return "";
}
// Every destination remains available in both modes. Advanced controls are
// disclosed inside their workspace; the mode switch never discards navigation.
inline constexpr bool Visible(Page) {return true;}
}
