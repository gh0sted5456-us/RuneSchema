#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace PS::WearableBridgeDirective {
inline bool Requested(const nlohmann::json& fields){
    const auto found=fields.find("$WearableBridge");if(found==fields.end())return false;
    if(!found->is_boolean())throw std::runtime_error("$WearableBridge must be true or false");
    return found->get<bool>();
}
}
