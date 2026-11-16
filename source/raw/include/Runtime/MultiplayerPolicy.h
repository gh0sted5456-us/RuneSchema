#pragma once

#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <nlohmann/json.hpp>

namespace PS::MultiplayerPolicy {
inline constexpr const char* Service = "runeschema.multiplayer.policy";

inline bool ActorClass(std::string_view path) {
    if (!path.starts_with('/') || (!path.starts_with("/Script/") && !path.ends_with("_C"))) return false;
    const auto dot = path.find('.');
    if (dot == path.npos || dot <= path.rfind('/') + 1 || path.find('.', dot + 1) != path.npos) return false;
    for (const auto ch : path) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')
            || ch == '_' || ch == '-' || ch == '/' || ch == '.') continue;
        return false;
    }
    return path.find("//") == path.npos;
}

struct Rule {
    std::optional<float> DistanceSquared;
    std::optional<float> Frequency;
};

inline Rule Parse(const nlohmann::json& value) {
    if (!value.is_object() || value.empty()) throw std::runtime_error("Enter a range or update rate.");
    Rule rule;
    for (const auto& [key, field] : value.items()) {
        if ((key != "RangeMeters" && key != "UpdatesPerSecond") || !field.is_number())
            throw std::runtime_error("Unsupported replication setting.");
        auto requested = field.get<double>();
        if (key == "RangeMeters" && requested > 0) {
            const auto centimeters = requested * 100;
            requested = centimeters * centimeters;
        }
        if (!std::isfinite(requested) || requested < std::numeric_limits<float>::min()
            || requested > std::numeric_limits<float>::max())
            throw std::runtime_error("Enter a positive value representable by the engine.");
        if (key == "RangeMeters") rule.DistanceSquared = static_cast<float>(requested);
        else rule.Frequency = static_cast<float>(requested);
    }
    return rule;
}
}
