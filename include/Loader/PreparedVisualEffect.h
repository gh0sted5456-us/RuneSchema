#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

namespace DragonWilds {
struct PreparedVisualEffect {
    nlohmann::json value;
    std::string key;

    PreparedVisualEffect() = default;
    explicit PreparedVisualEffect(nlohmann::json effect):value(std::move(effect)) {
        auto style=value;
        if(style.is_object())style.erase("Target");
        key=style.dump();
    }
};
}
