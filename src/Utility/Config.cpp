#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Helpers/String.hpp"
#include <cmath>
#include <fstream>
#include "UE4SSProgram.hpp"

namespace fs = std::filesystem;

namespace PS {
    namespace {
        void EnsureConfigWritable(const fs::path& configFile)
        {
            if (!fs::exists(configFile))
            {
                return;
            }

            std::error_code error;
            fs::permissions(configFile, fs::perms::owner_write, fs::perm_options::add, error);
            if (error)
            {
                PS::Log<RC::LogLevel::Warning>(
                    STR("Could not clear the config read-only flag: {}\n"),
                    PS::ToWideSafe(error.message().c_str()));
            }
        }
    }

    std::unique_ptr<PSConfig> s_config;

    PSConfig* PSConfig::Get()
    {
        if (!s_config)
        {
            s_config = std::make_unique<PSConfig>();
        }
        
        return s_config.get();
    }

    std::string PSConfig::GetLanguageOverride()
    {
        auto config = Get();
        return config ? config->m_settings.languageOverride : "";
    }

    bool PSConfig::IsAutoReloadEnabled()
    {
        auto config = Get();
        return config ? config->m_settings.enableAutoReload : false;
    }

    bool PSConfig::IsDebugLoggingEnabled()
    {
        auto config = Get();
        return config ? config->m_settings.enableDebugLogging : false;
    }

    bool PSConfig::IsExperimentalDropScalingEnabled()
    {
        auto config = Get();
        return config ? config->m_settings.enableExperimentalDropScaling : false;
    }

    const IdentityOverrideSettings& PSConfig::GetIdentityOverrideSettings() const
    {
        return m_settings.identityOverrides;
    }

    const SpawnSafetySettings& PSConfig::GetSpawnSafetySettings() const
    {
        return m_settings.spawnSafety;
    }

    bool PSConfig::IsSchemaGenerationEnabled()
    {
        auto config = Get();
        return config && config->m_settings.tooling.enabled
            && config->m_settings.tooling.enableSchemaGeneration;
    }

    bool PSConfig::IsFModelSnippetGeneratorEnabled()
    {
        auto config = Get();
        return config && config->m_settings.tooling.enabled
            && config->m_settings.tooling.enableFModelSnippetGenerator;
    }

    bool PSConfig::IsToolingEnabled() const
    {
        return m_settings.tooling.enabled;
    }

    const ModsTxtSettings& PSConfig::GetModsTxtSettings() const
    {
        return m_settings.tooling.modsTxt;
    }

    const CompatibilityReportSettings& PSConfig::GetCompatibilityReportSettings() const
    {
        return m_settings.tooling.compatibilityReports;
    }

    const SchemaTypeSettings& PSConfig::GetSchemaTypeSettings() const
    {
        return m_settings.tooling.schemaTypes;
    }

    PSConfigSettings& PSConfig::GetSettings()
    {
        return m_settings;
    }

    void PSConfig::Load()
    {
        auto folderPath = GetConfigPath();
        if (!fs::exists(folderPath))
        {
            fs::create_directory(folderPath);
        }

        auto configFile = folderPath / "config.json";
        if (!fs::exists(configFile))
        {
            this->Save();
            PS::Log<RC::LogLevel::Warning>(STR("Config file not found, a new one was generated. Default values will be used.\n"));
            return;
        }

        // Some deployment/synchronization tools mark extracted config files as
        // read-only. Keep RuneSchema's user-editable config writable.
        EnsureConfigWritable(configFile);

        try
        {
            std::ifstream file(configFile);
            auto data = nlohmann::json::parse(file, nullptr, true, true);
            m_rawSettings = data;

            m_settings.languageOverride = data.value("languageOverride", m_settings.languageOverride);
            m_settings.enableAutoReload = data.value("enableAutoReload", m_settings.enableAutoReload);
            m_settings.enableDebugLogging = data.value("enableDebugLogging", m_settings.enableDebugLogging);
            m_settings.enableExperimentalDropScaling = data.value(
                "enableExperimentalDropScaling", m_settings.enableExperimentalDropScaling);

            if (auto identity = data.find("identityOverrides"); identity != data.end() && identity->is_object())
            {
                auto& settings = m_settings.identityOverrides;
                settings.enabled = identity->value("enabled", settings.enabled);
                settings.assets = identity->value("assets", settings.assets);
                settings.recipes = identity->value("recipes", settings.recipes);
                settings.journals = identity->value("journals", settings.journals);
                settings.dryRun = identity->value("dryRun", settings.dryRun);
                settings.logChanges = identity->value("logChanges", settings.logChanges);
            }

            if (auto safety = data.find("spawnSafety"); safety != data.end() && safety->is_object())
            {
                auto& settings = m_settings.spawnSafety;
                settings.maxScale = safety->value("maxScale", settings.maxScale);
                settings.maxDropIncreasePercent = safety->value(
                    "maxDropIncreasePercent", settings.maxDropIncreasePercent);
                if (!std::isfinite(settings.maxScale) || settings.maxScale <= 0.0)
                {
                    settings.maxScale = 10.0;
                    PS::Log<RC::LogLevel::Warning>(STR("spawnSafety.maxScale was invalid; using 10.0.\n"));
                }
                if (!std::isfinite(settings.maxDropIncreasePercent)
                    || settings.maxDropIncreasePercent < 0.0)
                {
                    settings.maxDropIncreasePercent = 500.0;
                    PS::Log<RC::LogLevel::Warning>(
                        STR("spawnSafety.maxDropIncreasePercent was invalid; using 500.0.\n"));
                }
            }

            if (auto tooling = data.find("tooling"); tooling != data.end() && tooling->is_object())
            {
                m_settings.tooling.enabled = tooling->value("enabled", m_settings.tooling.enabled);
                m_settings.tooling.enableSchemaGeneration = tooling->value(
                    "enableSchemaGeneration", m_settings.tooling.enableSchemaGeneration);
                m_settings.tooling.enableFModelSnippetGenerator = tooling->value(
                    "enableFModelSnippetGenerator", m_settings.tooling.enableFModelSnippetGenerator);

                if (auto schemaTypes = tooling->find("schemaTypes");
                    schemaTypes != tooling->end() && schemaTypes->is_object())
                {
                    auto& settings = m_settings.tooling.schemaTypes;
                    settings.utility = schemaTypes->value("utility", settings.utility);
                    settings.assets = schemaTypes->value("assets", settings.assets);
                    settings.blueprints = schemaTypes->value("blueprints", settings.blueprints);
                    settings.buildings = schemaTypes->value("buildings", settings.buildings);
                    settings.courses = schemaTypes->value("courses", settings.courses);
                    settings.enums = schemaTypes->value("enums", settings.enums);
                    settings.journal = schemaTypes->value(
                        "journal", schemaTypes->value("journals", settings.journal));
                    settings.raw = schemaTypes->value(
                        "raw", schemaTypes->value("tables", settings.raw));
                    settings.recipes = schemaTypes->value("recipes", settings.recipes);
                    settings.spawns = schemaTypes->value("spawns", settings.spawns);
                    settings.strings = schemaTypes->value("strings", settings.strings);
                }

                if (auto modsTxt = tooling->find("modsTxt"); modsTxt != tooling->end() && modsTxt->is_object())
                {
                    auto& settings = m_settings.tooling.modsTxt;
                    settings.enabled = modsTxt->value("enabled", settings.enabled);
                    settings.autoCreate = modsTxt->value("autoCreate", settings.autoCreate);
                    settings.reconcileFolders = modsTxt->value("reconcileFolders", settings.reconcileFolders);
                    settings.preserveComments = modsTxt->value("preserveComments", settings.preserveComments);
                    settings.strictValues = modsTxt->value("strictValues", settings.strictValues);
                }

                if (auto reports = tooling->find("compatibilityReports"); reports != tooling->end() && reports->is_object())
                {
                    auto& settings = m_settings.tooling.compatibilityReports;
                    settings.enabled = reports->value("enabled", settings.enabled);
                    settings.writeFile = reports->value("writeFile", settings.writeFile);
                    settings.warnSameTarget = reports->value("warnSameTarget", settings.warnSameTarget);
                    settings.warnSameProperty = reports->value("warnSameProperty", settings.warnSameProperty);
                    settings.warnArrayReplacement = reports->value("warnArrayReplacement", settings.warnArrayReplacement);
                }
            }

            // Missing keys retain their defaults in memory. Do not rewrite a valid
            // user config merely because RuneSchema was started.
        }
        catch (const std::exception& e)
        {
            PS::Log<RC::LogLevel::Error>(STR("Error parsing config: {}\n"), PS::ToWideSafe(e.what()));
            this->Save();
            PS::Log<RC::LogLevel::Normal>(STR("Config has been repaired.\n"));
        }

        PS::Log<RC::LogLevel::Normal>(STR("Config loaded.\n"));
    }

    std::filesystem::path PSConfig::GetConfigPath()
    {
        static auto path = fs::path(UE4SSProgram::get_program().get_working_directory()) / "Mods" / "RuneSchema" / "config";
        return path;
    }

    void PSConfig::Save()
    {
        auto configFile = GetConfigPath() / "config.json";
        try
        {
            EnsureConfigWritable(configFile);

            auto data = m_rawSettings.is_object() ? m_rawSettings : nlohmann::json::object();
            // configVersion was an internal migration marker, not a user-facing
            // setting. Accept it when reading older files but omit it going forward.
            data.erase("configVersion");
            data["languageOverride"] = m_settings.languageOverride;
            data["enableAutoReload"] = m_settings.enableAutoReload;
            data["enableDebugLogging"] = m_settings.enableDebugLogging;
            data["enableExperimentalDropScaling"] = m_settings.enableExperimentalDropScaling;
            auto& identity = data["identityOverrides"];
            identity["enabled"] = m_settings.identityOverrides.enabled;
            identity["assets"] = m_settings.identityOverrides.assets;
            identity["recipes"] = m_settings.identityOverrides.recipes;
            identity["journals"] = m_settings.identityOverrides.journals;
            identity["dryRun"] = m_settings.identityOverrides.dryRun;
            identity["logChanges"] = m_settings.identityOverrides.logChanges;
            auto& spawnSafety = data["spawnSafety"];
            spawnSafety["maxScale"] = m_settings.spawnSafety.maxScale;
            spawnSafety["maxDropIncreasePercent"] = m_settings.spawnSafety.maxDropIncreasePercent;
            auto& tooling = data["tooling"];
            tooling["enabled"] = m_settings.tooling.enabled;
            tooling["enableSchemaGeneration"] = m_settings.tooling.enableSchemaGeneration;
            tooling["enableFModelSnippetGenerator"] = m_settings.tooling.enableFModelSnippetGenerator;
            auto& schemaTypes = tooling["schemaTypes"];
            schemaTypes.erase("journals");
            schemaTypes.erase("tables");
            schemaTypes["utility"] = m_settings.tooling.schemaTypes.utility;
            schemaTypes["assets"] = m_settings.tooling.schemaTypes.assets;
            schemaTypes["blueprints"] = m_settings.tooling.schemaTypes.blueprints;
            schemaTypes["buildings"] = m_settings.tooling.schemaTypes.buildings;
            schemaTypes["courses"] = m_settings.tooling.schemaTypes.courses;
            schemaTypes["enums"] = m_settings.tooling.schemaTypes.enums;
            schemaTypes["journal"] = m_settings.tooling.schemaTypes.journal;
            schemaTypes["raw"] = m_settings.tooling.schemaTypes.raw;
            schemaTypes["recipes"] = m_settings.tooling.schemaTypes.recipes;
            schemaTypes["spawns"] = m_settings.tooling.schemaTypes.spawns;
            schemaTypes["strings"] = m_settings.tooling.schemaTypes.strings;
            auto& modsTxt = tooling["modsTxt"];
            modsTxt["enabled"] = m_settings.tooling.modsTxt.enabled;
            modsTxt["autoCreate"] = m_settings.tooling.modsTxt.autoCreate;
            modsTxt["reconcileFolders"] = m_settings.tooling.modsTxt.reconcileFolders;
            modsTxt["preserveComments"] = m_settings.tooling.modsTxt.preserveComments;
            modsTxt["strictValues"] = m_settings.tooling.modsTxt.strictValues;
            auto& reports = tooling["compatibilityReports"];
            reports["enabled"] = m_settings.tooling.compatibilityReports.enabled;
            reports["writeFile"] = m_settings.tooling.compatibilityReports.writeFile;
            reports["warnSameTarget"] = m_settings.tooling.compatibilityReports.warnSameTarget;
            reports["warnSameProperty"] = m_settings.tooling.compatibilityReports.warnSameProperty;
            reports["warnArrayReplacement"] = m_settings.tooling.compatibilityReports.warnArrayReplacement;

            std::ofstream file(configFile, std::ios::trunc);
            file << data.dump(4) << '\n';
            if (!file.good())
            {
                throw std::runtime_error("write failed");
            }
            m_rawSettings = std::move(data);
        }
        catch (const std::exception& e)
        {
            PS::Log<RC::LogLevel::Error>(STR("Failed to write config: {}\n"), PS::ToWideSafe(e.what()));
        }
    }
}
