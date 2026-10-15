#pragma once
#include <nlohmann/json.hpp>
namespace PS::HelpyEnvironment {
enum class Kind { Unknown, Steam, GamePass, Dedicated };
inline Kind Current=Kind::Unknown;
inline Kind Decode(const nlohmann::json& value) {
    if(value.value("DedicatedServer",false))return Kind::Dedicated;
    const auto lane=value.value("Storefront",std::string{});
    return lane=="Steam/GOG"?Kind::Steam:lane=="Game Pass/WinGDK"?Kind::GamePass:Kind::Unknown;
}
inline bool Server(){return Current==Kind::Dedicated;}
inline const char* Label(){switch(Current){case Kind::Steam:return "Steam / GOG";case Kind::GamePass:return "Game Pass";case Kind::Dedicated:return "Dedicated server";default:return "Runtime";}}
}
