#pragma once
#include "Loader/VirtualDefinitionId.h"
#include <nlohmann/json.hpp>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace DragonWilds::EquipmentEffectRules {
enum class Mode { Replace, Append, Clear };
struct Assignment {
    Mode mode=Mode::Replace;
    std::vector<std::string> effects;
    bool operator==(const Assignment&) const = default;
};
using Rules=std::map<std::string,Assignment>;

inline void Merge(Rules& current,const nlohmann::json& value) {
    if(!value.is_object() || value.size()>256)throw std::runtime_error("GrantedEffects requires an object map with at most 256 item paths");
    auto staged=current;
    for(const auto& [path,body]:value.items()) {
        if(!path.starts_with('/') || path.find('.')==std::string::npos || path.size()>1024)
            throw std::runtime_error("GrantedEffects item key must be a cooked object path");
        if(!body.is_object())throw std::runtime_error("GrantedEffects item rule must be an object");
        for(const auto& [field,unused]:body.items())if(field!="Mode" && field!="Effects" && field!="$Comment")
            throw std::runtime_error("Unsupported GrantedEffects field: "+field);
        if(!body.contains("Mode") || !body.at("Mode").is_string())throw std::runtime_error("GrantedEffects rule requires Mode");
        const auto modeText=body.at("Mode").get<std::string>();
        Assignment next;
        if(modeText=="Replace")next.mode=Mode::Replace;
        else if(modeText=="Append")next.mode=Mode::Append;
        else if(modeText=="Clear")next.mode=Mode::Clear;
        else throw std::runtime_error("GrantedEffects Mode must be Replace, Append, or Clear");
        if(next.mode==Mode::Clear) {
            if(body.contains("Effects") && (!body.at("Effects").is_array() || !body.at("Effects").empty()))
                throw std::runtime_error("GrantedEffects Clear cannot contain effects");
        } else {
            if(!body.contains("Effects") || !body.at("Effects").is_array() || body.at("Effects").empty() || body.at("Effects").size()>64)
                throw std::runtime_error("GrantedEffects Replace/Append requires 1..64 Effects");
            for(const auto& effect:body.at("Effects")) {
                if(!effect.is_string())throw std::runtime_error("GrantedEffects effect references must be strings");
                const auto reference=effect.get<std::string>();
                const bool cooked=reference.starts_with('/') && reference.find('.')!=std::string::npos && reference.ends_with("_C");
                if(!cooked && !VirtualDefinitionId::IsCanonical(reference))
                    throw std::runtime_error("GrantedEffects reference must be Mod:Effects/Id or a cooked class path ending in _C");
                next.effects.push_back(reference);
            }
        }
        staged[path]=std::move(next);
    }
    current=std::move(staged);
}
}
