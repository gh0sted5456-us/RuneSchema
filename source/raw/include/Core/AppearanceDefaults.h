#pragma once

#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

namespace PS::AppearanceDefaults {
struct Selection {
    nlohmann::json Document;
    bool External = false;
    std::string Notice;
};

const nlohmann::json& BuiltIn();
std::filesystem::path OverridePath();
Selection Load(bool allowExternal);
bool ReadField(const nlohmann::json& document, const std::string& field,
    std::string& dataTablePath, std::string& rowName, std::string& error);
}
