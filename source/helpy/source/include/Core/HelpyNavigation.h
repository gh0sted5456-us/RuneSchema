#pragma once
#include <array>
#include <string_view>

namespace PS::HelpyNavigation {
enum class Page { Items, Creation, Recipes, Spawns, Map, Tools, Authoring, Settings, LiveLog, DedicatedServer, SaveEditor, Home, Journal, Mods };
enum class SpawnSection { Resources, Npcs, Ai };
enum class ToolSection { Settings, Maintenance, Cleanup, Buildings, ItemCache };
inline constexpr std::array Pages{Page::Home, Page::Items, Page::Creation, Page::Recipes, Page::Spawns, Page::Map, Page::Journal, Page::Authoring, Page::Tools, Page::Settings, Page::LiveLog, Page::SaveEditor, Page::DedicatedServer,Page::Mods};
// Keep route identities stable: regrouping must not reset editors or drafts.
inline constexpr std::array SidebarPages{Page::Home,Page::Items,Page::Map,Page::Mods,Page::Tools,Page::Settings,Page::LiveLog,Page::DedicatedServer};
inline constexpr std::array PlayerTabs{Page::Items,Page::Creation,Page::Recipes,Page::SaveEditor};
inline constexpr std::array WorldTabs{Page::Map,Page::Spawns,Page::Journal};
inline constexpr std::array ToolTabs{Page::Tools,Page::Authoring};
inline constexpr Page Parent(Page page){
    switch(page){case Page::Creation:case Page::Recipes:case Page::SaveEditor:return Page::Items;
    case Page::Spawns:case Page::Journal:return Page::Map;case Page::Authoring:return Page::Tools;default:return page;}
}
inline constexpr bool TopLevel(Page page){return Parent(page)==page;}
inline constexpr const char* Title(Page page) {
    switch(page) {
    case Page::Home:return "Home";
    case Page::Mods:return "Mods";
    case Page::Items:return "Players";
    case Page::Creation:return "Clone";
    case Page::Recipes:return "Recipes";
    case Page::Spawns:return "Spawns";
    case Page::Map:return "World";
    case Page::Tools:return "Tools";
    case Page::Settings:return "Settings";
    case Page::Authoring:return "Authoring";
    case Page::LiveLog:return "Live Log";
    case Page::DedicatedServer:return "Server / Co-op";
    case Page::SaveEditor:return "Character Editor";
    case Page::Journal:return "Journal";
    }
    return "RSMK";
}
inline constexpr const char* TabTitle(Page page){return page==Page::Items?"Items":page==Page::Map?"Map":page==Page::Tools?"Utilities":Title(page);}
inline constexpr const char* Description(Page page) {
    switch(page) {
    case Page::Home:return "Welcome to RuneSchema. Your tools, cached content and common settings in one place.";
    case Page::Mods:return "Manage next-launch mod order, imports and source artwork. Mounted content stays unchanged.";
    case Page::Items:return "Right-click an item for actions, or drag it into your cart.";
    case Page::Creation:return "Choose a source, customize its appearance or properties, and create your item.";
    case Page::Recipes:return "Connect ingredients, a crafting station, and outputs. Drag items from the bay below.";
    case Page::Spawns:return "Choose resources, NPCs, or enemies, then place them near a player.";
    case Page::Map:return "Read coordinates, place resources and buildings, or teleport a player to a map point.";
    case Page::Tools:return "Maintain catalogs and manage session tools.";
    case Page::Settings:return "Configure RuneSchema for the next launch, or adjust RSMK appearance.";
    case Page::Authoring:return "Create content, inspect contracts, and export reviewed drafts and reports.";
    case Page::LiveLog:return "Read Game, UE4SS, RuneSchema and EOS logs, with clear events and linked diagnostic references.";
    case Page::DedicatedServer:return "Open co-op session tools or review configuration on the dedicated server itself.";
    case Page::SaveEditor:return "Edit and clean offline character copies using current mod information; review before exporting.";
    case Page::Journal:return "Browse native vanilla/modded journal and lore pages, view artwork, and author new RuneSchema entries. Wiki articles remain optional references.";
    }
    return "";
}
struct Workspaces {bool Authoring=false,Tools=false,LiveLog=false,SaveEditor=false,ServerTools=false,Journal=true;};
inline constexpr bool Visible(Page page,const Workspaces& enabled={}) {
    switch(page){case Page::Authoring:return enabled.Authoring;
    case Page::LiveLog:return enabled.LiveLog;case Page::SaveEditor:return enabled.SaveEditor;case Page::Journal:return enabled.Journal;default:return true;}
}
inline constexpr bool Available(Page page,const Workspaces& enabled,bool dedicatedServer) {
    return Visible(page,enabled)&&(page!=Page::DedicatedServer||dedicatedServer||enabled.ServerTools);
}
inline constexpr bool ShowSidebar(Page page) { return page!=Page::Home; }
}
