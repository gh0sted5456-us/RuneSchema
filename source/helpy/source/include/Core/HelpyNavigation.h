#pragma once
#include <array>
#include <string_view>

namespace PS::HelpyNavigation {
enum class Page { Items, Creation, Recipes, Spawns, Map, Tools, Authoring, Settings, LiveLog, DedicatedServer, SaveEditor, Home, Journal, Mods, Profile, BugReport, SaveRecovery, Notifications, ModMenu2 };
enum class SpawnSection { Resources, Npcs, Ai };
enum class ToolSection { Settings, Maintenance, Cleanup, Buildings, ItemCache, CacheBuilder, Repair, Extensions, Developer };
inline constexpr std::array Pages{Page::Home, Page::Items, Page::Creation, Page::Recipes, Page::Spawns, Page::Map, Page::Journal, Page::Authoring, Page::Tools, Page::Settings, Page::LiveLog, Page::SaveEditor, Page::DedicatedServer,Page::Mods,Page::Profile,Page::BugReport,Page::SaveRecovery,Page::Notifications,Page::ModMenu2};
// Keep route identities stable: regrouping must not reset editors or drafts.
inline constexpr std::array SidebarPages{Page::Home,Page::Items,Page::Map,Page::Mods,Page::SaveRecovery,Page::Tools,Page::Settings,Page::LiveLog,Page::BugReport,Page::DedicatedServer,Page::Notifications};
inline constexpr std::array PlayerTabs{Page::Items,Page::Creation,Page::Recipes};
inline constexpr std::array ProfileTabs{Page::Profile,Page::SaveEditor};
inline constexpr std::array WorldTabs{Page::Map,Page::Spawns,Page::Journal};
inline constexpr std::array ToolTabs{Page::Tools,Page::Authoring};
inline constexpr std::array ModTabs{Page::Mods,Page::ModMenu2};
inline constexpr Page Parent(Page page){
    switch(page){case Page::Creation:case Page::Recipes:return Page::Items;case Page::SaveEditor:return Page::Profile;
    case Page::Spawns:case Page::Journal:return Page::Map;case Page::Authoring:return Page::Tools;case Page::ModMenu2:return Page::Mods;default:return page;}
}
inline constexpr bool TopLevel(Page page){return Parent(page)==page;}
inline constexpr const char* Title(Page page) {
    switch(page) {
    case Page::Home:return "Home";
    case Page::Profile:return "Profile";
    case Page::BugReport:return "Report a bug";
    case Page::Notifications:return "Notifications";
    case Page::ModMenu2:return "Mod Menu 2";
    case Page::Mods:return "Mods";
    case Page::SaveRecovery:return "Save Recovery";
    case Page::Items:return "Items";
    case Page::Creation:return "Clone";
    case Page::Recipes:return "Recipes";
    case Page::Spawns:return "Spawns";
    case Page::Map:return "World";
    case Page::Tools:return "Tools";
    case Page::Settings:return "Settings";
    case Page::Authoring:return "Authoring";
    case Page::LiveLog:return "Diagnostics";
    case Page::DedicatedServer:return "Server / Co-op";
    case Page::SaveEditor:return "Character";
    case Page::Journal:return "Journal";
    }
    return "RSMK";
}
inline constexpr const char* TabTitle(Page page){return page==Page::Items?"Items":page==Page::Map?"Map":page==Page::Tools?"Utilities":page==Page::Mods?"Management":Title(page);}
inline constexpr const char* Description(Page page) {
    switch(page) {
    case Page::Profile:return "Your account profile, linked characters, portraits and shared details.";
    case Page::BugReport:return "Open RuneSchema's Nexus Mods Bugs page.";
    case Page::Notifications:return "Cache updates, game changes and completed jobs for this runtime.";
    case Page::ModMenu2:return "Live mod controls, character-editor panels and compatible Mod Menu settings.";
    case Page::Home:return "Welcome to RuneSchema. Your tools, cached content and common settings in one place.";
    case Page::Mods:return "Manage next-launch mod order, imports and source artwork. Mounted content stays unchanged.";
    case Page::SaveRecovery:return "Review character and world snapshots, recorded mods and backup reasons, then restore with the game closed.";
    case Page::Items:return "Select items and send them to the cart, drag them in, or right-click for more actions.";
    case Page::Creation:return "Choose a source, customize its appearance or properties, and create your item.";
    case Page::Recipes:return "Connect ingredients, a crafting station, and outputs. Drag items from the bay below.";
    case Page::Spawns:return "Choose resources, NPCs, or enemies, then place them near a player.";
    case Page::Map:return "Read coordinates, place resources and buildings, or teleport a player to a map point.";
    case Page::Tools:return "Manage content, integrations and advanced capabilities.";
    case Page::Settings:return "Adjust Helpy, game integration, appearance and diagnostics.";
    case Page::Authoring:return "Create content, inspect contracts, and export reviewed drafts and reports.";
    case Page::LiveLog:return "Live logs and advanced diagnostics: grouped issues, retained evidence, session history and reviewed reports.";
    case Page::DedicatedServer:return "Open co-op session tools or review configuration on the dedicated server itself.";
    case Page::SaveEditor:return "Linked characters and copy-only save editing.";
    case Page::Journal:return "Browse native vanilla/modded journal and lore pages, view artwork, and author new RuneSchema entries. Wiki articles remain optional references.";
    }
    return "";
}
struct Workspaces {bool Authoring=false,Tools=false,LiveLog=false,SaveEditor=false,ServerTools=false,Journal=true,ModMenu=false,Clone=false,Recipes=false;};
inline constexpr bool Visible(Page page,const Workspaces& enabled={}) {
    switch(page){case Page::ModMenu2:return enabled.ModMenu;case Page::Authoring:return enabled.Authoring;
    case Page::Creation:return enabled.Clone;case Page::Recipes:return enabled.Recipes;
    case Page::LiveLog:return enabled.LiveLog;case Page::SaveEditor:return true;case Page::Journal:return enabled.Journal;default:return true;}
}
inline constexpr bool Available(Page page,const Workspaces& enabled,bool dedicatedServer) {
    return Visible(page,enabled)&&(page!=Page::DedicatedServer||dedicatedServer||enabled.ServerTools);
}
inline constexpr bool ShowSidebar(Page page) { return page!=Page::Home; }
struct SidebarState {
    bool ControllerExpanded=false,UsingController=false;
    bool Collapsed(bool manual,bool pinned,bool controller){if(controller!=UsingController)ControllerExpanded=false;UsingController=controller;return manual||(controller&&!pinned&&!ControllerExpanded);}
    void Toggle(bool& manual,bool pinned,bool controller){const bool collapsed=Collapsed(manual,pinned,controller);manual=!collapsed;ControllerExpanded=collapsed&&controller;}
    void TogglePin(bool& manual,bool& pinned){pinned=!pinned;ControllerExpanded=pinned;if(pinned)manual=false;}
    void Navigate(){ControllerExpanded=false;}
};
}
