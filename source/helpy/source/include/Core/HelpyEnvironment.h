#pragma once
#include <nlohmann/json.hpp>
namespace PS::HelpyEnvironment {
enum class Kind { Unknown, Steam, GamePass, Dedicated, Epic, SteamEpic };
inline Kind Current=Kind::Unknown;
inline std::string RuneSchemaVersion;
inline std::string GameVersion;
inline nlohmann::json UE4SS=nlohmann::json::object();
inline nlohmann::json Steam=nlohmann::json::object();
inline bool UpdateChecksAvailable=false;
inline bool NativeIconCaptureAvailable=true;
inline Kind Decode(const nlohmann::json& value) {
    if(value.value("DedicatedServer",false))return Kind::Dedicated;
    const auto lane=value.value("Storefront",std::string{});
    if(lane=="Game Pass/WinGDK")return Kind::GamePass;
    // Retain the old host wire label as a compatibility alias only.
    if(lane=="Steam/Epic"||lane=="Steam/GOG") {
        const auto platform=value.value("Platform",std::string{});
        if(platform=="Epic Games")return Kind::Epic;
        if(platform=="Steam")return Kind::Steam;
        // An initialized Steamworks build ID can identify a custom Steam
        // install whose directory does not contain a launcher name.
        if(value.contains("Steam")&&value["Steam"].is_object()) {
            const auto& steam=value["Steam"];
            if(steam.contains("BuildId")&&steam["BuildId"].is_number_integer()&&steam["BuildId"]>0)return Kind::Steam;
        }
        return Kind::SteamEpic;
    }
    return Kind::Unknown;
}
inline bool Server(){return Current==Kind::Dedicated;}
inline bool SharedStoreBadge(){return Current==Kind::SteamEpic;}
// Publish once, before starting the UI thread. Never infer versions from a
// package filename or a stale log from another storefront/session.
inline void Configure(const nlohmann::json& value) {
    Current=Decode(value);
    // Desktop D3D/WARP rendering is independent of the game's RHI. Dedicated
    // servers can display imported pixels without polling a headless game GPU.
    NativeIconCaptureAvailable=!Server()&&value.value("NativeIconCaptureAvailable",true);
    RuneSchemaVersion=value.contains("RuneSchemaVersion")&&value["RuneSchemaVersion"].is_string()?value["RuneSchemaVersion"].get<std::string>():std::string{};
    GameVersion=value.contains("GameVersion")&&value["GameVersion"].is_string()?value["GameVersion"].get<std::string>():std::string{};
    UE4SS=value.contains("UE4SS")&&value["UE4SS"].is_object()?value["UE4SS"]:nlohmann::json::object();
    Steam=Current==Kind::Steam&&value.contains("Steam")&&value["Steam"].is_object()?value["Steam"]:nlohmann::json::object();
    UpdateChecksAvailable=value.value("UpdateChecksAvailable",false);
}
inline const char* Label(){switch(Current){case Kind::Steam:return "Steam";case Kind::Epic:return "Epic Games";
    case Kind::SteamEpic:return "Steam / Epic";case Kind::GamePass:return "Game Pass";case Kind::Dedicated:return "Dedicated server";default:return "Runtime";}}
}
