#pragma once
#include <nlohmann/json.hpp>
namespace PS::HelpyEnvironment {
enum class Kind { Unknown, Steam, GamePass, Dedicated };
inline Kind Current=Kind::Unknown;
inline std::string RuneSchemaVersion;
inline std::string GameVersion;
inline Kind Decode(const nlohmann::json& value) {
    if(value.value("DedicatedServer",false))return Kind::Dedicated;
    const auto lane=value.value("Storefront",std::string{});
    return lane=="Steam/GOG"?Kind::Steam:lane=="Game Pass/WinGDK"?Kind::GamePass:Kind::Unknown;
}
inline bool Server(){return Current==Kind::Dedicated;}
// Publish once, before starting the UI thread. Never infer versions from a
// package filename or a stale log from another storefront/session.
inline void Configure(const nlohmann::json& value) {
    Current=Decode(value);
    RuneSchemaVersion=value.contains("RuneSchemaVersion")&&value["RuneSchemaVersion"].is_string()?value["RuneSchemaVersion"].get<std::string>():std::string{};
    GameVersion=value.contains("GameVersion")&&value["GameVersion"].is_string()?value["GameVersion"].get<std::string>():std::string{};
}
inline const char* Label(){switch(Current){case Kind::Steam:return "Steam / GOG";case Kind::GamePass:return "Game Pass";case Kind::Dedicated:return "Dedicated server";default:return "Runtime";}}
}
