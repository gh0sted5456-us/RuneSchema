#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>
#include "nlohmann/json.hpp"

namespace PS {
    struct ModsTxtSettings {
        bool enabled = true;
        bool autoCreate = true;
        bool reconcileFolders = true;
        bool preserveComments = true;
        bool strictValues = true;
    };

    struct CompatibilityReportSettings {
        bool enabled = true;
        bool writeFile = true;
        bool warnSameTarget = true;
        bool warnSameProperty = true;
        bool warnArrayReplacement = true;
    };

    struct ToolingSettings {
        bool enabled = true;
        ModsTxtSettings modsTxt;
        CompatibilityReportSettings compatibilityReports;
        bool enableSchemaGeneration = true;
        bool enableFModelSnippetGenerator = false;
    };

    struct PSConfigSettings {
        std::string languageOverride = "";
        bool enableAutoReload = false;
        bool enableDebugLogging = false;
        bool enableExperimentalDropScaling = false;
        ToolingSettings tooling;
    };

    class PSConfig {
    public:
        static PSConfig* Get();
    public:
        std::string GetLanguageOverride();

        bool IsAutoReloadEnabled();

        bool IsDebugLoggingEnabled();

        bool IsExperimentalDropScalingEnabled();

        bool IsSchemaGenerationEnabled();

        bool IsFModelSnippetGeneratorEnabled();

        bool IsToolingEnabled() const;

        const ModsTxtSettings& GetModsTxtSettings() const;

        const CompatibilityReportSettings& GetCompatibilityReportSettings() const;

        PSConfigSettings& GetSettings();

        void Load();

        void Save();

        static std::filesystem::path GetConfigPath();
    private:
        PSConfigSettings m_settings;
        nlohmann::json m_rawSettings = nlohmann::json::object();
    };
}
