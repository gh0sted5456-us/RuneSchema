#pragma once
#include "Runtime/Storefront.h"
#include <windows.h>
#include <nlohmann/json.hpp>
namespace PS::SteamBuild {
// Flat Steamworks exports only. Reuse the game's initialized client; never
// initialize/shut down Steam, load another DLL, or guess a vtable slot.
inline nlohmann::json Snapshot(){
    nlohmann::json result=nlohmann::json::object();
    if(Storefront::Current()!=Storefront::Kind::SteamEpic||Storefront::IsDedicatedServer()
        ||Storefront::CurrentDetection().EpicLauncher)return result;
    const auto module=GetModuleHandleW(L"steam_api64.dll");if(!module)return result;
    using Accessor=void* (__cdecl*)();using Read=int (__cdecl*)(void*);
    const auto read=reinterpret_cast<Read>(GetProcAddress(module,"SteamAPI_ISteamApps_GetAppBuildId"));
    if(!read)return result;
    for(const auto* name:{"SteamAPI_SteamApps_v009","SteamAPI_SteamApps_v008"}){
        const auto accessor=reinterpret_cast<Accessor>(GetProcAddress(module,name));if(!accessor)continue;
        auto* apps=accessor();if(!apps)continue;const auto build=read(apps);
        if(build>0)result={{"BuildId",build},{"Source","Steamworks ISteamApps::GetAppBuildId"}};
        break;
    }
    return result;
}
}
