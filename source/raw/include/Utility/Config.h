#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
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

    struct LoaderActivationSettings {
        bool assets = true;
        bool blueprints = true;
        bool buildings = true;
        bool courses = true;
        bool dialogue = true;
        bool effects = true;
        bool enums = true;
        bool equipment = true;
        bool events = true;
        bool journal = true;
        bool lore = true;
        bool nameplates = true;
        bool niagara = true;
        bool npc = true;
        bool players = true;
        bool quests = true;
        bool registry = true;
        bool raw = true;
        bool recipes = true;
        bool spawns = true;
        bool strings = true;
        bool vendors = true;
    };

    struct SpawnBehaviorSettings {
        bool enableNativeRoaming = true;
        double defaultRoamRadius = 800.0;
        double defaultRoamMaxZTolerance = 200.0;
    };

    struct NpcDiagnosticSettings {
        bool statusExport = false;
        bool interactionTraceExport = false;
    };

    struct DiagnosticJobSettings {
        // Master switch for JSON trace/search jobs. advancedRuntime is the
        // global kill switch for every advanced diagnostic facility.
        bool enabled = false;
        bool characterEditorPreset = false;
    };

    struct PluginSettings {
        // "normal" logs compatibility notices normally, "quiet" emits them
        // only with advanced logging, and "off" suppresses them.
        std::string compatibilityNotices = "quiet";
    };

    struct PersistenceSettings {
        // Automatic /players appearance assignments stay read-only while
        // false. Character option/table loaders remain active.
        bool characterCustomization = false;
        // Journal/lore has no native transient unlock collection. When this
        // is false the loader still registers and places content, but skips
        // the player unlock call that would write it into the save.
        bool journal = false;
        // Recipes use the game's explicit non-persistent unlock collection
        // while this is false, so they remain usable for the current session.
        bool recipes = false;
    };

    struct HelpyAuthoritySettings {
        // Client Helpy requests are always server-executed and independently
        // validated. Permanent authoring/export operations are never accepted.
        bool allowClientItemGrants = false;
        bool allowClientTemporarySpawns = false;
        int maximumItemCount = 100;
        int maximumSpawnCount = 100;
        int maximumNpcDurationSeconds = 300;
        // Empty lists deny remote Helpy mutations. GUIDs are preferred;
        // exact names are an explicit compatibility fallback.
        std::vector<std::string> permittedPlayerGuids{};
        std::vector<std::string> permittedPlayerNames{};
    };

    struct PSConfigSettings {
        bool enableAutoReload = false;
        // Logging detail and advanced authoring/diagnostic facilities are
        // independent. Core loaders and network bridges are never gated here.
        bool advancedLogging = false;
        bool authoringTools = true;
        // Retained JSON name for compatibility. This now means heavyweight
        // diagnostics/full catalog work, never ordinary mod authoring.
        bool advancedRuntime = false;
        bool colorCodeLoaderAnnotations = true;
        bool enableExperimentalDropScaling = false;
        LoadOrderSettings loadOrder{};
        LoaderActivationSettings loaders{};
        SpawnBehaviorSettings spawnBehavior{};
        NpcDiagnosticSettings npcDiagnostics{};
        DiagnosticJobSettings diagnosticJobs{};
        PluginSettings plugins{};
        PersistenceSettings persistence{};
        HelpyAuthoritySettings helpyAuthority{};
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

        bool Save();
        const std::string& GetStatus() const { return m_status; }

        void Load();
    private:
        static std::filesystem::path GetSettingsPath();

    private:
        PSConfigSettings m_settings;
        std::string m_status;
        bool m_preserveOriginal = false;
    };
}
