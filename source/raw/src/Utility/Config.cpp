#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Utility/StructuredDiagnostics.h"
#include "Utility/StructuredDiagnosticB3.h"
#include "Core/JsonDocument.h"
#include "Helpers/String.hpp"
#include "Core/ConfigFiles.h"
#include "Core/HelpyRemoteAccess.h"
#include "Core/DiagnosticLogPaths.h"
#include "Utility/ConfigCodec.h"
#include "Utility/ConfigStore.h"
#include "Core/RuneSchemaSettingsEditor.h"
#include "Runtime/HostServices.h"
#include "Misc/FileWatchWrapper.h"
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace PS {
    PSConfig::PSConfig() = default;
    PSConfig::~PSConfig() = default;

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
    std::string_view PSConfig::ItemDeliveryMode() const {
        return m_preserveOriginal?"off":DecodeItemDeliveryMode(m_configDocument);
    }

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
        m_liveHelpyAuthority=m_settings.helpyAuthority;
        InvalidateHelpyAuthority();
    }

    void PSConfig::StartHelpyAuthorityWatch()
    {
        if(m_helpyAuthorityWatch||m_configFile.empty())return;
        try{
            const auto filename=m_configFile.filename().string();
            auto watcher=std::make_unique<FileWatchWrapper>(m_configFile.parent_path(),
                [this,filename](auto,const auto&,const std::string& name,auto,const std::string& previous){
                    if(name==filename||previous==filename)InvalidateHelpyAuthority();
                });
            if(!watcher->IsWatchingDirectory())throw std::runtime_error("Settings directory watch unavailable");
            watcher->Watch();
            m_helpyAuthorityWatch=std::move(watcher);
            InvalidateHelpyAuthority();
        }catch(const std::exception&){
            PS::CodedLog<RC::LogLevel::Warning>(PS::Diagnostics::Id::ConfigLoadFailed,
                STR("Cannot watch the Helpy server settings file. Save from Helpy to apply changes while the host is running.\n"));
        }
    }

    void PSConfig::StopHelpyAuthorityWatch()
    {
        m_helpyAuthorityWatch.reset();
    }

    void PSConfig::ApplyHelpyAuthority(HelpyAuthoritySettings latest)
    {
        if(latest==m_liveHelpyAuthority)return;
        if(latest.allowPinAccess!=m_liveHelpyAuthority.allowPinAccess||latest.accessPin!=m_liveHelpyAuthority.accessPin)
            HelpyRemoteAccess::Server.Reset();
        m_liveHelpyAuthority=std::move(latest);
        ++m_helpyAuthorityVersion;
    }

    uint64_t PSConfig::RefreshHelpyAuthority()
    {
        if(m_configFile.empty()||!m_helpyAuthorityRefresh.exchange(false))return m_helpyAuthorityVersion;
        std::lock_guard lock(RuneSchemaSettingsEditor::Gate);
        try{
            ApplyHelpyAuthority(SettingsStore::ReadHelpyAuthority(m_configFile));
            m_helpyAuthorityReadFailed=false;
        }catch(const std::exception&){
            if(!m_helpyAuthorityReadFailed)
                PS::CodedLog<RC::LogLevel::Warning>(PS::Diagnostics::Id::ConfigLoadFailed,
                    STR("Cannot reload Helpy server access settings. Last valid policy remains active.\n"));
            m_helpyAuthorityReadFailed=true;
        }
        return m_helpyAuthorityVersion;
    }

    bool PSConfig::SaveHelpyPlayerAccess(const HelpyAuthoritySettings& before,const HelpyAuthoritySettings& proposed)
    {
        std::lock_guard lock(RuneSchemaSettingsEditor::Gate);
        try{
            ApplyHelpyAuthority(SettingsStore::SaveHelpyPlayerAccess(m_configFile,before,proposed));
            return true;
        }catch(const std::exception&){
            InvalidateHelpyAuthority();
            return false;
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
        std::lock_guard lock(RuneSchemaSettingsEditor::Gate);
        try {
            auto document=SettingsDocument(m_settings,m_configDocument);
            if(!m_preserveOriginal&&fs::exists(configFile)){
                SettingsStore::Loaded persisted;
                try{persisted=SettingsStore::Parse(configFile,configFile.extension()==".ini");}
                catch(const std::exception&){throw std::runtime_error("Cannot verify the current server access settings. Reload them in Helpy before saving.");}
                const auto disk=SettingsDocument(persisted.Settings,persisted.Document).at("helpyAuthority");
                const auto baseline=SettingsDocument(DecodeSettings(m_configDocument.dump()),m_configDocument).at("helpyAuthority");
                auto& edited=document.at("helpyAuthority");
                if(disk!=baseline&&edited!=disk){
                    if(edited!=baseline)throw std::runtime_error("Server access settings changed on disk. Reload them in Helpy before saving.");
                    edited=disk;
                }
            }
            const auto contents=configFile.extension()==".ini"?EncodeIniDocument(document):document.dump(2)+"\n";
            auto validated=DecodeSettings(document.dump());
            if (m_preserveOriginal&&fs::exists(configFile)) backup = ConfigFiles::Backup(configFile);
            ConfigFiles::Write(configFile, contents);
            m_configDocument=document;
            m_settings.helpyAuthority=std::move(validated.helpyAuthority);
            InvalidateHelpyAuthority();
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
