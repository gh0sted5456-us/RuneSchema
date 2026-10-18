#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
namespace PS::MapTeleportRisk {
enum class Level { Unknown,Lower,Higher };
inline Level Read(const nlohmann::json& observation,int64_t now){
    try{if(!observation.at("ObservedUnixMs").is_number_integer())return Level::Unknown;const int64_t at=observation.at("ObservedUnixMs");
        if(at<=0||at>now||now-at>10000||!observation.value("LandingFootprintChecked",false))return Level::Unknown;
        const auto value=observation.value("LandingRisk",std::string{});return value=="lower"?Level::Lower:value=="higher"?Level::Higher:Level::Unknown;
    }catch(...){return Level::Unknown;}
}
}
