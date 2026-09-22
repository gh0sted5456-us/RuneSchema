#pragma once
#include <nlohmann/json.hpp>
#include <cmath>
#include <string>

namespace PS::SpawnAuthoring {
inline std::string GroundToken(double offset) {
    if(!std::isfinite(offset)||std::abs(offset)>100000)throw std::runtime_error("Spawn ground offset is invalid");
    if(offset==0)return "$";
    auto value=std::to_string(offset);
    while(value.size()>1&&value.back()=='0')value.pop_back();
    if(value.back()=='.')value.pop_back();
    return std::string("$")+(offset>0?"+":"")+value;
}
inline nlohmann::json StageGroundDraft(nlohmann::json placement,double resolvedZ,double offset) {
    if(!placement.is_object()||!placement.contains("Location")||!placement["Location"].is_object()||!std::isfinite(resolvedZ))
        throw std::runtime_error("Spawn placement cannot be staged for ground export");
    placement["Location"]["Z"]=GroundToken(offset);
    placement["$z"]=resolvedZ;
    placement.erase("Grid");
    return placement;
}
inline nlohmann::json ResolveGroundDrafts(const nlohmann::json& drafts) {
    if(!drafts.is_array())throw std::runtime_error("Spawn drafts must be an array");
    auto result=drafts;
    for(auto& placement:result) {
        if(!placement.is_object()||!placement.contains("Location")||!placement["Location"].is_object()
            ||!placement.contains("$z")||!placement["$z"].is_number())throw std::runtime_error("Spawn draft has no resolved $z value");
        const auto z=placement["$z"].get<double>();
        if(!std::isfinite(z))throw std::runtime_error("Spawn draft $z value is invalid");
        placement["Location"]["Z"]=z;
        placement.erase("$z");
    }
    return result;
}
}
