#include <atomic>
#include <filesystem>
#include <fstream>
#include <vector>
#include "Helpers/String.hpp"
#include "Mod/CppUserModBase.hpp"
#include "UE4SSProgram.hpp"
#include "Generator/JsonSchemaGenerator.h"
#include "Generator/FModelSnippetGenerator.h"
#include "Loader/CompatibilityReporter.h"
#include "Loader/DragonWildsMainLoader.h"
#include "Loader/ModLoadOrder.h"
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "SDK/DragonWildsSignatures.h"
#include "SDK/UnrealOffsets.h"

using namespace RC;
using namespace RC::Unreal;

class RuneSchema : public RC::CppUserModBase
{
public:
    RuneSchema() : CppUserModBase()
    {
        ModName = STR("RuneSchema");
        ModVersion = STR("0.6.1 Experimental");
        ModDescription = STR("Experimental RuneSchema build with mod management and extended spawn controls.");
        ModAuthors = STR("Okaetsu");

        auto config = PS::PSConfig::Get();
        config->Load();

        if (config->IsFModelSnippetGeneratorEnabled())
        {
            PS::FModelSnippetGenerator::GenerateConfiguredInputs();
        }

        PS::Log<LogLevel::Verbose>(STR("Initializing SignatureManager...\n"));
        DragonWilds::SignatureManager::Initialize();

        PS::Log<LogLevel::Verbose>(STR("Initializing UnrealOffsets...\n"));
        DragonWilds::UnrealOffsets::Initialize();

        PS::Log<LogLevel::Verbose>(STR("Preparing to pre-initialize RuneSchema...\n"));
        MainLoader.PreInitialize();

        PS::Log<RC::LogLevel::Normal>(STR("{} v{} by {} loaded.\n"), ModName, ModVersion, ModAuthors);
    }

    ~RuneSchema() override
    {
    }

    auto on_ui_init() -> void override
    {
        if (!UE4SSProgram::settings_manager.Debug.DebugConsoleEnabled)
        {
            return;
        }

        PS::Log<LogLevel::Verbose>(STR("GUI Console is enabled, enabling ImGui for RuneSchema...\n"));

        UE4SS_ENABLE_IMGUI()

        PS::Log<LogLevel::Verbose>(STR("Registering RuneSchema tab in GUI Console...\n"));
        register_tab(STR("RuneSchema"), [](CppUserModBase* instance) {
            auto mod = dynamic_cast<RuneSchema*>(instance);
            if (!mod)
            {
                return;
            }

            mod->render_control_panel();
        });

        PS::Log<LogLevel::Verbose>(STR("Finished registering RuneSchema tab for GUI Console.\n"));
    }

    auto render_control_panel() -> void
    {
        if (!m_uiCacheInitialized || m_refreshUi.exchange(false))
        {
            refresh_ui_cache();
        }

        if (!ImGui::BeginTabBar("RuneSchemaControlPanel"))
        {
            return;
        }

        if (ImGui::BeginTabItem("Overview"))
        {
            render_overview();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Settings"))
        {
            render_settings();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Generators"))
        {
            render_generators();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Load Order"))
        {
            render_load_order();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Compatibility"))
        {
            render_compatibility();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    auto render_overview() -> void
    {
        const auto root = get_runeschema_path();
        const auto modsPath = root / "mods";

        ImGui::SeparatorText("Status");
        ImGui::Text("RuneSchema %s", "0.6.1 Experimental");
        ImGui::Text("Detected mod folders: %zu", m_cachedModCount);
        ImGui::Text("Tooling: %s", PS::PSConfig::Get()->IsToolingEnabled() ? "Enabled" : "Disabled");
        ImGui::Text("Configuration: %s", (root / "config" / "config.json").string().c_str());
        ImGui::Text("Mods: %s", modsPath.string().c_str());

        ImGui::SeparatorText("Design");
        if (ImGui::Button("Refresh Status")) refresh_ui_cache();
        ImGui::TextWrapped("Status is cached. This panel performs filesystem work only when opened, refreshed, or an action is requested.");
        if (m_configDirty)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Settings have unsaved changes.");
        }
    }

    auto render_settings() -> void
    {
        auto* config = PS::PSConfig::Get();
        auto& settings = config->GetSettings();
        bool changed = false;

        ImGui::SeparatorText("Runtime");
        changed |= ImGui::Checkbox("Automatic reload", &settings.enableAutoReload);
        changed |= ImGui::Checkbox("Debug logging", &settings.enableDebugLogging);
        changed |= ImGui::Checkbox(
            "Experimental spawn drop scaling",
            &settings.enableExperimentalDropScaling);
        ImGui::TextWrapped(
            "When enabled, an explicit DropIncreasePercent multiplies supported MinToDrop/MaxToDrop rows. Scale changes size only. Restart after changing this setting.");

        ImGui::SeparatorText("Tooling");
        changed |= ImGui::Checkbox("Enable tooling", &settings.tooling.enabled);
        changed |= ImGui::Checkbox("Enable JSON schema generation", &settings.tooling.enableSchemaGeneration);
        changed |= ImGui::Checkbox("Enable FModel snippet generator", &settings.tooling.enableFModelSnippetGenerator);

        ImGui::SeparatorText("mods.txt");
        auto& modsTxt = settings.tooling.modsTxt;
        changed |= ImGui::Checkbox("Enable load-order file", &modsTxt.enabled);
        changed |= ImGui::Checkbox("Create mods.txt automatically", &modsTxt.autoCreate);
        changed |= ImGui::Checkbox("Reconcile folders", &modsTxt.reconcileFolders);
        changed |= ImGui::Checkbox("Preserve comments", &modsTxt.preserveComments);
        changed |= ImGui::Checkbox("Require 0 or 1 values", &modsTxt.strictValues);

        if (changed) m_configDirty = true;
        if (ImGui::Button("Save Settings"))
        {
            config->Save();
            m_configDirty = false;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(m_configDirty ? "Unsaved" : "Saved");
        ImGui::TextWrapped("Most switches apply immediately. Loader initialization and automatic reload changes are safest after a restart.");
    }

    auto render_generators() -> void
    {
        ImGui::SeparatorText("JSON Schemas");
        if (ImGui::Button("Generate JSON Schema Files"))
        {
            if (PS::PSConfig::Get()->IsSchemaGenerationEnabled())
            {
                bool expected = false;
                m_generateSchemas.compare_exchange_strong(expected, true);
            }
        }

        if (m_generateSchemas)
        {
            ImGui::ProgressBar(-0.5f * (float)ImGui::GetTime(), ImVec2(0.0f, 0.0f), "Generating...");
        }

        ImGui::TextWrapped("Writes editor autocomplete schemas for mod JSON files to Mods/RuneSchema/schemas. "
                           "Data tables load with the world, so run this after entering one for full coverage.");
        if (!PS::PSConfig::Get()->IsSchemaGenerationEnabled())
        {
            ImGui::TextWrapped("Schema generation is disabled in config/config.json.");
        }

        ImGui::SeparatorText("FModel Conversion");
        if (ImGui::Button("Generate FModel Snippets"))
        {
            if (PS::PSConfig::Get()->IsFModelSnippetGeneratorEnabled())
            {
                bool expected = false;
                m_generateFModel.compare_exchange_strong(expected, true);
            }
        }
        if (m_generateFModel)
        {
            ImGui::ProgressBar(-0.5f * (float)ImGui::GetTime(), ImVec2(0.0f, 0.0f), "Converting...");
        }
        ImGui::TextWrapped("Reads FModel JSON exports from config/fmodel-input and writes sanitized RuneSchema snippets to config/fmodel-snippets.");
        if (!PS::PSConfig::Get()->IsFModelSnippetGeneratorEnabled())
        {
            ImGui::TextWrapped("The FModel snippet generator is disabled in Settings.");
        }
    }

    auto render_load_order() -> void
    {
        ImGui::SeparatorText("Actions");
        if (ImGui::Button("Reconcile mods.txt Now"))
        {
            bool expected = false;
            m_reconcileMods.compare_exchange_strong(expected, true);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(m_reconcileMods ? "Working..." : "Scans once when requested.");

        ImGui::SeparatorText("Current Load Order");
        ImGui::Text("%s", (get_runeschema_path() / "mods" / "mods.txt").string().c_str());
        if (ImGui::Button("Refresh Load Order"))
        {
            refresh_ui_cache();
            m_loadOrderDirty = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Load Order"))
        {
            const auto& settings = PS::PSConfig::Get()->GetModsTxtSettings();
            DragonWilds::ModLoadOrder::WriteEntries(
                get_runeschema_path() / "mods", m_cachedLoadOrder, settings.preserveComments);
            m_loadOrderDirty = false;
        }

        bool reordered = false;
        for (std::size_t index = 0; index < m_cachedLoadOrder.size(); ++index)
        {
            auto& entry = m_cachedLoadOrder[index];
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::Checkbox("##Enabled", &entry.Enabled)) m_loadOrderDirty = true;
            ImGui::SameLine();
            ImGui::Text("%zu. %s", index + 1, RC::to_string(entry.Name).c_str());
            ImGui::SameLine();
            if (index > 0 && ImGui::SmallButton("Up"))
            {
                std::swap(m_cachedLoadOrder[index], m_cachedLoadOrder[index - 1]);
                m_loadOrderDirty = true;
                reordered = true;
            }
            ImGui::SameLine();
            if (index + 1 < m_cachedLoadOrder.size() && ImGui::SmallButton("Down"))
            {
                std::swap(m_cachedLoadOrder[index], m_cachedLoadOrder[index + 1]);
                m_loadOrderDirty = true;
                reordered = true;
            }
            ImGui::PopID();
            if (reordered) break;
        }
        if (m_cachedLoadOrder.empty())
        {
            ImGui::TextUnformatted("No active load-order entries were found.");
        }
        if (m_loadOrderDirty)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Load-order changes are not saved yet.");
        }
        ImGui::TextWrapped("Checked mods are enabled. Order and enable states are written to mods.txt when Save Load Order is pressed and apply on the next RuneSchema load.");
    }

    auto render_compatibility() -> void
    {
        auto* config = PS::PSConfig::Get();
        auto& reports = config->GetSettings().tooling.compatibilityReports;
        bool changed = false;

        ImGui::SeparatorText("Report Settings");
        changed |= ImGui::Checkbox("Enable compatibility reports", &reports.enabled);
        changed |= ImGui::Checkbox("Write compatibility_report.txt", &reports.writeFile);
        changed |= ImGui::Checkbox("Warn when mods share a target", &reports.warnSameTarget);
        changed |= ImGui::Checkbox("Warn when mods share a property", &reports.warnSameProperty);
        changed |= ImGui::Checkbox("Warn about array replacement", &reports.warnArrayReplacement);
        if (changed) m_configDirty = true;

        if (ImGui::Button("Generate Compatibility Report"))
        {
            if (config->IsToolingEnabled() && reports.enabled)
            {
                bool expected = false;
                m_generateCompatibility.compare_exchange_strong(expected, true);
            }
        }
        if (m_generateCompatibility)
        {
            ImGui::ProgressBar(-0.5f * (float)ImGui::GetTime(), ImVec2(0.0f, 0.0f), "Scanning...");
        }
        ImGui::TextWrapped("Performs a one-time scan of mod JSON targets and properties to flag likely load-order conflicts.");
    }

    auto on_update() -> void override
    {
        if (m_generateSchemas.exchange(false))
        {
            PS::JsonSchemaGenerator::GenerateSchemaFiles();
        }
        if (m_generateFModel.exchange(false))
        {
            PS::FModelSnippetGenerator::GenerateConfiguredInputs();
        }
        if (m_reconcileMods.exchange(false))
        {
            resolve_mod_order();
            m_refreshUi = true;
        }
        if (m_generateCompatibility.exchange(false))
        {
            auto orderedMods = resolve_mod_order();
            DragonWilds::CompatibilityReporter::Generate(get_runeschema_path() / "mods", orderedMods);
        }
    }

    auto on_program_start() -> void override
    {
    }

    auto on_unreal_init() -> void override
    {
        MainLoader.Initialize();
    }

private:
    static auto get_runeschema_path() -> std::filesystem::path
    {
        return std::filesystem::path(UE4SSProgram::get_program().get_working_directory())
            / "Mods" / "RuneSchema";
    }

    static auto resolve_mod_order() -> std::vector<RC::StringType>
    {
        const auto modsPath = get_runeschema_path() / "mods";
        std::vector<RC::StringType> discovered;
        if (std::filesystem::exists(modsPath))
        {
            for (const auto& entry : std::filesystem::directory_iterator(modsPath))
            {
                if (entry.is_directory()) discovered.push_back(entry.path().filename().native());
            }
        }
        return DragonWilds::ModLoadOrder::Resolve(modsPath, discovered);
    }

    auto refresh_ui_cache() -> void
    {
        const auto modsPath = get_runeschema_path() / "mods";
        m_cachedModCount = 0;
        if (std::filesystem::exists(modsPath))
        {
            for (const auto& entry : std::filesystem::directory_iterator(modsPath))
            {
                if (entry.is_directory()) ++m_cachedModCount;
            }
        }

        const auto& settings = PS::PSConfig::Get()->GetModsTxtSettings();
        m_cachedLoadOrder = DragonWilds::ModLoadOrder::ReadEntries(modsPath, settings.strictValues);
        m_uiCacheInitialized = true;
    }

    DragonWilds::DragonWildsMainLoader MainLoader;
    std::atomic<bool> m_generateSchemas = false;
    std::atomic<bool> m_generateFModel = false;
    std::atomic<bool> m_reconcileMods = false;
    std::atomic<bool> m_generateCompatibility = false;
    std::atomic<bool> m_refreshUi = false;
    bool m_configDirty = false;
    bool m_loadOrderDirty = false;
    bool m_uiCacheInitialized = false;
    std::size_t m_cachedModCount = 0;
    std::vector<DragonWilds::ModOrderEntry> m_cachedLoadOrder;
};

#define RuneSchema_API __declspec(dllexport)
extern "C"
{
    RuneSchema_API RC::CppUserModBase* start_mod()
    {
        return new RuneSchema();
    }

    RuneSchema_API void uninstall_mod(RC::CppUserModBase* mod)
    {
        delete mod;
    }
}
