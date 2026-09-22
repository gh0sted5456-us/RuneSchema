#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Helpers/String.hpp"
#include "Core/ConfigFiles.h"
#include "Utility/ConfigCodec.h"
#include "Runtime/HostServices.h"
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace PS {

    PSConfig* PSConfig::Get()
    {
        static PSConfig config;
        return &config;
    }

    bool PSConfig::IsAutoReloadEnabled()
    {
        auto config = Get();
        return config ? config->m_settings.enableAutoReload : false;
    }

    bool PSConfig::IsDebugLoggingEnabled()
    {
        auto config = Get();
        return config ? config->m_settings.advancedLogging : false;
    }

    bool PSConfig::IsExperimentalDropScalingEnabled()
    {
        auto config = Get();
        return config ? config->m_settings.enableExperimentalDropScaling : false;
    }

    const LoadOrderSettings& PSConfig::GetLoadOrderSettings()
    {
        return m_settings.loadOrder;
    }

    bool PSConfig::IsLoaderEnabled(const std::string& name) const
    {
        const auto& v = m_settings.loaders;
        if (name == "assets") return v.assets;
        if (name == "blueprints") return v.blueprints;
        if (name == "buildings") return v.buildings;
        if (name == "courses") return v.courses;
        if (name == "effects") return v.effects;
        if (name == "enums") return v.enums;
        if (name == "equipment") return v.equipment;
        if (name == "journal") return v.journal;
        if (name == "lore") return v.lore;
        if (name == "nameplates") return v.nameplates;
        if (name == "niagara") return v.niagara;
        if (name == "players") return v.players;
        if (name == "raw") return v.raw;
        if (name == "recipes") return v.recipes;
        if (name == "spawns") return v.spawns;
        if (name == "strings") return v.strings;
        if (name == "vendors") return v.vendors;
        if (name == "npc") return v.npc;
        if (name == "dialogue") return v.dialogue;
        if (name == "quests") return v.quests;
        if (name == "registry") return v.registry;
        if (name == "events") return v.events;
        return true;
    }

    bool PSConfig::IsRoutineNotificationEnabled(std::string_view channel) const
    {
        // Clean/Debug controls output; saved channel settings remain compatible.
        return m_settings.advancedLogging;
    }

    PSConfigSettings& PSConfig::GetMutableSettings() { return m_settings; }
    const PSConfigSettings& PSConfig::GetSettings() const { return m_settings; }

    void PSConfig::Load()
    {
        m_settings = PSConfigSettings{};
        m_preserveOriginal = true;
        const auto configFile = GetSettingsPath() / "settings.jsonc";
        std::string contents;
        try {
            HostServices::MigrateLegacyLayout();
            if (!fs::exists(configFile)) {
                m_preserveOriginal = false;
                if (Save()) m_status = "Configuration created with default settings.";
                return;
            }
            contents = ConfigFiles::Read(configFile);
        } catch (const std::exception& error) {
            m_status = std::string("Cannot read configuration; using session defaults. Original left untouched: ") + error.what();
            PS::Log<RC::LogLevel::Error>(STR("{}\n"), RC::to_generic_string(m_status));
            return;
        }
        try {
            m_settings = DecodeSettings(contents);
            if(m_settings.plugins.compatibilityNotices!="normal"&&m_settings.plugins.compatibilityNotices!="quiet"&&m_settings.plugins.compatibilityNotices!="off")
                throw std::runtime_error("Invalid plugins.compatibilityNotices; use normal, quiet, or off");
            m_preserveOriginal = false;
            if(contents.find("// RuneSchema settings.")==std::string::npos)
                ConfigFiles::Write(configFile,EncodeSettings(m_settings));
            m_status = "Configuration loaded.";
            PS::Log<RC::LogLevel::Normal>(STR("Config loaded.\n"));
        } catch (const std::exception& error) {
            PS::Log<RC::LogLevel::Error>(STR("Invalid configuration; recovering with defaults: {}\n"), RC::to_generic_string(error.what()));
            // Save preserves the original before attempting any replacement.
            Save();
        }
    }

    std::filesystem::path PSConfig::GetSettingsPath()
    {
        static auto path = PS::HostServices::SettingsDirectory();
        return path;
    }

    bool PSConfig::Save()
    {
        const auto configFile = GetSettingsPath() / "settings.jsonc";
        fs::path backup;
        try {
            const auto contents = EncodeSettings(m_settings);
            if(m_settings.plugins.compatibilityNotices!="normal"&&m_settings.plugins.compatibilityNotices!="quiet"&&m_settings.plugins.compatibilityNotices!="off")
                throw std::runtime_error("Invalid plugins.compatibilityNotices; use normal, quiet, or off");
            if (m_preserveOriginal) backup = ConfigFiles::Backup(configFile);
            ConfigFiles::Write(configFile, contents);
            m_preserveOriginal = false;
            m_status = backup.empty() ? "Settings saved. Restart for startup settings."
                : "Configuration recovered with current settings. Original preserved at: " + backup.string();
            if (!backup.empty()) PS::Log<RC::LogLevel::Warning>(STR("{}\n"), RC::to_generic_string(m_status));
            return true;
        } catch (const std::exception& error) {
            m_status = std::string("Settings NOT saved; changes are session-only. ") + error.what();
            if (!backup.empty()) m_status += " Original backup: " + backup.string();
            PS::Log<RC::LogLevel::Error>(STR("{}\n"), RC::to_generic_string(m_status));
            return false;
        }
    }
}
