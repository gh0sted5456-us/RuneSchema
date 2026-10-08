#pragma once
#include <nlohmann/json.hpp>
#include <cmath>
#include <stdexcept>
#include <string>
namespace DragonWilds::NpcMarkers {
inline constexpr const char* DefaultIcon="/Game/Art/UI/Map/T_Map_Secondary_Quest_Icon_NPC.T_Map_Secondary_Quest_Icon_NPC";
inline void Validate(const nlohmann::json& data) {
    for(const auto* group:{"Map","OverheadIcon"}) {
        if(!data.contains(group))continue;
        const auto& value=data.at(group);
        if(!value.is_object())throw std::runtime_error(std::string(group)+" must be an object");
        for(const auto& [key,entry]:value.items()) {
            if(key=="Enabled" || (std::string(group)=="Map" && key=="ShowName")) {if(!entry.is_boolean())throw std::runtime_error("Marker "+key+" must be boolean");}
            else if(key=="Icon") {
                const auto path=entry.get<std::string>();
                if(path.size()<2 || path.size()>1024 || path.front()!='/' || path.find_first_of("\r\n\t")!=path.npos || path.find('\0')!=path.npos)
                    throw std::runtime_error("Marker Icon requires a texture asset path");
            } else if(key=="SizeMode") {
                if(!entry.is_string())throw std::runtime_error("Marker SizeMode must be a string");
                if(std::string(group)=="Map" && entry!="Pixels" && entry!="WorldUnits")
                    throw std::runtime_error("Map SizeMode must be Pixels or WorldUnits");
                if(std::string(group)=="OverheadIcon" && entry!="Scale" && entry!="Pixels")
                    throw std::runtime_error("OverheadIcon SizeMode must be Scale or Pixels");
            } else if(key=="Size") {
                if(!entry.is_number())throw std::runtime_error("Marker Size must be numeric");
                const double v=entry.get<double>();
                if(!std::isfinite(v) || v<1 || v>4096)throw std::runtime_error("Marker Size must be between 1 and 4096");
            } else if(std::string(group)=="OverheadIcon" && (key=="Height" || key=="Scale" || key=="Distance")) {
                if(!entry.is_number())throw std::runtime_error("Marker dimensions must be numeric");
                const double v=entry.get<double>();
                const double max=key=="Height"?2000:key=="Scale"?5:100000;
                const double min=key=="Height"?0:key=="Scale"?0.01:100;
                if(!std::isfinite(v) || v<min || v>max)throw std::runtime_error("Marker dimension out of range");
            } else throw std::runtime_error("Unsupported marker field: "+key);
        }
    }
}
inline std::string MapLabel(const nlohmann::json& options,const std::string& displayName) {
    return options.value("ShowName",true)?displayName:std::string{};
}
inline nlohmann::json MapSizeUnit(const std::string& mode) {
    return mode=="WorldUnits" ? nlohmann::json("WorldSpace") : nlohmann::json("ScreenSpace");
}
}
