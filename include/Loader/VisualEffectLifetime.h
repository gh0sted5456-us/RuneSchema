#pragma once
#include <nlohmann/json.hpp>
#include <cmath>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>

namespace DragonWilds {
struct VisualEffectLifetime {
    std::string Trigger;
    double DurationSeconds = 0.0;
    [[nodiscard]] bool IsFinite() const { return DurationSeconds > 0.0; }
};

inline VisualEffectLifetime ParseVisualEffectLifetime(const nlohmann::json& value,
    std::string defaultTrigger, std::initializer_list<const char*> supportedTriggers)
{
    VisualEffectLifetime result{std::move(defaultTrigger), 0.0};
    if (value.contains("Trigger")) {
        if (!value.at("Trigger").is_string())
            throw std::runtime_error("VisualEffect.Trigger must be a string");
        result.Trigger = value.at("Trigger").get<std::string>();
    }
    bool supported = false;
    for (const auto* trigger : supportedTriggers)
        if (result.Trigger == trigger) { supported = true; break; }
    if (!supported)
        throw std::runtime_error("VisualEffect.Trigger is unsupported by this loader");
    if (!value.contains("DurationSeconds")) return result;
    const auto& duration = value.at("DurationSeconds");
    if (duration.is_string()) {
        if (duration.get<std::string>() != "INFINITE")
            throw std::runtime_error("VisualEffect.DurationSeconds must be a number or 'INFINITE'");
        return result;
    }
    if (!duration.is_number())
        throw std::runtime_error("VisualEffect.DurationSeconds must be a number or 'INFINITE'");
    result.DurationSeconds = duration.get<double>();
    if (!std::isfinite(result.DurationSeconds) || result.DurationSeconds <= 0.0
        || result.DurationSeconds > 300.0)
        throw std::runtime_error("VisualEffect.DurationSeconds must be greater than 0 and at most 300");
    return result;
}

inline nlohmann::json VisualEffectStyle(nlohmann::json value) {
    value.erase("Trigger");
    value.erase("DurationSeconds");
    return value;
}
}
