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

    struct IdentityOverrideSettings {
        bool enabled = true;
        bool assets = true;
        bool recipes = true;
        bool journals = true;
        bool dryRun = false;
        bool logChanges = true;
    };

    struct SpawnSafetySettings {
        double maxScale = 10.0;
        double maxDropIncreasePercent = 500.0;
    };

    struct SchemaTypeSettings {
        bool assets = true;
        bool blueprints = true;
        bool recipes = true;
        bool journals = true;
        bool tables = true;
        bool enums = true;
    };

    struct ToolingSettings {
        bool enabled = true;
        ModsTxtSettings modsTxt;
        CompatibilityReportSettings compatibilityReports;
        bool enableSchemaGeneration = true;
        bool enableFModelSnippetGenerator = false;
        SchemaTypeSettings schemaTypes;
    };

    struct PSConfigSettings {
        std::string languageOverride = "";
        bool enableAutoReload = false;
        bool enableDebugLogging = false;
        bool enableExperimentalDropScaling = false;
        IdentityOverrideSettings identityOverrides;
        SpawnSafetySettings spawnSafety;
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

        const IdentityOverrideSettings& GetIdentityOverrideSettings() const;

        const SpawnSafetySettings& GetSpawnSafetySettings() const;

        bool IsSchemaGenerationEnabled();

        bool IsFModelSnippetGeneratorEnabled();

        bool IsToolingEnabled() const;

        const ModsTxtSettings& GetModsTxtSettings() const;

        const CompatibilityReportSettings& GetCompatibilityReportSettings() const;

        const SchemaTypeSettings& GetSchemaTypeSettings() const;

        PSConfigSettings& GetSettings();

        void Load();

        void Save();

        static std::filesystem::path GetConfigPath();
    private:
        PSConfigSettings m_settings;
        nlohmann::json m_rawSettings = nlohmann::json::object();
    };
}
