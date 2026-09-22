#pragma once
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <string>
#include <stdexcept>
namespace RC::Unreal { class UClass; }
namespace DragonWilds::DefinitionRegistry {
inline std::unordered_map<std::string,RC::Unreal::UClass*> Effects;
inline std::unordered_map<std::string,nlohmann::json> Niagara;
inline nlohmann::json Visual(const nlohmann::json& input) {
    if(!input.is_object() || !input.contains("Definition"))return input;
    const auto id=input.at("Definition").get<std::string>();
    const auto found=Niagara.find(id);
    if(found==Niagara.end())throw std::runtime_error("Unknown Niagara definition: "+id);
    auto value=found->second;
    auto overrides=input;
    overrides.erase("Definition");
    value.merge_patch(overrides);
    return value;
}
}
