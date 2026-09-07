#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include "nlohmann/json.hpp"

namespace PS {
    struct LoadOrderSettings {
        bool enabled = true;
        bool autoCreate = true;
        bool reconcileFolders = true;
        bool preserveComments = true;
        bool strictValues = true;
        bool deterministicFallback = true;
    };

    struct RoutineNotificationSettings {
        bool modLoading = true;
        bool assets = true;
        bool raw = true;
        bool recipes = true;
        bool journal = true;
        bool spawns = true;
        bool players = true;
        bool patches = true;
    };

    struct LoaderActivationSettings {
        bool equipment = true;
        bool blueprints = true;
        bool assets = true;
        bool recipes = true;
        bool journal = true;
        bool raw = true;
        bool enums = true;
        bool strings = true;
        bool buildings = true;
        bool spawns = true;
        bool courses = true;
        bool players = true;
    };

    struct SpawnBehaviorSettings {
        bool enableNativeRoaming = true;
        double defaultRoamRadius = 800.0;
        double defaultRoamMaxZTolerance = 200.0;
    };

    struct PSConfigSettings {
        std::string languageOverride = "";
        bool enableAutoReload = false;
        bool enableDebugLogging = false;
        bool enableExperimentalDropScaling = false;
        LoadOrderSettings loadOrder{};
        RoutineNotificationSettings notifications{};
        LoaderActivationSettings loaders{};
        SpawnBehaviorSettings spawnBehavior{};
    };

    class PSConfig {
    public:
        static PSConfig* Get();
    public:
        bool IsAutoReloadEnabled();

        bool IsDebugLoggingEnabled();

        bool IsExperimentalDropScalingEnabled();

        const LoadOrderSettings& GetLoadOrderSettings();

        bool IsLoaderEnabled(const std::string& loaderName) const;

        bool IsRoutineNotificationEnabled(std::string_view channel) const;

        PSConfigSettings& GetMutableSettings();

        const PSConfigSettings& GetSettings() const;

        void Save();

        void Load();
    private:
        static std::filesystem::path GetConfigPath();

    private:
        PSConfigSettings m_settings;
    };
}
