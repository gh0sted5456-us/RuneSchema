#include <atomic>
#include "imgui.h"
#include "Unreal/Hooks.hpp"
#include "Mod/CppUserModBase.hpp"
#include "Runtime/HostServices.h"
#include "Generator/JsonSchemaGenerator.h"
#include "Generator/EquipmentSpellApi.h"
#include "Generator/InspectionTools.h"
#include "Generator/AppearanceTrace.h"
#include "Generator/PlayerTrace.h"
#include "Loader/DragonWildsMainLoader.h"
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Utility/StartupTrace.h"
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
        ModVersion = STR("0.6.1E (UE4SS-Stack Limit Fix Dependent)");
        ModDescription = STR("Allows modifying of DragonWilds's assets dynamically.");
        ModAuthors = STR("Snorkles (RuneSchema); Jonesing4Space (extended features); based on PalSchema by Okaetsu");

        auto config = PS::PSConfig::Get();
        PS::StartupTrace::Mark("config load begin");
        config->Load();
        PS::StartupTrace::Mark("signature scan begin");

        DragonWilds::SignatureManager::Initialize();
        PS::StartupTrace::Mark("signature scan complete; offsets begin");

        DragonWilds::UnrealOffsets::Initialize();
        PS::StartupTrace::Mark("offsets complete; early hooks begin");

        MainLoader.PreInitialize();
        PS::StartupTrace::Mark("early hooks complete");

        PS::Log<RC::LogLevel::Normal>(STR("{} v{} by {} loaded.\n"), ModName, ModVersion, ModAuthors);
    }

    ~RuneSchema() override
    {
        if (m_apiExportCallbackId != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_apiExportCallbackId);
        PS::PlayerTrace::Cancel();
        PS::AppearanceTrace::Cancel();
    }

    auto on_ui_init() -> void override
    {
        if (!PS::HostServices::GuiEnabled())
        {
            return;
        }

        PS::HostServices::InitializeGui();

        register_tab(STR("RuneSchema"), [](CppUserModBase* instance) {
            auto mod = dynamic_cast<RuneSchema*>(instance);
            if (!mod)
            {
                return;
            }

            mod->render_settings();
        });

    }

    auto render_schema_generator() -> void
    {
        if (ImGui::Button("Generate JSON Schema Files"))
        {
            bool expected = false;
            m_generateSchemas.compare_exchange_strong(expected, true);
        }

        if (m_generateSchemas)
        {
            ImGui::ProgressBar(-0.5f * (float)ImGui::GetTime(), ImVec2(0.0f, 0.0f), "Generating...");
        }

        ImGui::TextWrapped("Writes editor autocomplete schemas for mod JSON files to Mods/RuneSchema/schemas. "
                           "Data tables load with the world, so run this after entering one for full coverage.");
    }

    auto render_settings() -> void
    {
        auto* config = PS::PSConfig::Get();
        auto& settings = config->GetMutableSettings();
        if (ImGui::Button("Save settings")) {
            config->Save();
            PS::Log<LogLevel::Normal>(STR("RuneSchema settings saved. Restart for startup settings.\n"));
        }
        ImGui::Separator();
        if (ImGui::BeginTabBar("RuneSchemaSettings")) {
        if (ImGui::BeginTabItem("General")) {
        ImGui::BeginChild("GeneralContent", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGui::PushID("General");
        ImGui::Checkbox("Automatic JSON reload", &settings.enableAutoReload);
        ImGui::TextWrapped("Blueprint patch rules and ghost appearance changes require a restart.");
        ImGui::Checkbox("Advanced verbose logging", &settings.enableDebugLogging);
        ImGui::Checkbox("Experimental spawn drop scaling", &settings.enableExperimentalDropScaling);

        ImGui::PopID(); ImGui::EndChild(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Loaders")) {
        ImGui::BeginChild("LoadersContent", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGui::PushID("Loaders");
        ImGui::SeparatorText("Loader activation (restart required)");
        ImGui::Checkbox("Equipment runtime behaviors", &settings.loaders.equipment);
        ImGui::Checkbox("Assets", &settings.loaders.assets);
        ImGui::Checkbox("Raw tables", &settings.loaders.raw);
        ImGui::Checkbox("Recipes", &settings.loaders.recipes);
        ImGui::Checkbox("Journal", &settings.loaders.journal);
        ImGui::Checkbox("Blueprints", &settings.loaders.blueprints);
        ImGui::Checkbox("Buildings", &settings.loaders.buildings);
        ImGui::Checkbox("Enums", &settings.loaders.enums);
        ImGui::Checkbox("Strings", &settings.loaders.strings);
        ImGui::Checkbox("Courses", &settings.loaders.courses);
        ImGui::Checkbox("World / player runtime", &settings.loaders.spawns);
        ImGui::Checkbox("Player profiles", &settings.loaders.players);
        ImGui::TextWrapped("Disable one lane for crash isolation. Warnings and errors remain visible.");

        ImGui::PopID(); ImGui::EndChild(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Logging")) {
        ImGui::BeginChild("LoggingContent", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGui::PushID("Logging");
        ImGui::SeparatorText("Routine console notifications");
        ImGui::Checkbox("Mod loading", &settings.notifications.modLoading);
        ImGui::Checkbox("Assets", &settings.notifications.assets);
        ImGui::Checkbox("Raw", &settings.notifications.raw);
        ImGui::Checkbox("Recipes", &settings.notifications.recipes);
        ImGui::Checkbox("Journal", &settings.notifications.journal);
        ImGui::Checkbox("Patches", &settings.notifications.patches);
        ImGui::Checkbox("Spawns", &settings.notifications.spawns);
        ImGui::Checkbox("Players", &settings.notifications.players);
        ImGui::TextWrapped("These switches hide successful routine chatter only. Errors and warnings are never filtered.");

        ImGui::PopID(); ImGui::EndChild(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Spawns")) {
        ImGui::BeginChild("SpawnsContent", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGui::PushID("Spawns");
        ImGui::SeparatorText("Native AI roaming");
        ImGui::Checkbox("Allow native roaming", &settings.spawnBehavior.enableNativeRoaming);
        ImGui::InputDouble("Default roam radius", &settings.spawnBehavior.defaultRoamRadius, 50.0, 250.0, "%.0f");
        ImGui::InputDouble("Default vertical tolerance", &settings.spawnBehavior.defaultRoamMaxZTolerance, 25.0, 100.0, "%.0f");
        ImGui::TextWrapped("A spawn with AmbientBehaviour 'Roam' uses these defaults when RoamRadius or RoamMaxZTolerance is omitted.");

        ImGui::PopID(); ImGui::EndChild(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Load order")) {
        ImGui::BeginChild("Load orderContent", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGui::PushID("Load order");
        ImGui::SeparatorText("runeschema.txt load order");
        ImGui::Checkbox("Use runeschema.txt", &settings.loadOrder.enabled);
        ImGui::Checkbox("Create the file automatically", &settings.loadOrder.autoCreate);
        ImGui::Checkbox("Add new folders and remove missing folders", &settings.loadOrder.reconcileFolders);
        ImGui::Checkbox("Preserve comments when reconciling", &settings.loadOrder.preserveComments);
        ImGui::Checkbox("Only accept 0 or 1", &settings.loadOrder.strictValues);
        ImGui::Checkbox("Sort folders when no order file is used", &settings.loadOrder.deterministicFallback);
        ImGui::TextWrapped("Folder order is top-to-bottom. Set a mod to 0 to disable it. "
                           "$Patch/$Target documents are queued and applied after ordinary definitions.");
        ImGui::TextWrapped("AA_ folders load first; ZZ_ folders load last (case-insensitive). Order within each group is preserved. Name patch mods with the ZZ_ prefix.");

        ImGui::PopID(); ImGui::EndChild(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Tools")) {
            ImGui::TextWrapped("Diagnostic searches and dumps can stall the game and increase memory use. A game restart may be needed to restore normal performance after a diagnostic session.");
            if (m_apiExportCallbackId == Hook::ERROR_ID) {
                ImGui::TextWrapped("Tools are off. Activation lasts only for this game session and is not saved.");
                ImGui::BeginDisabled(!m_unrealReady.load());
                if (ImGui::Button("Activate Tools for this session")) ActivateTools();
                ImGui::EndDisabled();
            }
            if (m_apiExportCallbackId != Hook::ERROR_ID) {
            if (ImGui::BeginTabBar("RuneSchemaTools")) {
            constexpr const char* names[]{"Inspector", "Presets", "Traces", "Results"};
            constexpr PS::InspectionTools::Section sections[]{
                PS::InspectionTools::Section::Inspector, PS::InspectionTools::Section::Presets,
                PS::InspectionTools::Section::Traces,
                PS::InspectionTools::Section::Results};
            for (int i = 0; i < 4; ++i) {
                if (ImGui::BeginTabItem(names[i])) {
                    ImGui::PushID(names[i]);
                    ImGui::BeginChild("Content", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
                    PS::InspectionTools::Render(true, sections[i]);
                    ImGui::PopItemWidth();
                    ImGui::EndChild(); ImGui::PopID(); ImGui::EndTabItem();
                }
            }
            if (ImGui::BeginTabItem("Exports")) {
            ImGui::BeginChild("ExportsContent", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            ImGui::SeparatorText("Authoring exports");
            ImGui::BeginDisabled(m_apiExportCallbackId == Hook::ERROR_ID);
            render_schema_generator();
            ImGui::EndDisabled();
            ImGui::SeparatorText("Equipment spell API");
            if (m_apiExportCallbackId != Hook::ERROR_ID) {
                if (ImGui::Button("Export equipment/spell API")) m_exportEquipmentSpellApi = true;
            } else ImGui::TextWrapped("Equipment/spell API exporter could not register its game-thread callback.");
            ImGui::TextWrapped("Enter a world first. Exports native type signatures to RuneSchema/diagnostics/EquipmentSpellAPI.json. Does not grant spells or modify equipment.");
            ImGui::EndChild(); ImGui::EndTabItem(); }
            ImGui::EndTabBar(); }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        }
    }

    auto on_unreal_init() -> void override
    {
        PS::StartupTrace::Mark("UE4SS on_unreal_init begin");
        MainLoader.Initialize();
        PS::StartupTrace::Mark("UE4SS on_unreal_init complete");
        m_unrealReady = true;
    }

    void ActivateTools()
    {
        if (m_apiExportCallbackId != Hook::ERROR_ID || !m_unrealReady.load()) return;
        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("EquipmentSpellApiExport");
        m_apiExportCallbackId = Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>&, UEngine*, float, bool) {
                if (m_exportEquipmentSpellApi.exchange(false)) PS::EquipmentSpellApi::Export();
                if (m_generateSchemas.exchange(false)) PS::JsonSchemaGenerator::GenerateSchemaFiles();
                PS::InspectionTools::Tick();
            }, options);
    }

private:
    DragonWilds::DragonWildsMainLoader MainLoader;
    std::atomic<bool> m_generateSchemas = false;
    std::atomic<bool> m_exportEquipmentSpellApi = false;
    std::atomic<bool> m_unrealReady = false;
    Hook::GlobalCallbackId m_apiExportCallbackId = Hook::ERROR_ID;
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
