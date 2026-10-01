#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace PS::AppearanceDefaults {
const nlohmann::json& BuiltIn();
bool ReadField(const nlohmann::json& document, const std::string& field,
    std::string& dataTablePath, std::string& rowName, std::string& error);
}
