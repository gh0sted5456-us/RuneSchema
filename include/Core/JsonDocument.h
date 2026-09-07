#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include "nlohmann/json_fwd.hpp"

namespace PS::JsonHelpers {
    bool FieldExists(const nlohmann::json& data, const std::string& fieldName);
    void ValidateFieldExists(const nlohmann::json& data, const std::string& fieldName);
    void ParseDouble(const nlohmann::json& value, const std::string& fieldName, double& outValue);
    void ParseInteger(const nlohmann::json& value, const std::string& fieldName, int& outValue);
    void ParseString(const nlohmann::json& value, const std::string& fieldName, std::string& outValue);
    void ParseJsonFileInPath(const std::filesystem::path& path, const std::function<void(const nlohmann::json&)>& callback);
    void ParseJsonFilesInPath(const std::filesystem::path& path, const std::function<void(const nlohmann::json&)>& callback);
}
