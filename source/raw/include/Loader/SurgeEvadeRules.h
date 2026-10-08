#pragma once
#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include <stdexcept>

namespace DragonWilds::SurgeEvadeRules {
using Rules = std::map<std::string, bool>;
inline void Merge(Rules& current, const nlohmann::json& document) {
    if (!document.is_object() || document.size() != 1 || !document.contains("SurgeEvadeLegs")
        || !document["SurgeEvadeLegs"].is_object() || document["SurgeEvadeLegs"].size() > 64)
        throw std::runtime_error("Expected SurgeEvadeLegs object with at most 64 exact wearable asset paths and boolean values");
    auto staged = current;
    for (const auto& [path, enabled] : document["SurgeEvadeLegs"].items()) {
        if (!path.starts_with("/Game/") || path.size() > 512 || path.find('.') == std::string::npos
            || path.find_first_of("*?:\\\r\n\t") != std::string::npos || !enabled.is_boolean())
            throw std::runtime_error("SurgeEvadeLegs requires exact /Game/Package.Asset paths and boolean values");
        for (unsigned char c : path) if (c < 32) throw std::runtime_error("Invalid control character in wearable path");
        if (enabled.get<bool>()) staged[path] = true;
        else staged.erase(path);
    }
    if (staged.size() > 64) throw std::runtime_error("At most 64 Surge evade wearables may be enabled");
    current.swap(staged);
}
}
