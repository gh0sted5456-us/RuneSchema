#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Helpers/String.hpp"
#include <fstream>
#include "glaze/glaze.hpp"
#include "Runtime/HostServices.h"

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
        return config ? config->m_settings.enableDebugLogging : false;
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
        if (name == "equipment") return v.equipment;
        if (name == "blueprints") return v.blueprints;
        if (name == "assets") return v.assets;
        if (name == "recipes") return v.recipes;
        if (name == "journal") return v.journal;
        if (name == "raw") return v.raw;
        if (name == "enums") return v.enums;
        if (name == "strings") return v.strings;
        if (name == "buildings") return v.buildings;
        if (name == "spawns") return v.spawns;
        if (name == "courses") return v.courses;
        if (name == "players") return v.players;
        return true;
    }

    bool PSConfig::IsRoutineNotificationEnabled(std::string_view channel) const
    {
        const auto& v = m_settings.notifications;
        if (channel == "mods") return v.modLoading;
        if (channel == "assets") return v.assets;
        if (channel == "raw") return v.raw;
        if (channel == "recipes") return v.recipes;
        if (channel == "journal") return v.journal;
        if (channel == "spawns") return v.spawns;
        if (channel == "players") return v.players;
        if (channel == "patches") return v.patches;
        return true;
    }

    PSConfigSettings& PSConfig::GetMutableSettings() { return m_settings; }
    const PSConfigSettings& PSConfig::GetSettings() const { return m_settings; }

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

        auto readErrorCode = glz::read_file_json < glz::opts{ .error_on_missing_keys = false } > (m_settings, configFile.string(), std::string{});
        if (readErrorCode) {
            std::string errorMessage = glz::format_error(readErrorCode, std::string{});
            PS::Log<RC::LogLevel::Error>(STR("Error parsing config: {}\n"), RC::to_generic_string(errorMessage));
            this->Save();
            PS::Log<RC::LogLevel::Normal>(STR("Config has been repaired.\n"));
        }
        else
        {
            this->Save();
        }

        PS::Log<RC::LogLevel::Normal>(STR("Config loaded.\n"));
    }

    std::filesystem::path PSConfig::GetConfigPath()
    {
        static auto path = fs::path(PS::HostServices::WorkingDirectory()) / "Mods" / "RuneSchema" / "config";
        return path;
    }

    void PSConfig::Save()
    {
        auto configFile = GetConfigPath() / "config.json";
        auto writeErrorCode = glz::write_file_json<glz::opts{ .prettify = true }>(m_settings, configFile.string(), std::string{});
        if (writeErrorCode)
        {
            std::string errorMessage = glz::format_error(writeErrorCode, std::string{});
            PS::Log<RC::LogLevel::Error>(STR("Failed to write to config: {}\n"), RC::to_generic_string(errorMessage));
        }
    }
}
