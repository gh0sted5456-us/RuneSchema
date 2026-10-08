#pragma once
#include "Loader/VisualPolicy.h"
#include <cmath>
#include <string_view>

namespace DragonWilds::NpcVisualEffect {
inline void Validate(const nlohmann::json& effect) {
    if(!effect.is_object())throw std::runtime_error("NPC VisualEffect requires an object");
    if(effect.value("Type",std::string{})=="Niagara") {
        for(const auto& [key,value]:effect.items())if(key!="Type" && key!="System" && key!="Parameters" && key!="LocationOffset")
            throw std::runtime_error("NPC Niagara supports Type, System, Parameters and LocationOffset");
        if(!effect.contains("System") || !effect.at("System").is_string())throw std::runtime_error("NPC Niagara requires System");
        const auto path=effect.at("System").get<std::string>();
        if(path.empty() || path.size()>1024 || path.front()!='/' || path.find('.')==path.npos || path.find_first_of("\r\n\t")!=path.npos)
            throw std::runtime_error("NPC Niagara System requires a cooked object path");
        const auto numeric=[](const nlohmann::json& value) {
            return value.is_number() && std::isfinite(value.get<double>()) && std::abs(value.get<double>())<=100000.0;
        };
        if(effect.contains("LocationOffset")) {
            const auto& offset=effect.at("LocationOffset");
            if(!offset.is_object() || offset.size()!=3)throw std::runtime_error("Niagara LocationOffset requires X, Y and Z");
            for(const auto* key:{"X","Y","Z"})if(!offset.contains(key) || !numeric(offset.at(key)))throw std::runtime_error("Invalid Niagara LocationOffset");
        }
        if(effect.contains("Parameters")) {
            const auto& parameters=effect.at("Parameters");
            if(!parameters.is_object() || parameters.size()>32)throw std::runtime_error("Niagara Parameters requires at most 32 entries");
            for(const auto& [name,value]:parameters.items()) {
                if(!name.starts_with("User.") || name.size()>128 || name.find_first_of("\r\n\t")!=name.npos)throw std::runtime_error("Niagara parameter requires a User. name");
                if(value.is_boolean() || numeric(value))continue;
                if(!value.is_object())throw std::runtime_error("Invalid Niagara parameter value");
                if(value.contains("R")) {
                    if(value.size()!=4)throw std::runtime_error("Niagara color requires RGBA");
                    for(const auto* key:{"R","G","B","A"})if(!value.contains(key) || !numeric(value.at(key)))throw std::runtime_error("Invalid Niagara color");
                } else {
                    if(value.size()!=3)throw std::runtime_error("Niagara vector requires XYZ");
                    for(const auto* key:{"X","Y","Z"})if(!value.contains(key) || !numeric(value.at(key)))throw std::runtime_error("Invalid Niagara vector");
                }
            }
        }
        return;
    }
    for(const auto& [key,value]:effect.items())
        if(key!="Type" && key!="Overlay" && key!="BodyMaterial" && key!="MainColor" && key!="SecondaryColor")
            throw std::runtime_error("Unsupported NPC VisualEffect field: "+key);
    if(!effect.contains("Type") || effect.at("Type")!="Ghost Glow")
        throw std::runtime_error("NPC VisualEffect.Type must be Ghost Glow or Niagara");
    ValidateVisualLayers(effect);
    for(const auto* field:{"MainColor","SecondaryColor"}) {
        if(!effect.contains(field))continue;
        const auto& color=effect.at(field);
        if(!color.is_object() || color.size()!=4)throw std::runtime_error("Ghost Glow colors require R, G, B and A");
        for(const auto* channel:{"R","G","B","A"}) {
            if(!color.contains(channel) || !color.at(channel).is_number())throw std::runtime_error("Ghost Glow color channels must be numbers");
            const auto value=color.at(channel).get<double>();
            if(!std::isfinite(value) || value<0 || value>(std::string_view(channel)=="A"?1.0:64.0))
                throw std::runtime_error("Ghost Glow RGB must be 0..64 and alpha 0..1");
        }
    }
}
}
