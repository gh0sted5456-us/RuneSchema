#include "Generator/FModelSnippetGenerator.h"

#include <filesystem>
#include <fstream>
#include <unordered_set>

#include "Helpers/String.hpp"
#include "UE4SSProgram.hpp"
#include "nlohmann/json.hpp"
#include "Utility/Logging.h"
#include "Utility/ObjectPath.h"

namespace fs = std::filesystem;

namespace {
    const std::unordered_set<std::string> UnsafeProperties = {
        "PersistenceID", "InternalName", "RootComponent", "UberGraphFrame",
        "BlueprintCreatedComponents", "InstanceComponents",
        "SimpleConstructionScript", "UberGraphFunction"
    };

    bool IsUnsafe(const std::string& name)
    {
        return UnsafeProperties.contains(name)
            || name.ends_with("Guid") || name.ends_with("GUID");
    }

    nlohmann::ordered_json SanitizeValue(const nlohmann::json& value)
    {
        if (value.is_object())
        {
            auto result = nlohmann::ordered_json::object();
            for (const auto& [name, child] : value.items())
            {
                if (!IsUnsafe(name)) result[name] = SanitizeValue(child);
            }
            return result;
        }
        if (value.is_array())
        {
            auto result = nlohmann::ordered_json::array();
            for (const auto& child : value) result.push_back(SanitizeValue(child));
            return result;
        }
        return value;
    }

    nlohmann::ordered_json SafeProperties(const nlohmann::json& properties)
    {
        nlohmann::ordered_json result = nlohmann::ordered_json::object();
        if (!properties.is_object()) return result;

        for (const auto& [name, value] : properties.items())
        {
            if (!IsUnsafe(name)) result[name] = SanitizeValue(value);
        }
        return result;
    }

    std::string ExtractObjectName(const nlohmann::json& reference)
    {
        if (!reference.is_object() || !reference.contains("ObjectName")
            || !reference.at("ObjectName").is_string()) return {};

        auto value = reference.at("ObjectName").get<std::string>();
        auto firstQuote = value.find('\'');
        auto lastQuote = value.find_last_of('\'');
        return firstQuote != std::string::npos && lastQuote > firstQuote
            ? value.substr(firstQuote + 1, lastQuote - firstQuote - 1) : value;
    }

    bool GenerateSnippet(const fs::path& inputPath, const fs::path& outputPath)
    {
        std::ifstream input(inputPath);
        auto exports = nlohmann::json::parse(input, nullptr, true, true);
        if (!exports.is_array()) return false;

        const nlohmann::json* blueprintClass = nullptr;
        for (const auto& entry : exports)
        {
            if (entry.is_object() && entry.value("Type", "") == "BlueprintGeneratedClass")
            {
                blueprintClass = &entry;
                break;
            }
        }

        nlohmann::ordered_json snippet = nlohmann::ordered_json::object();
        snippet["$generated"] = "Review and remove unwanted properties before moving this file into a mod loader folder.";

        if (blueprintClass)
        {
            auto className = blueprintClass->value("Name", "");
            auto cdoName = ExtractObjectName(blueprintClass->value("ClassDefaultObject", nlohmann::json::object()));
            const nlohmann::json* cdo = nullptr;
            for (const auto& entry : exports)
            {
                if (entry.is_object() && entry.value("Name", "") == cdoName)
                {
                    cdo = &entry;
                    break;
                }
            }
            if (className.empty() || !cdo || !cdo->contains("Properties")) return false;

            auto properties = SafeProperties(cdo->at("Properties"));
            for (const auto& entry : exports)
            {
                if (!entry.is_object() || !entry.contains("Outer") || !entry.contains("Properties")) continue;
                if (ExtractObjectName(entry.at("Outer")) == cdoName)
                {
                    auto componentName = entry.value("Name", "");
                    if (!componentName.empty() && !IsUnsafe(componentName))
                    {
                        properties[componentName] = SafeProperties(entry.at("Properties"));
                    }
                }
            }
            snippet[className] = std::move(properties);
        }
        else
        {
            const nlohmann::json* asset = nullptr;
            for (const auto& entry : exports)
            {
                if (entry.is_object() && entry.contains("Package") && entry.contains("Properties")
                    && entry.at("Package").is_string() && entry.at("Properties").is_object())
                {
                    asset = &entry;
                    break;
                }
            }
            if (!asset) return false;

            auto package = RC::to_generic_string(asset->at("Package").get<std::string>());
            auto name = RC::to_generic_string(asset->value("Name", ""));
            auto target = PS::ObjectPath::Normalize(package, name);
            snippet[RC::to_string(target)] = SafeProperties(asset->at("Properties"));
        }

        std::ofstream output(outputPath, std::ios::trunc);
        output << snippet.dump(2) << '\n';
        return output.good();
    }
}

namespace PS::FModelSnippetGenerator {
    void GenerateConfiguredInputs()
    {
        auto configPath = fs::path(UE4SSProgram::get_program().get_working_directory())
            / "Mods" / "RuneSchema" / "config";
        auto inputPath = configPath / "fmodel-input";
        auto outputPath = configPath / "fmodel-snippets";
        fs::create_directories(inputPath);
        fs::create_directories(outputPath);

        std::size_t generated = 0;
        for (const auto& entry : fs::directory_iterator(inputPath))
        {
            auto extension = entry.path().extension().string();
            if (!entry.is_regular_file() || (extension != ".json" && extension != ".jsonc")) continue;

            try
            {
                auto destination = outputPath / (entry.path().stem().string() + ".runeschema.json");
                if (GenerateSnippet(entry.path(), destination)) ++generated;
                else PS::Log<RC::LogLevel::Warning>(STR("FModel snippet generator could not identify an editable asset in '{}'.\n"), entry.path().native());
            }
            catch (const std::exception& e)
            {
                PS::Log<RC::LogLevel::Warning>(STR("FModel snippet generation failed for '{}': {}\n"),
                    entry.path().native(), PS::ToWideSafe(e.what()));
            }
        }

        PS::Log<RC::LogLevel::Normal>(STR("FModel snippet generator wrote {} snippet(s).\n"), generated);
    }
}
