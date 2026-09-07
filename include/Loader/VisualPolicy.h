#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace DragonWilds {
inline void ValidateVisualLayers(const nlohmann::json& effect) {
    for (const char* field : {"Overlay", "BodyMaterial"})
        if (effect.contains(field) && !effect.at(field).is_boolean())
            throw std::runtime_error(std::string("VisualEffect.") + field + " must be a boolean");
    if (!effect.value("Overlay", true) && !effect.value("BodyMaterial", false))
        throw std::runtime_error("VisualEffect must enable Overlay or BodyMaterial");
}
}
