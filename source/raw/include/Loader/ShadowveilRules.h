#pragma once
#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include <stdexcept>
#include <cstdint>

namespace DragonWilds::ShadowveilRules {
using ActionMask = uint32_t;
inline constexpr const char* ActionNames[]{"MeleeAttack", "RangedAttack", "Evade", "MagicAttack", "UtilityCast"};
inline constexpr ActionMask LegacyActions = 7;
using Rules = std::map<std::string, ActionMask>;
inline void Merge(Rules& current, const nlohmann::json& document) {
    if (!document.is_object() || document.size() != 1)
        throw std::runtime_error("Expected one Shadowveil wearables rule object");
    const bool legacy = document.contains("ShadowveilAttackEvadeWearables");
    const char* key = legacy ? "ShadowveilAttackEvadeWearables" : "ShadowveilWearables";
    if (!document.contains(key) || !document[key].is_object() || document[key].size() > 64)
        throw std::runtime_error("Expected ShadowveilWearables object with at most 64 exact wearable paths");
    auto staged = current;
    for (const auto& [path, value] : document[key].items()) {
        if (!path.starts_with("/Game/") || path.size() > 512 || path.find('.') == std::string::npos
            || path.find_first_of("*?:\\\r\n\t") != std::string::npos)
            throw std::runtime_error("Shadowveil requires exact /Game/Package.Asset paths");
        for (unsigned char c : path) if (c < 32) throw std::runtime_error("Invalid control character in wearable path");
        ActionMask mask = 0;
        if (legacy) {
            if (!value.is_boolean()) throw std::runtime_error("Legacy Shadowveil rules require boolean values");
            if (value.get<bool>()) mask = LegacyActions;
        } else if (value != false) {
            if (!value.is_object() || value.size() != 1 || !value.contains("PreserveOn")
                || !value["PreserveOn"].is_array() || value["PreserveOn"].size() > std::size(ActionNames))
                throw std::runtime_error("ShadowveilWearables values must be false or {PreserveOn: [action names]}");
            for (const auto& action : value["PreserveOn"]) {
                bool found = false;
                for (unsigned i = 0; i < std::size(ActionNames); ++i) {
                    if (action != ActionNames[i]) continue;
                    if (mask & (1u << i)) throw std::runtime_error("Duplicate Shadowveil action");
                    mask |= 1u << i;
                    found = true;
                    break;
                }
                if (!found) throw std::runtime_error("Unknown Shadowveil action");
            }
        }
        if (mask) staged[path] = mask;
        else staged.erase(path);
    }
    if (staged.size() > 64) throw std::runtime_error("At most 64 Shadowveil wearables may be enabled");
    current.swap(staged);
}
}
