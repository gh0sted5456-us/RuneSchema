#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Utility/StructuredDiagnostics.h"
#include "Utility/StructuredDiagnosticB3.h"
#include "Core/JsonDocument.h"
#include "Helpers/String.hpp"
#include "Core/ConfigFiles.h"
#include "Core/DiagnosticLogPaths.h"
#include "Utility/ConfigCodec.h"
#include "Utility/ConfigStore.h"
#include "Core/RuneSchemaSettingsEditor.h"
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
        JsonHelpers::SetDiagnosticReporter(+[](const char* code,const fs::path& file,
                const std::string& field,const std::string& detail) noexcept {
            try {
                auto provenance=StructuredDiagnosticB3::FromPath(file);
                if(std::string_view(code)=="RS-JSON-005")provenance.File.clear();
                provenance.OperationId=StructuredDiagnosticB2::NewOperationId("json");
                const std::string identifier=code;
                const bool missing=identifier=="RS-JSON-002";
                const bool incompatible=identifier=="RS-JSON-003";
                const bool limit=identifier=="RS-JSON-004"||identifier=="RS-JSON-005";
                const auto title=missing?"Required JSON field missing":incompatible?
                    "JSON field has wrong type":limit?"JSON safety limit exceeded":"JSON definition could not be parsed";
                (void)StructuredDiagnosticB3::Report(identifier,"Error",missing||incompatible||limit?"Blocked":"Failed",
                    "JsonParser",title,detail,std::move(provenance),
                    (identifier=="RS-JSON-005")?"Discovery":(missing||incompatible||limit)?"Validation":"Parsing",field);
            }catch(...){}
        });
        m_settings = PSConfigSettings{};
        m_preserveOriginal = true;
        m_configDocument=nlohmann::json::object();
        m_configFile=GetSettingsPath()/"settings.ini";
        try {
            HostServices::InitializeRuntimeLayout();
            if(!fs::exists(m_configFile)&&fs::exists(GetSettingsPath()/"settings.jsonc"))m_configFile=GetSettingsPath()/"settings.jsonc";
            auto loaded=SettingsStore::Load(GetSettingsPath());
            m_settings=std::move(loaded.Settings);
            m_configDocument=std::move(loaded.Document);
            m_configFile=std::move(loaded.File);
            m_status=std::move(loaded.Status);
            m_preserveOriginal=false;
            DiagnosticLogPaths::UseUE4SS=m_settings.logsInUE4SS;
            DiagnosticLog::Initialize(m_settings.archiveLogs);
            StructuredDiagnostics::Initialize();
            if(fs::exists(GetSettingsPath().parent_path()/"modmenu.json")) {
                const auto prepared=RuneSchemaSettingsEditor::PrepareModMenuFields(GetSettingsPath());
                if(!prepared.value("Ok",false))PS::Log<RC::LogLevel::Warning>(STR(
                    "[SETTINGS][MOD-MENU][DEFERRED] Controls could not be prepared: {}. Existing settings retained.\n"),
                    RC::to_generic_string(prepared.value("Status",std::string("settings unavailable"))));
            }
            PS::Log<RC::LogLevel::Normal>(STR("{}\n"),RC::to_generic_string(m_status));
        } catch (const std::exception& error) {
            // Invalid configuration must not discard the earlier diagnostic log.
            DiagnosticLog::Initialize(false,false);
            StructuredDiagnostics::Initialize();
            m_status = std::string("Cannot load configuration; using session defaults. Originals left untouched: ") + error.what();
            PS::CodedLog<RC::LogLevel::Error>(PS::Diagnostics::Id::ConfigLoadFailed, STR("{}\n"), RC::to_generic_string(m_status));
        }
    }

    std::filesystem::path PSConfig::GetSettingsPath()
    {
        static auto path = PS::HostServices::SettingsDirectory();
        return path;
    }

    bool PSConfig::Save()
    {
        const auto configFile = m_configFile.empty()?GetSettingsPath()/"settings.ini":m_configFile;
        fs::path backup;
        try {
            const auto document=SettingsDocument(m_settings,m_configDocument);
            const auto contents=configFile.extension()==".ini"?EncodeIniDocument(document):document.dump(2)+"\n";
            (void)DecodeSettings(document.dump());
            if (m_preserveOriginal&&fs::exists(configFile)) backup = ConfigFiles::Backup(configFile);
            ConfigFiles::Write(configFile, contents);
            m_configDocument=document;
            m_preserveOriginal = false;
            m_status = backup.empty() ? "Settings saved. Restart for startup settings."
                : "Configuration recovered with current settings. Original preserved at: " + backup.string();
            if (!backup.empty()) PS::CodedLog<RC::LogLevel::Warning>(PS::Diagnostics::Id::ConfigRecovered, STR("{}\n"), RC::to_generic_string(m_status));
            return true;
        } catch (const std::exception& error) {
            m_status = std::string("Settings NOT saved; changes are session-only. ") + error.what();
            if (!backup.empty()) m_status += " Original backup: " + backup.string();
            PS::CodedLog<RC::LogLevel::Error>(PS::Diagnostics::Id::ConfigSaveFailed, STR("{}\n"), RC::to_generic_string(m_status));
            return false;
        }
    }
}
