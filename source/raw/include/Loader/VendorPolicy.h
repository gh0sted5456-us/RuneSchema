#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
namespace DragonWilds::VendorPolicy {
enum class VendorStage { Visual, Interaction, Merchant };
inline VendorStage Stage(const nlohmann::json& data) {
    const auto value = data.value("Stage", std::string("Visual"));
    if (value == "Visual") return VendorStage::Visual;
    if (value == "Interaction") return VendorStage::Interaction;
    if (value == "Merchant") return VendorStage::Merchant;
    throw std::runtime_error("Stage must be Visual, Interaction or Merchant");
}
}
