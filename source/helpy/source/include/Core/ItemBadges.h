#pragma once
#include <cmath>
#include <optional>
#include <nlohmann/json.hpp>
namespace PS::ItemBadges {
inline std::optional<double> Power(const nlohmann::json& row) {
    const auto value=row.find("PowerLevel");if(value==row.end()||!value->is_number())return {};
    const auto power=value->get<double>();return std::isfinite(power)&&power>=0?std::optional<double>(power):std::nullopt;
}
inline bool Masterwork(const nlohmann::json& row) {
    const auto value=row.find("Masterwork");return value!=row.end()&&value->is_boolean()&&value->get<bool>();
}
}
