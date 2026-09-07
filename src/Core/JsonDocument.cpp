#include "Core/JsonDocument.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <fstream>
#include <format>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;
namespace PS::JsonHelpers {
    bool FieldExists(const nlohmann::json& data, const std::string& fieldName)
    {
        return data.contains(fieldName);
    }

    void ValidateFieldExists(const nlohmann::json& data, const std::string& fieldName)
    {
        if (!data.contains(fieldName))
        {
            throw std::runtime_error(std::format("Missing a required field of '{}'.", fieldName));
        }
    }

    void ParseDouble(const nlohmann::json& value, const std::string& fieldName, double& outValue)
    {
        auto& field = value.at(fieldName);

        if (!field.is_number())
        {
            throw std::runtime_error(std::format("Value '{}' must be a number.", fieldName));
        }

        outValue = field.get<double>();
    }

    void ParseInteger(const nlohmann::json& value, const std::string& fieldName, int& outValue)
    {
        auto& field = value.at(fieldName);

        if (!field.is_number_integer())
        {
            throw std::runtime_error(std::format("Value '{}' must be an integer.", fieldName));
        }

        outValue = field.get<int>();
    }

    void ParseString(const nlohmann::json& value, const std::string& fieldName, std::string& outValue)
    {
        auto& field = value.at(fieldName);

        if (!field.is_string())
        {
            throw std::runtime_error(std::format("Value '{}' must be a string.", fieldName));
        }

        outValue = field.get<std::string>();
    }

    void ParseJsonFileInPath(const std::filesystem::path& path, const std::function<void(const nlohmann::json&)>& callback)
    {
        if (!fs::exists(path))
        {
            return;
        }

        if (path.extension() != ".json" && path.extension() != ".jsonc")
        {
            return;
        }

        std::ifstream f(path);

        nlohmann::json data = nlohmann::json::parse(f, nullptr, true, true);
        callback(data);
    }

    void ParseJsonFilesInPath(const std::filesystem::path& path, const std::function<void(const nlohmann::json&)>& callback)
    {
        if (!fs::is_directory(path))
        {
            return;
        }

        std::vector<fs::path> files;
        for (const auto& file : fs::directory_iterator(path))
            if (file.is_regular_file() && file.path().has_extension()) files.push_back(file.path());
        std::sort(files.begin(), files.end());
        for (const auto& filePath : files)
        {
            try
            {
                ParseJsonFileInPath(filePath, callback);
            }
            catch (const std::exception& e)
            {
                throw std::runtime_error(std::format("Failed parsing mod file {} - {}.\n", filePath.string(), e.what()));
            }
        }
    }
}
