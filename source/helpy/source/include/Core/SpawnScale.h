#pragma once
#include <nlohmann/json.hpp>
#include <cmath>
#include <optional>
namespace PS::SpawnScale {
inline constexpr double ReferenceCm=180.; // Explicit reference, not a claim about the current player.
inline std::optional<double> Height(const nlohmann::json& row){
    const auto found=row.find("PreviewHeightCm");if(found==row.end()||!found->is_number())return {};
    const auto height=found->get<double>();if(!std::isfinite(height)||height<=1||height>=100000)return {};return height;
}
inline double Ratio(const nlohmann::json& row,double scale){if(!std::isfinite(scale)||scale<=0)return 1;return Height(row).value_or(ReferenceCm)*scale/ReferenceCm;}
}
