#pragma once
#include <algorithm>
#include <array>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace PS::NativeDataFamilies {
struct Family {const char* Lane;const char* DataClass;const char* Subsystem;};
// These subclasses expose only the five inherited data-registry containers.
// This enrolls cooked identities; it does not trigger gameplay hooks or grant entitlements.
inline constexpr std::array Families{
    Family{"ContainerContents","/Script/Dominion.ContainerContentDataAsset","/Script/Dominion.ContainerContentDataSubsystem"},
    Family{"FarmPlants","/Script/Dominion.FarmPlantDataAsset","/Script/Dominion.FarmPlantDataSubsystem"},
    Family{"Trees","/Script/Dominion.TreeData","/Script/Dominion.TreeDataSubsystem"},
    Family{"TreeGrowth","/Script/Dominion.TreeGrowStagesDataAsset","/Script/Dominion.TreeGrowingDataSubsystem"},
    Family{"FixedTextNotifications","/Script/Dominion.FixedTextNotificationData","/Script/Dominion.FixedTextNotificationDataSubsystem"},
    Family{"FTUENotifications","/Script/Dominion.FTUENotificationData","/Script/Dominion.FTUENotificationDataSubsystem"},
    Family{"PlayerHooks","/Script/Dominion.PlayerHook","/Script/Dominion.PlayerHooksSubsystem"},
    Family{"WorldHooks","/Script/Dominion.WorldHook","/Script/Dominion.WorldHooksSubsystem"}
};
inline const Family* Find(std::string_view lane){for(const auto& family:Families)if(lane==family.Lane)return &family;return nullptr;}
struct Identity {std::string Persistence,Internal,Asset,Path;std::size_t SourceIndex;};
inline std::vector<Identity> Order(std::vector<Identity> entries){
    if(entries.size()>4096)throw std::runtime_error("Cooked native family exceeds its 4096-asset limit");
    std::map<std::string,std::string> ids,names,assets;
    const auto claim=[](auto& claims,const std::string& alias,const std::string& path){
        if(alias.empty()||alias.size()>1024||alias.find_first_of("\r\n\t")!=alias.npos||alias.find('\0')!=alias.npos)throw std::runtime_error("Cooked native identity is empty or malformed");
        const auto [at,added]=claims.emplace(alias,path);if(!added&&at->second!=path)throw std::runtime_error("Conflicting cooked native alias: "+alias);
    };
    for(const auto& entry:entries){
        if(entry.Path.empty()||entry.Path.size()>1024)throw std::runtime_error("Cooked native asset path is empty or too long");
        claim(ids,entry.Persistence,entry.Path);claim(names,entry.Persistence,entry.Path);claim(names,entry.Internal,entry.Path);claim(assets,entry.Asset,entry.Path);
    }
    std::ranges::sort(entries,{},[](const Identity& entry){return std::pair{entry.Persistence,entry.Path};});return entries;
}
}
