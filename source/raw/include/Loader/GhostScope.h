#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace DragonWilds {
inline bool PlayerMeshOnly(const nlohmann::json& effect) {
    if(!effect.contains("Target"))return false;
    const auto& target=effect.at("Target");
    if(!target.is_string() || (target!="EntirePerson" && target!="PlayerMesh"))
        throw std::runtime_error("VisualEffect.Target must be EntirePerson or PlayerMesh");
    return target=="PlayerMesh";
}
}
