#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace PS::PlayerTrace {
inline void ValidateOptions(const nlohmann::json& options) {
    if(!options.is_object())throw std::runtime_error("Player Trace options must be an object.");
    for(const auto& [key,value]:options.items()) {
        if(key=="Seconds" || key=="MaxEvents" || key=="Categories") {
            const auto maximum=key=="Seconds"?60:key=="MaxEvents"?4096:31;
            if(!value.is_number_integer() || value<1 || value>maximum)throw std::runtime_error("Player Trace option out of range: "+key);
        } else if(key=="Filter") {
            if(!value.is_string() || value.get_ref<const std::string&>().size()>128)throw std::runtime_error("Player Trace filter must be 0-128 characters.");
        } else if(key=="SuppressTicks") {
            if(!value.is_boolean())throw std::runtime_error("SuppressTicks must be boolean.");
        } else throw std::runtime_error("Unknown Player Trace option: "+key);
    }
}
}
