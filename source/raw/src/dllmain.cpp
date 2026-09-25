#include <atomic>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "imgui.h"
#include "Runtime/UE4SSCompatibility.h"
#include "Unreal/Hooks.hpp"
#include "Mod/CppUserModBase.hpp"
#include "Runtime/HostServices.h"
#include "Generator/JsonSchemaGenerator.h"
#include "Generator/InspectionTools.h"
#include "Generator/SaveViewer.h"
#include "Generator/SpawnToolsPanel.h"
#include "Generator/QuestStatusPanel.h"
#include "Runtime/SearchJobs.h"
#include "Runtime/NetworkRoleMonitor.h"
#include "Generator/NiagaraTest.h"
#include "Generator/LoaderTemplate.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Generator/AppearanceTrace.h"
#include "Generator/PlayerTrace.h"
#include "Loader/DragonWildsMainLoader.h"
#include "Runtime/PluginHost.h"
#include "Loader/ModLoadOrder.h"
#include "Loader/ModOrderPolicy.h"
#include "Utility/Config.h"
#include "Utility/BuildInfo.h"
#include "Runtime/Storefront.h"
#include "Runtime/MappingBackbone.h"
#include "Utility/Logging.h"
#include "Utility/StartupTrace.h"
#include "SDK/DragonWildsSignatures.h"
#include "SDK/UnrealOffsets.h"

using namespace RC;
using namespace RC::Unreal;

namespace {
enum class TabRole { Settings, Diagnostics, SavedData, Exports };

struct LoadOrderRow {
    std::string name;
    bool enabled = true;
    std::vector<size_t> loaders;
    std::vector<std::string> patches;
};
struct LoadOrderEditorState {
    bool loaded = false;
    bool dirty = false;
    std::string status;
    std::vector<LoadOrderRow> rows;
};

constexpr auto KnownLoaders=[] {
    std::array<const char*,PS::LoaderCapabilities.size()> names{};
    for(size_t i=0;i<names.size();++i)names[i]=PS::LoaderCapabilities[i].Name;
    return names;
}();
constexpr ImVec4 loaderColors[]{
    {0.22f,0.43f,0.64f,1},
    {0.22f,0.37f,0.58f,1},
    {0.50f,0.38f,0.22f,1},
    {0.32f,0.43f,0.24f,1},
    {0.40f,0.36f,0.52f,1},
    {0.54f,0.29f,0.38f,1},
    {0.38f,0.36f,0.55f,1},
    {0.46f,0.29f,0.35f,1},
    {0.55f,0.31f,0.22f,1},
    {0.46f,0.39f,0.25f,1},
    {0.42f,0.34f,0.25f,1},
    {0.48f,0.34f,0.58f,1},
    {0.57f,0.32f,0.18f,1},
    {0.35f,0.45f,0.55f,1},
    {0.43f,0.32f,0.59f,1},
    {0.47f,0.40f,0.24f,1},
    {0.28f,0.43f,0.49f,1},
    {0.24f,0.44f,0.32f,1},
    {0.36f,0.39f,0.57f,1},
    {0.52f,0.30f,0.23f,1},
    {0.48f,0.32f,0.43f,1},
    {0.30f,0.43f,0.43f,1}};
constexpr ImVec4 niagaraColor{0.57f,0.32f,0.18f,1};


std::filesystem::path RuneSchemaModsRoot() {
    return std::filesystem::path(PS::HostServices::WorkingDirectory()) / "Mods" / "RuneSchema" / "mods";
}

void RefreshLoadOrderEditor(LoadOrderEditorState& state, const std::filesystem::path& modsRoot,
    bool strictValues) {
    state.rows.clear();
    state.dirty = false;
    std::vector<std::string> discovered;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(modsRoot, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec)) continue;
        const auto name = it->path().filename().string();
        if (name != "RuneSchema") discovered.push_back(name);
    }
    std::sort(discovered.begin(), discovered.end());
    std::unordered_map<std::string, bool> enabled;
    std::vector<std::string> ordered;
    for (const auto& entry : DragonWilds::ModLoadOrder::Load(
        DragonWilds::ModLoadOrder::GetOrderPath(modsRoot), strictValues)) {
        const auto name = RC::to_string(entry.Name);
        if (!enabled.contains(name)) {
            enabled.emplace(name, entry.Enabled);
            ordered.push_back(name);
        }
    }
    for (const auto& name : discovered)
        if (!enabled.contains(name) && DragonWilds::ModOrderPolicy::ShouldAutoPersist(name)) {
            enabled.emplace(name, true);
            ordered.push_back(name);
        }
    std::erase_if(ordered, [&](const auto& name) {
        return !std::binary_search(discovered.begin(), discovered.end(), name);
    });
    for (const auto& name : ordered) {
        LoadOrderRow row{name, enabled.at(name)};
        const auto mod = modsRoot / name;
        for (size_t loader = 0; loader < std::size(KnownLoaders); ++loader) {
            const auto folder = mod / KnownLoaders[loader];
            if (!std::filesystem::is_directory(folder, ec)) { ec.clear(); continue; }
            row.loaders.push_back(loader);
            size_t inspected = 0;
            for (std::filesystem::directory_iterator files(folder, ec), end;
                !ec && files != end && inspected < 2048; files.increment(ec), ++inspected) {
                if (!files->is_regular_file(ec)) continue;
                const auto extension = files->path().extension().string();
                if (extension != ".json" && extension != ".jsonc") continue;
                const auto size = files->file_size(ec);
                if (ec || size > 1024 * 1024) { ec.clear(); continue; }
                std::ifstream input(files->path(), std::ios::binary);
                const std::string text((std::istreambuf_iterator<char>(input)), {});
                if (text.find("\"$Patch\"") != std::string::npos)
                    row.patches.push_back(name + "/" + KnownLoaders[loader] + "/" + files->path().filename().string());
            }
            ec.clear();
        }
        state.rows.push_back(std::move(row));
    }
    DragonWilds::ModOrderPolicy::Apply(state.rows, [](const auto& row) -> const auto& { return row.name; });
    state.loaded = true;
    state.status = ec ? "Load-order scan completed with filesystem warnings." : "Load order loaded from disk.";
}

bool BeginRuneSchemaTab(const char* label, TabRole role, const ImVec4* custom = nullptr,
    const ImVec4* labelColor = nullptr) {
    constexpr ImVec4 accents[]{
        {0.22f, 0.43f, 0.64f, 1.0f},
        {0.55f, 0.37f, 0.15f, 1.0f},
        {0.43f, 0.32f, 0.59f, 1.0f},
        {0.16f, 0.46f, 0.40f, 1.0f}
    };
    const auto accent = custom ? *custom : accents[static_cast<int>(role)];
    const auto shade = [&](float scale) {
        return ImVec4(accent.x * scale, accent.y * scale, accent.z * scale, 1.0f);
    };
    ImGui::PushStyleColor(ImGuiCol_Tab, shade(0.55f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, shade(1.1f));
    ImGui::PushStyleColor(ImGuiCol_TabSelected, accent);
    ImGui::PushStyleColor(ImGuiCol_TabSelectedOverline, shade(1.4f));
    ImGui::PushStyleColor(ImGuiCol_TabDimmed, shade(0.45f));
    ImGui::PushStyleColor(ImGuiCol_TabDimmedSelected, shade(0.8f));
    ImGui::PushStyleColor(ImGuiCol_TabDimmedSelectedOverline, accent);
    ImGui::PushStyleColor(ImGuiCol_Text,
        labelColor ? *labelColor : ImVec4(0.96f, 0.96f, 0.96f, 1.0f));
    const bool selected = ImGui::BeginTabItem(label);
    ImGui::PopStyleColor(8);
    if (selected) {
        const auto tabMin = ImGui::GetItemRectMin();
        const auto tabMax = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRect(
            ImVec2(tabMin.x + 1.0f, tabMin.y + 1.0f),
            ImVec2(tabMax.x - 1.0f, tabMax.y - 1.0f),
            ImGui::GetColorU32(accent), 3.0f, 0, 1.5f);
        const auto light = [&](float amount) {
            return ImVec4(accent.x + (1.0f - accent.x) * amount,
                accent.y + (1.0f - accent.y) * amount,
                accent.z + (1.0f - accent.z) * amount, 1.0f);
        };
        for (const auto color : {ImGuiCol_FrameBg, ImGuiCol_Button, ImGuiCol_Header})
            ImGui::PushStyleColor(color, shade(0.85f));
        for (const auto color : {ImGuiCol_FrameBgHovered, ImGuiCol_ButtonHovered, ImGuiCol_HeaderHovered})
            ImGui::PushStyleColor(color, accent);
        for (const auto color : {ImGuiCol_FrameBgActive, ImGuiCol_ButtonActive, ImGuiCol_HeaderActive})
            ImGui::PushStyleColor(color, shade(0.7f));
        ImGui::PushStyleColor(ImGuiCol_CheckMark, light(0.8f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, light(0.6f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, light(0.85f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, shade(0.85f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, accent);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, light(0.3f));
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, shade(0.65f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.96f, 0.96f, 1.0f));
    }
    return selected;
}

void EndRuneSchemaTab() {
    // Nested tabs restore their parent's colors; outer tabs restore the host theme.
    ImGui::PopStyleColor(17);
    ImGui::EndTabItem();
}

void RenderToolsWarning() {
    constexpr const char* warning = "Advanced tools: searches and dumps can stall the game and increase memory use. A game restart may be needed after a diagnostic session.";
    const ImVec4 red(1.0f, 0.38f, 0.38f, 1.0f);
    const auto position = ImGui::GetCursorScreenPos();
    const auto width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    ImGui::PushStyleColor(ImGuiCol_Text, red);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(warning);
    ImGui::PopTextWrapPos();
    // Thicken the existing font without modifying the host's font atlas.
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
        ImVec2(position.x + ImGui::GetFontSize() / 28.0f, position.y),
        ImGui::GetColorU32(red), warning, nullptr, width);
    ImGui::PopStyleColor();
}

}

class RuneSchema : public RC::CppUserModBase
{
public:
    RuneSchema() : CppUserModBase()
    {
        ModName = STR("RuneSchema");
        ModVersion = RC::to_generic_string(PS::BuildInfo::Version);
        ModDescription = STR("Allows modifying of DragonWilds's assets dynamically.");
        ModAuthors = STR("See RuneSchema Settings > Attribution");

        auto config = PS::PSConfig::Get();
        PS::StartupTrace::Mark("config load begin");
        config->Load();
        const auto pluginRoot=std::filesystem::path(PS::HostServices::WorkingDirectory())/"Mods"/"RuneSchema"/"plugins";
        m_pluginHost.Load(pluginRoot);
        for(const auto& message:m_pluginHost.Diagnostics())
            PS::Log<LogLevel::Normal>(TEXT("Plugin host: {}\n"),PS::ToWideSafe(message.c_str()));
        for(const auto& connection:m_pluginHost.Connections())
            PS::Log<LogLevel::Normal>(TEXT("Plugin connection enabled: {}\n"),PS::ToWideSafe(connection.c_str()));
        if(!m_pluginHost.HasCapability("bridge.registry"))
            PS::Log<LogLevel::Normal>(TEXT("RuneSchema.Networking is not active. Local loaders remain enabled.\n"));
        if(m_pluginHost.HasCapability("helpy.navigation"))try {
            const auto about=m_pluginHost.Call("RuneSchema.Core","helpy.about","{}");
            PS::Log<LogLevel::Normal>(TEXT("Helpy plugin service: {}\n"),PS::ToWideSafe(about.c_str()));
        } catch(const std::exception& error) {
            PS::Log<LogLevel::Warning>(TEXT("Helpy plugin service validation failed: {}\n"),PS::ToWideSafe(error.what()));
        }
        PS::UE4SSCompatibility::Report();
        // Storefront selection controls native signature lookup.
        const auto& storefront=PS::Storefront::CurrentDetection();
        RC::Output::send<RC::LogLevel::Normal>(TEXT("[RuneSchema] Runtime storefront: {} | reason: {} | package profile: Universal.\n"),
            PS::ToWideSafe(PS::Storefront::Name(storefront.Value)),storefront.Reason);
        RC::Output::send<RC::LogLevel::Normal>(TEXT("[RuneSchema] Native binding lane: {}.\n"),
            PS::ToWideSafe(PS::Storefront::NativeLaneName(PS::Storefront::CurrentNativeLane())));
        if(storefront.Value==PS::Storefront::Kind::GamePass) {
            const auto signatureState=storefront.HasGamePassSignatures?TEXT("available"):TEXT("missing");
            RC::Output::send<RC::LogLevel::Normal>(TEXT("[RuneSchema] Game Pass pivot: UE4SS_Signatures={} at {} | isolated WinGDK native lane selected.\n"),
                signatureState,storefront.SignatureRoot.wstring());
        }
        const auto& mapping=PS::MappingBackbone::Current(std::filesystem::path(PS::HostServices::WorkingDirectory()));
        if(mapping.Available)
            RC::Output::send<RC::LogLevel::Normal>(TEXT("[RuneSchema] Mapping backbone: {} | bytes={} | fingerprint={}.\n"),
                mapping.Path.wstring(),mapping.Size,PS::ToWideSafe(mapping.Fingerprint.c_str()));
        else
            RC::Output::send<RC::LogLevel::Normal>(TEXT("[RuneSchema] Mapping backbone: no Mappings.usmap found; live reflection remains authoritative.\n"));
        PS::StartupTrace::Mark("signature scan begin");

        DragonWilds::SignatureManager::Initialize();
        PS::StartupTrace::Mark("signature scan complete; offsets begin");

        DragonWilds::UnrealOffsets::Initialize();
        PS::StartupTrace::Mark("offsets complete; early hooks begin");

        MainLoader.PreInitialize();
        PS::StartupTrace::Mark("early hooks complete");

        RC::Output::send<RC::LogLevel::Normal>(STR("[RuneSchema] v{} loaded | UE4SS | network role determined by game state.\n"), ModVersion);
    }

    ~RuneSchema() override
    {
        m_pluginHost.Shutdown();
        m_networkRoleMonitor.Stop();
        PS::RuntimeJobs::Shutdown();
        if (m_apiExportCallbackId != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_apiExportCallbackId);
        if (m_toolsWorldCallbackId != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_toolsWorldCallbackId);
        PS::PlayerTrace::Cancel();
        PS::AppearanceTrace::Cancel();
        PS::NiagaraTest::Unbind();
    }

    auto on_ui_init() -> void override
    {
        PS::StartupTrace::Mark("UE4SS on_ui_init begin");
        if (!PS::HostServices::GuiEnabled())
        {
            PS::StartupTrace::Mark("UE4SS on_ui_init skipped: GUI unavailable");
            return;
        }

        PS::HostServices::InitializeGui();

        m_pluginHost.OnUiInit();

        register_tab(STR("RuneSchema"), [](CppUserModBase* instance) {
            auto mod = dynamic_cast<RuneSchema*>(instance);
            if (!mod)
            {
                return;
            }

            mod->render_settings();
        });

        PS::StartupTrace::Mark("UE4SS RuneSchema tab registered");

    }

    auto render_schema_generator() -> void
    {
        static bool loadedTables=false;
        static char schemaName[97]{};
        ImGui::SeparatorText("Configure export");
        ImGui::TextWrapped("Export structural JSON schemas for all 12 loader folders. These are authoring hints, not complete runtime validation or game-data dumps.");
        ImGui::BeginDisabled(m_schemaBusy.load());
        ImGui::InputText("Schema export name (optional)",schemaName,sizeof(schemaName));
        ImGui::Checkbox("Include loaded DataTable fields (slower; enter a world first)",&loadedTables);
        if (ImGui::Button("Export loader schemas"))
        {
            m_schemaBusy=true;
            m_includeLoadedTables=loadedTables;
            {std::lock_guard lock(m_schemaStatusMutex);m_schemaExportName=schemaName;}
            m_generateSchemas=true;
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Saved to runtime/live/jobs/exports/schema/<name>_schemas_<timestamp>. Linked schema filenames stay unchanged inside the bundle. Default export does not scan game objects.");
        {std::lock_guard lock(m_schemaStatusMutex);if(!m_schemaStatus.empty())ImGui::TextWrapped("%s",m_schemaStatus.c_str());}

        if (m_schemaBusy)
        {
            ImGui::ProgressBar(-0.5f * (float)ImGui::GetTime(), ImVec2(0.0f, 0.0f), "Generating...");
        }

    }

    auto render_settings() -> void
    {
        auto* config = PS::PSConfig::Get();
        auto& settings = config->GetMutableSettings();
        if (ImGui::Button("Save settings")) {
            if (config->Save())
                PS::Log<LogLevel::Normal>(STR("RuneSchema settings saved. Restart for startup settings.\n"));
        }
        if (!config->GetStatus().empty()) ImGui::TextWrapped("%s", config->GetStatus().c_str());
        ImGui::Separator();
        // The OS client area can be smaller than the host's reported window size.
        const auto clientBottom = ImGui::GetMainViewport()->Pos.y + ImGui::GetIO().DisplaySize.y;
        const auto visibleHeight = clientBottom - ImGui::GetCursorScreenPos().y
            - ImGui::GetStyle().WindowPadding.y - ImGui::GetFrameHeightWithSpacing();
        const auto viewportHeight = std::max(1.0f, std::min(ImGui::GetContentRegionAvail().y, visibleHeight));
        ImGui::BeginChild("SettingsViewport", ImVec2(0, viewportHeight), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        if (ImGui::BeginTabBar("RuneSchemaSettings")) {
        if (BeginRuneSchemaTab("General", TabRole::Settings)) {
        static int settingsPage = 0;
        ImGui::BeginChild("SettingsRail", ImVec2(125, 0), true);
        constexpr const char* settingsLabels[]{"General", "Runtime", "About"};
        for (int i = 0; i < 3; ++i) {
            const bool active = settingsPage == i;
            if (active) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            if (ImGui::Button(settingsLabels[i], ImVec2(-1, 0))) settingsPage = i;
            if (active) ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("SettingsContent", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGui::PushID("Settings");
        if (settingsPage == 0) {
            ImGui::SeparatorText("General RuneSchema behavior");
            ImGui::TextWrapped("Core settings that affect the whole RuneSchema runtime.");
            ImGui::SeparatorText("Messages");
            ImGui::Checkbox("Advanced logging", &settings.advancedLogging);
            ImGui::TextWrapped("Off: essential startup, compatibility, warnings and errors. On: detailed loader and bridge traces. This does not change UE4SS logging.");
            ImGui::Checkbox("Color-code loader annotations", &settings.colorCodeLoaderAnnotations);
            ImGui::TextWrapped("When enabled, /loader annotations in Server & Loaders > Installed Mods use the matching loader color.");
        } else if(settingsPage==1) {
            ImGui::SeparatorText("Runtime and reload");
            ImGui::Checkbox("Automatic JSON reload", &settings.enableAutoReload);
            ImGui::Checkbox("Mod authoring tools (restart required)", &settings.authoringTools);
            ImGui::TextWrapped("Enables presets, traces, inspection, exports, save cleanup, and guided loader authoring. It uses only the live job, export, and saved-result folders required by those tools.");
            ImGui::Checkbox("Advanced diagnostics (restart required)", &settings.advancedRuntime);
            ImGui::TextWrapped("Enables full catalog scans, diagnostic caches, and heavyweight troubleshooting. It is not required for normal authoring or gameplay.");
            ImGui::BeginDisabled(!settings.advancedRuntime);
            ImGui::Checkbox("Enable diagnostic jobs (restart required)", &settings.diagnosticJobs.enabled);
            ImGui::Checkbox("Character editor trace preset", &settings.diagnosticJobs.characterEditorPreset);
            ImGui::TextWrapped("Jobs run from settings/jobs recursively and do not require a player. Turning Advanced diagnostics off disables every job.");
            ImGui::EndDisabled();
            ImGui::SeparatorText("Journal and recipe persistence");
            ImGui::Checkbox("Save RuneSchema character customization", &settings.persistence.characterCustomization);
            ImGui::TextWrapped("Off by default. Character option and data-table loaders remain active, but automatic /players appearance assignments do not rewrite CustomizationSaveData.");
            ImGui::Checkbox("Save RuneSchema journal/lore unlocks", &settings.persistence.journal);
            ImGui::TextWrapped("Off by default. Journal and lore assets still load and are placed, but automatic player unlock delivery is skipped so RuneSchema does not add them to the save.");
            ImGui::Checkbox("Save RuneSchema recipe unlocks", &settings.persistence.recipes);
            ImGui::TextWrapped("Off by default. Recipes remain available for the current session through the game's non-persistent recipe set. Enable this only when permanent recipe unlocks are wanted.");
            ImGui::SeparatorText("Server Helpy permissions");
            ImGui::TextWrapped("These permissions are disabled by default. Allowed client requests are executed and validated by server authority; clients can never export files or create permanent placements through this bridge.");
            ImGui::Checkbox("Allow client item grants", &settings.helpyAuthority.allowClientItemGrants);
            ImGui::Checkbox("Allow client temporary spawns", &settings.helpyAuthority.allowClientTemporarySpawns);
            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputInt("Maximum items per request", &settings.helpyAuthority.maximumItemCount);
            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputInt("Maximum actors per request", &settings.helpyAuthority.maximumSpawnCount);
            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputInt("Maximum temporary NPC seconds", &settings.helpyAuthority.maximumNpcDurationSeconds);
            settings.helpyAuthority.maximumItemCount=std::clamp(settings.helpyAuthority.maximumItemCount,1,10000);
            settings.helpyAuthority.maximumSpawnCount=std::clamp(settings.helpyAuthority.maximumSpawnCount,1,100);
            settings.helpyAuthority.maximumNpcDurationSeconds=std::clamp(settings.helpyAuthority.maximumNpcDurationSeconds,1,3600);
            ImGui::TextWrapped("Remote users must also match permittedPlayerGuids or permittedPlayerNames in settings/settings.jsonc. An empty allowlist denies all remote Helpy mutations; GUIDs are preferred.");
            ImGui::TextWrapped("Plugin controls are owned by each plugin under plugins/<PluginId>/settings/. Helpy activation and its hotkey are managed by RuneSchema.Helpy.");
            ImGui::TextWrapped("Settings: settings/settings.jsonc | Live jobs: runtime/live/jobs | Live state: runtime/live/saved");
            ImGui::TextWrapped("Hotloading applies supported JSON changes while the game is running. Blueprint patches and ghost appearance changes still require a restart.");
            ImGui::SeparatorText("Load-order prefixes");
            ImGui::TextWrapped("AA_ loads first, then numeric prefixes such as 00_ and 01_ in ascending order, ordinary folders, and ZZ_ last. AA_ and ZZ_ folders are implied enabled and are omitted from generated runeschema.txt files unless you add an explicit override.");
        } else {
            ImGui::SeparatorText("About RuneSchema");
            ImGui::Text("RuneSchema %s", PS::BuildInfo::Version);
            ImGui::Text("Runtime: %s", PS::UE4SSCompatibility::DetectedBuild());
            ImGui::Text("Built against: %s", PS::BuildInfo::UE4SSTarget);
            ImGui::Text("Game target: %s", PS::BuildInfo::GameTarget);
            if (!PS::UE4SSCompatibility::MatchesTarget()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.18f, 1.0f));
                ImGui::TextWrapped("Runtime build differs from the tested API line. Unavailable capabilities are reported separately.");
                ImGui::PopStyleColor();
            }
            ImGui::SeparatorText("Attribution");
            ImGui::TextWrapped("Based on the original RuneSchema 0.6.0 from Snorkles. This version is maintained by members of the RSDW Modding Community.");
            ImGui::TextWrapped("PalSchema foundation — Okaetsu");
            ImGui::TextDisabled("Release history and detailed credits are included in the documentation.");
        }
        ImGui::PopID(); ImGui::EndChild(); EndRuneSchemaTab(); }
        if (BeginRuneSchemaTab("Server & Loaders", TabRole::Settings)) {
            static int loadPage = 1;
            ImGui::BeginChild("LoadNavigation", ImVec2(125, 0), true);
            constexpr const char* loadLabels[]{"Installed Mods", "Loader Controls", "Load Order"};
            for (int i = 0; i < 3; ++i) {
                const bool active = loadPage == i;
                if (active) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                if (ImGui::Button(loadLabels[i], ImVec2(-1, 0))) loadPage = i;
                if (active) ImGui::PopStyleColor();
            }
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("LoadWorkspace", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            if (loadPage == 0) {
                ImGui::SeparatorText("Installed mods");
                ImGui::TextWrapped("Server overview: each mod is annotated with the loader folders currently present beneath its mod directory. This page is read-only; use Load Order to control priority.");
                const auto modsRoot = RuneSchemaModsRoot();
                try {
                    for (const auto& entry : std::filesystem::directory_iterator(modsRoot)) {
                        if (!entry.is_directory() || entry.path().filename() == "RuneSchema") continue;
                        ImGui::PushID(entry.path().filename().string().c_str());
                        ImGui::SeparatorText(entry.path().filename().string().c_str());
                        bool found = false;
                        for (size_t loaderIndex = 0; loaderIndex < std::size(KnownLoaders); ++loaderIndex) {
                            const auto* loaderName = KnownLoaders[loaderIndex];
                            if (std::filesystem::exists(entry.path() / loaderName)) {
                                found = true;
                                ImGui::SameLine();
                                const auto color = settings.colorCodeLoaderAnnotations
                                    ? (loaderIndex < std::size(loaderColors) ? loaderColors[loaderIndex] : niagaraColor)
                                    : ImVec4(.75f, .75f, .75f, 1);
                                ImGui::TextColored(color, "/%s", loaderName);
                            }
                        }
                        if (!found) ImGui::TextDisabled("No recognized loader folders");
                        ImGui::PopID();
                    }
                } catch (const std::exception& error) {
                    ImGui::TextWrapped("Could not scan the Mods folder: %s", error.what());
                }
            } else if (loadPage == 1) {
            ImGui::SeparatorText("Loader controls and authoring map");
            ImGui::TextWrapped("Choose a loader on the left. Enable/disable controls affect startup; authoring guidance explains ownership and points to Authoring & Tools. Restart remains the acceptance boundary for server changes unless the loader explicitly supports reload.");
            bool* enabled[]{
                &settings.loaders.assets,
                &settings.loaders.blueprints,
                &settings.loaders.buildings,
                &settings.loaders.courses,
                &settings.loaders.dialogue,
                &settings.loaders.effects,
                &settings.loaders.enums,
                &settings.loaders.equipment,
                &settings.loaders.events,
                &settings.loaders.journal,
                &settings.loaders.lore,
                &settings.loaders.nameplates,
                &settings.loaders.niagara,
                &settings.loaders.npc,
                &settings.loaders.players,
                &settings.loaders.quests,
                &settings.loaders.raw,
                &settings.loaders.recipes,
                &settings.loaders.registry,
                &settings.loaders.spawns,
                &settings.loaders.strings,
                &settings.loaders.vendors};
            static_assert(std::size(enabled)==PS::LoaderCapabilities.size());
            static_assert(std::size(loaderColors)==PS::LoaderCapabilities.size());
            static size_t selectedLoader{};
            ImGui::BeginChild("LoaderRail",ImVec2(150,0),true);
            for(size_t i=0;i<PS::LoaderCapabilities.size();++i) {
                const auto& color=i<PS::LoaderCapabilities.size()?loaderColors[i]
                    :niagaraColor;
                ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(color.x*.72f,color.y*.72f,color.z*.72f,1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,color);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(color.x*.55f,color.y*.55f,color.z*.55f,1));
                const bool active=selectedLoader==i;
                if(active)ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,1,1,1));
                if(ImGui::Button(PS::LoaderCapabilities[i].DisplayName,ImVec2(-1,0)))selectedLoader=i;
                if(active)ImGui::PopStyleColor();
                ImGui::PopStyleColor(3);
            }
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("LoaderPage",ImVec2(0,0),false);
            if(selectedLoader<PS::LoaderCapabilities.size()) {
                for(size_t i=0;i<PS::LoaderCapabilities.size();++i) {
                    if(i!=selectedLoader)continue;
                    const auto& capability=PS::LoaderCapabilities[i];
                    {
                        const auto loaderColor=loaderColors[i];
                        const auto shadeLoader=[&](float scale) {
                            return ImVec4(loaderColor.x*scale,loaderColor.y*scale,loaderColor.z*scale,loaderColor.w);
                        };
                        ImGui::PushStyleColor(ImGuiCol_Header,shadeLoader(.72f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,loaderColor);
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive,shadeLoader(.58f));
                        ImGui::PushStyleColor(ImGuiCol_Button,shadeLoader(.72f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,loaderColor);
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive,shadeLoader(.58f));
                        ImGui::PushStyleColor(ImGuiCol_FrameBg,shadeLoader(.50f));
                        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,shadeLoader(.72f));
                        ImGui::PushID(capability.Name);
                        ImGui::BeginChild("LoaderContent",ImVec2(0,0),false,ImGuiWindowFlags_AlwaysVerticalScrollbar);
                        ImGui::TextColored(loaderColor, "/%s", capability.Name);
                        ImGui::SameLine();
                        ImGui::Text("loader");
                         ImGui::SeparatorText("Basic options");
                        ImGui::Checkbox("Enable loader (restart required)",enabled[i]);
                        const std::string_view loaderName=capability.Name;
                        if(ImGui::CollapsingHeader("Runtime options and capabilities",ImGuiTreeNodeFlags_DefaultOpen)) {
                            ImGui::TextWrapped("%s",capability.Purpose);
                            if(loaderName=="spawns") {
                                ImGui::Checkbox("Experimental spawn drop scaling", &settings.enableExperimentalDropScaling);
                                ImGui::Checkbox("Allow native roaming", &settings.spawnBehavior.enableNativeRoaming);
                                ImGui::InputDouble("Default roam radius", &settings.spawnBehavior.defaultRoamRadius, 50.0, 250.0, "%.0f");
                                ImGui::InputDouble("Default vertical tolerance", &settings.spawnBehavior.defaultRoamMaxZTolerance, 25.0, 100.0, "%.0f");
                                ImGui::TextWrapped("Roaming defaults apply when omitted from a spawn definition. Live spawn changes are rejected once that mod has a live spawn; restart to apply them. No temporary summon/unload tool is available.");
                            }
                            if(loaderName=="recipes" || loaderName=="journal" || loaderName=="lore")ImGui::TextWrapped("Automatic reload can apply definitions and unlocks. This is not a temporary preview; saved progression may remain after removing a file.");
                            if(loaderName=="raw")ImGui::TextWrapped("Automatic reload edits shared DataTables. Removing a test file does not provide a transactional rollback.");
                            if(loaderName=="assets")ImGui::TextWrapped("Cloned items may be referenced by inventory or saves. No temporary item unload is available.");
                            if(loaderName=="players")ImGui::TextWrapped("Player runtime also requires the spawns loader. No general temporary player-rule rollback is available.");
                            if(loaderName=="npc")ImGui::TextWrapped("AI visuals and human previews share persistent identity. VendorID opens a store; DialogueID starts Talk. Human Pose presets support held poses, looping idles and one-shot emotes. Full-body idles do not preserve every weapon grip.");
                            if(loaderName=="dialogue")ImGui::TextWrapped("Experimental native dialogue for the local authoritative player. Next/End choices, one-time Completion rewards, and per-character store gates. Interrupted grants remain pending to prevent duplicates. Generic take-item actions are not enabled. Requires NPCs; restart after changes.");
                            if(loaderName=="vendors")ImGui::TextWrapped("Requires NPCs and Recipes. Bind stores through NPC VendorID; recipe VendorID adds offers. Banner override is optional; the Death banner is the default.");
                            if(loaderName=="vendors")ImGui::TextDisabled("Optional vendor reports: NPCs > Advanced diagnostics.");
                            ImGui::TextWrapped("%s",PS::LoaderTemplate::Help(capability.Name).c_str());
                             ImGui::Text("Starter formats:");
                             ImGui::SameLine();
                             ImGui::TextColored(loaderColor, "Basic");
                             ImGui::SameLine();
                             ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.20f, 1.0f), "Fields reference");
                             if (capability.StarterPatch) {
                                 ImGui::SameLine();
                                 ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "$Patch");
                             }
                             if (capability.Clone) {
                                 ImGui::SameLine();
                                 ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "$Clone");
                             }
                            ImGui::TextWrapped("Array append: %s.", capability.Append ? "supported" : "not supported");
                            ImGui::TextWrapped("Starter formats are not the full runtime contract. Buildings and players can patch authored definitions even without live-record patch starters.");
                        }
                        if(ImGui::CollapsingHeader("Authoring flow",ImGuiTreeNodeFlags_DefaultOpen)) {
                            ImGui::TextWrapped("Open Authoring & Tools > Mod Authoring > All Loaders to search this loader, inspect its schema, choose a supported starter format, and export editable JSONC. Export does not install or activate a mod.");
                            ImGui::Text("Patch vanilla: %s",capability.StarterPatch?"starter available":"loader-specific or unavailable");
                            ImGui::Text("Clone: %s",capability.Clone?"supported":"not supported");
                            ImGui::Text("Create authored definition: supported where this loader owns a stable identity");
                            if(loaderName=="players") {
                                ImGui::SeparatorText("Player authoring coverage");
                                ImGui::TextWrapped("Direct page: Authoring & Tools > Mod Authoring > Players. Implemented fields include name/GUID/all-player/load-slot targeting; scale; health and stamina; movement and carry weight; attack, defense and named attributes; appearance rows with fallbacks; visual effects; archetype; and inline or reusable nameplates.");
                                ImGui::TextWrapped("Use a stable Id for later $Patch operations. Inventory and quest/journal progress remain with their owning loaders.");
                            }
                            if(loaderName=="nameplates") {
                                ImGui::SeparatorText("Nameplate relationships");
                                ImGui::TextWrapped("Direct page: Authoring & Tools > Mod Authoring > Nameplates. /players selects a reusable definition with Nameplate.Definition and may override Mode, Icon, Scale, Distance, audience, states, events, SkillXP and timeout.");
                                ImGui::TextWrapped("Each current activity state owns a cooked icon. Event rules Pulse, Activate or Deactivate that state. Displayed counters require the next runtime phase and are not serialized by this checkpoint.");
                            }
                        }
                        if(loaderName=="npc" && ImGui::CollapsingHeader("Advanced diagnostics")) {
                            ImGui::Checkbox("Export NPC/vendor status", &settings.npcDiagnostics.statusExport);
                            ImGui::Checkbox("Export vendor interaction trace", &settings.npcDiagnostics.interactionTraceExport);
                            ImGui::TextWrapped("Off by default. These reports are not required for NPCs or shops. Status writes on setup/interaction events; the trace records at most 64 unique event keys per world. Changes apply to subsequent events; save settings to retain them.");
                            ImGui::TextDisabled("Output: runtime/live/jobs/exports/VendorStatus.json and VendorInteractionTrace.json.");
                            ImGui::TextWrapped("Turning a diagnostic off stops future writes; existing files are not automatically deleted. Manual preset and trace exports remain separate.");
                        }
                        ImGui::EndChild();ImGui::PopID();ImGui::PopStyleColor(8);
                    }
                }
            }
            ImGui::EndChild(); // LoaderPage
            }
        if (loadPage == 2) {
        ImGui::BeginChild("Load orderContent", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGui::PushID("Load order");
        static LoadOrderEditorState editor;
        const auto modsRoot = RuneSchemaModsRoot();
        ImGui::SeparatorText("runeschema.txt load order");
        ImGui::Checkbox("Use runeschema.txt", &settings.loadOrder.enabled);
        ImGui::Checkbox("Create the file automatically", &settings.loadOrder.autoCreate);
        if(ImGui::CollapsingHeader("Advanced file handling")) {
        ImGui::Checkbox("Add new folders and remove missing folders", &settings.loadOrder.reconcileFolders);
        ImGui::Checkbox("Preserve comments when reconciling", &settings.loadOrder.preserveComments);
        ImGui::Checkbox("Only accept 0 or 1", &settings.loadOrder.strictValues);
        ImGui::Checkbox("Sort folders when no order file is used", &settings.loadOrder.deterministicFallback);
        }
         ImGui::TextWrapped("Folder order is top-to-bottom. Set a mod to 0 to disable it. Directive documents are queued and applied after ordinary definitions.");
         ImGui::Text("Directive syntax:");
         ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "$Patch");
         ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "$Target");
        ImGui::TextWrapped("Priority tiers: AA_ first; 00_, 01_, and other numeric prefixes ascending; ordinary folders; ZZ_ last. AA_ and ZZ_ folders are enabled implicitly and stay out of the generated file. Add one to runeschema.txt manually with 0 to disable it or 1 to keep an explicit enable override. Changes take effect on the next game load.");
        if (ImGui::Button("Reload from disk") || !editor.loaded)
            RefreshLoadOrderEditor(editor, modsRoot, settings.loadOrder.strictValues);
        ImGui::SameLine();
        if (ImGui::Button("Save load order")) {
            DragonWilds::ModOrderPolicy::Apply(editor.rows,
                [](const auto& row) -> const auto& { return row.name; });
            std::vector<DragonWilds::ModOrderEntry> entries;
            entries.reserve(editor.rows.size());
            for (const auto& row : editor.rows)
                entries.push_back({PS::ToWideSafe(row.name.c_str()), row.enabled});
            const auto path = DragonWilds::ModLoadOrder::GetOrderPath(modsRoot);
            const bool saved = settings.loadOrder.preserveComments && std::filesystem::exists(path)
                ? DragonWilds::ModLoadOrder::SavePreservingComments(path, entries)
                : DragonWilds::ModLoadOrder::Save(path, entries);
            editor.status = saved ? "Saved runeschema.txt. The new order takes effect on the next game load."
                                  : "Could not save runeschema.txt; see the RuneSchema log.";
            editor.dirty = !saved;
        }
        if (!editor.status.empty()) ImGui::TextWrapped("%s", editor.status.c_str());
        ImGui::SeparatorText("Mods");
        for (size_t index = 0; index < editor.rows.size(); ++index) {
            auto& row = editor.rows[index];
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::Checkbox("##enabled", &row.enabled)) editor.dirty = true;
            ImGui::SameLine();
            ImGui::TextUnformatted(row.name.c_str());
            for (const auto loader : row.loaders) {
                ImGui::SameLine();
                const auto color = settings.colorCodeLoaderAnnotations
                    ? loaderColors[loader] : ImVec4(.75f, .75f, .75f, 1);
                ImGui::TextColored(color, "/%s", KnownLoaders[loader]);
            }
            ImGui::SameLine();
            const auto priority = DragonWilds::ModOrderPolicy::Priority(row.name);
            const bool canUp = index > 0
                && DragonWilds::ModOrderPolicy::Priority(editor.rows[index - 1].name) == priority;
            ImGui::BeginDisabled(!canUp);
            if (ImGui::SmallButton("Up")) { std::swap(editor.rows[index], editor.rows[index - 1]); editor.dirty = true; }
            ImGui::EndDisabled(); ImGui::SameLine();
            const bool canDown = index + 1 < editor.rows.size()
                && DragonWilds::ModOrderPolicy::Priority(editor.rows[index + 1].name) == priority;
            ImGui::BeginDisabled(!canDown);
            if (ImGui::SmallButton("Down")) { std::swap(editor.rows[index], editor.rows[index + 1]); editor.dirty = true; }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        if (editor.dirty) ImGui::TextColored(ImVec4(1.0f,.72f,.20f,1.0f), "Unsaved load-order changes");
        ImGui::SeparatorText("Patch documents (always applied last)");
        bool anyPatches = false;
        for (const auto& row : editor.rows) for (const auto& patch : row.patches) {
            anyPatches = true;
            ImGui::TextColored(ImVec4(.95f,.24f,.24f,1.0f), "%s", patch.c_str());
        }
        if (!anyPatches) ImGui::TextDisabled("No top-level $Patch documents found in recognized loader folders.");

        ImGui::PopID(); ImGui::EndChild(); }
        ImGui::EndChild();
        EndRuneSchemaTab(); }
        if (BeginRuneSchemaTab("Authoring & Tools", TabRole::Diagnostics)) {
            RenderToolsWarning();
            if(!settings.authoringTools)
                ImGui::TextWrapped("Mod authoring tools are disabled. Enable them under General > Runtime and restart. The interface remains visible so the setting never appears to remove functionality.");
            if (m_apiExportCallbackId == Hook::ERROR_ID) {
                ImGui::TextWrapped("Tools are off. Activation lasts only for this game session and is not saved.");
                ImGui::BeginDisabled(!m_unrealReady.load()||!settings.authoringTools);
                if (ImGui::Button("Activate Tools for this session")) ActivateTools();
                ImGui::EndDisabled();
            }
            if (m_apiExportCallbackId != Hook::ERROR_ID) {
            ImGui::TextWrapped("Author or repair content first; use presets/traces for contracts, inspect when narrowing a target, then review results. Save cleanup is offline and last.");
            constexpr const char* names[]{"Mod Authoring", "Presets", "Traces", "Inspector", "Results", "Save Cleanup"};
            constexpr PS::InspectionTools::Section sections[]{
                PS::InspectionTools::Section::Presets,
                PS::InspectionTools::Section::Presets,
                PS::InspectionTools::Section::Traces, PS::InspectionTools::Section::Inspector,
                PS::InspectionTools::Section::Results};
            constexpr ImVec4 toolColors[]{
                {0.55f, 0.37f, 0.15f, 1.0f},
                {0.22f, 0.43f, 0.64f, 1.0f},
                {0.43f, 0.32f, 0.59f, 1.0f},
                {0.16f, 0.46f, 0.40f, 1.0f},
                {0.53f, 0.32f, 0.20f, 1.0f},
                {0.36f, 0.46f, 0.23f, 1.0f}};
            static int selectedTool = 0;
            ImGui::BeginChild("AdvancedToolsBody", ImVec2(0, -180), false);
            ImGui::BeginChild("AdvancedToolsRail", ImVec2(145, 0), true);
            for (int i = 0; i < 6; ++i) {
                const auto color = toolColors[i];
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * .72f, color.y * .72f, color.z * .72f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x * .55f, color.y * .55f, color.z * .55f, 1));
                const bool active = selectedTool == i;
                if (active) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                if (ImGui::Button(names[i], ImVec2(-1, 0))) selectedTool = i;
                if (active) ImGui::PopStyleColor();
                ImGui::PopStyleColor(3);
            }
            ImGui::EndChild();
            ImGui::SameLine();
            const auto accent = toolColors[selectedTool];
            const auto shade = [&](float scale) {
                return ImVec4(accent.x * scale, accent.y * scale, accent.z * scale, 1.0f);
            };
            ImGui::PushStyleColor(ImGuiCol_Header, shade(.72f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, accent);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, shade(.58f));
            ImGui::PushStyleColor(ImGuiCol_Button, shade(.72f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, shade(.58f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, shade(.50f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, shade(.72f));
            ImGui::BeginChild("AdvancedToolsContent", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            ImGui::PushID(names[selectedTool]);
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
            if(selectedTool==5)PS::SaveViewer::RenderCleanup();
            else if(selectedTool==0) {
                static int authoringPage=0;
                constexpr const char* authoringNames[]{"All Loaders","Players","Nameplates","Items","Recipes","Journal","Quests","NPCs","Spawns","Events","Session Cleanup"};
                constexpr const char* authoringLoaders[]{nullptr,"players","nameplates","assets","recipes","journal","quests","npc","spawns","events",nullptr};
                ImGui::TextWrapped("Create and export definitions for every RuneSchema loader. Focused pages expose the most common relationships; All Loaders provides the complete 22-loader selector.");
                ImGui::BeginChild("ModAuthoringRail",ImVec2(135,0),true);
                for(int i=0;i<11;++i) {
                    const bool active=authoringPage==i;
                    if(active)ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,1,1,1));
                    if(ImGui::Button(authoringNames[i],ImVec2(-1,0)))authoringPage=i;
                    if(active)ImGui::PopStyleColor();
                }
                ImGui::EndChild();ImGui::SameLine();
                ImGui::BeginChild("ModAuthoringContent",ImVec2(0,0),false,ImGuiWindowFlags_AlwaysVerticalScrollbar);
                ImGui::PushID(authoringNames[authoringPage]);
                if(authoringPage==0) {
                    ImGui::SeparatorText("All loader authoring");
                    ImGui::TextWrapped("Choose any loader, search its loaded or authored records, select Patch/Create/Clone where supported, inspect its structural schema, and export an editable JSONC starter.");
                } else if(authoringPage==1) {
                    ImGui::SeparatorText("Player rule authoring");
                    ImGui::TextWrapped("Player rules can target names, GUIDs, all players, or load slots. The schema exposes implemented appearance, vital, movement, capacity, attack, defense, named attribute, visual-effect, archetype, and nameplate fields. Rules may patch another authored rule by stable Id.");
                    ImGui::TextWrapped("Inventory, quest progress, and journal progress remain with their owning loaders; this page does not perform unrestricted save editing.");
                    PS::InspectionTools::RenderPlayerNameplateBuilder(false);
                    ImGui::SeparatorText("Search, patch, and field reference");
                } else if(authoringPage==2) {
                    ImGui::SeparatorText("Reusable nameplate authoring");
                    ImGui::TextWrapped("Create a reusable /nameplates definition, then select it from /players with Nameplate.Definition. Inline player fields override the shared definition. States associate cooked icons with event pulses, activation/deactivation, skill XP, or a GameplayEffect condition.");
                    ImGui::TextWrapped("Conditions may use built-in activity and skill signals, GameplayEffect state, or exact native/Blueprint event paths with parameter comparisons. This covers quest, event, vendor, recipe, building, gathering, combat, spell, fishing, and emote sources without polling save data.");
                    PS::InspectionTools::RenderPlayerNameplateBuilder(true);
                    ImGui::SeparatorText("Search, patch, and field reference");
                } else if(authoringPage==3) {
                    ImGui::SeparatorText("Item definition authoring");
                    ImGui::TextWrapped("Use Helpy > Items > Item Lab for the guided in-world flow. This advanced page searches loaded item assets and builds reflected $Patch or $Clone starters for exact field review.");
                } else if(authoringPage==4) {
                    ImGui::SeparatorText("Recipe creation and alteration");
                    ImGui::TextWrapped("Search installed authored recipes, create a separate recipe identity, or patch an existing authored definition. Item identity remains in /assets; station placement and vendor contributions remain explicit recipe relationships.");
                } else if(authoringPage==5) {
                    ImGui::SeparatorText("Journal entry creation and alteration");
                    ImGui::TextWrapped("Search journal definitions, review pages, images, and unlock placement, then export a create or patch starter. Helpy can create an item-linked entry; this page handles independent and existing journal records.");
                } else if(authoringPage==6) {
                    ImGui::SeparatorText("Quest status and debug");
                    PS::QuestStatusPanel::Render();
                    ImGui::SeparatorText("Quest definition authoring");
                } else if(authoringPage==7) {
                    ImGui::SeparatorText("Character save import");
                    PS::SaveViewer::RenderNpcExport();
                    ImGui::SeparatorText("NPC definition authoring");
                } else if(authoringPage==8) {
                    ImGui::SeparatorText("Live spawn placement");
                    PS::SpawnToolsPanel::Render();
                    ImGui::SeparatorText("Spawn definition authoring");
                } else if(authoringPage==10) {
                    PS::SpawnToolsPanel::RenderSessionCleanup();
                }
                if(authoringPage!=10)PS::InspectionTools::Render(true,PS::InspectionTools::Section::AssetTemplates,authoringLoaders[authoringPage]);
                ImGui::PopID();ImGui::EndChild();
            }
            else PS::InspectionTools::Render(true, sections[selectedTool]);
            ImGui::PopItemWidth();
            ImGui::PopID();
            ImGui::EndChild();
            ImGui::PopStyleColor(8);
            ImGui::EndChild();
            PS::InspectionTools::RenderHistory();
            }
            EndRuneSchemaTab();
        }
        ImGui::EndTabBar();
        }
        ImGui::EndChild(); // SettingsViewport
    }

    auto on_unreal_init() -> void override
    {
        PS::StartupTrace::Mark("UE4SS on_unreal_init begin");
        MainLoader.Initialize();
        m_pluginHost.OnUnrealInit();
        PS::StartupTrace::Mark("UE4SS on_unreal_init complete");
        m_unrealReady = true;
        m_networkRoleMonitor.Start();
        PS::RuntimeJobs::Initialize();
    }

    void ActivateTools()
    {
        if (m_apiExportCallbackId != Hook::ERROR_ID || !m_unrealReady.load()) return;
        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("AuthoringToolsWorldTeardown");
        m_toolsWorldCallbackId = Hook::RegisterInitGameStatePreCallback(
            [this](Hook::TCallbackIterationData<void>&, AGameModeBase*) {
                m_generateSchemas=false;
                m_schemaBusy=false;
                {std::lock_guard lock(m_schemaStatusMutex);m_schemaStatus.clear();}
                PS::InspectionTools::Reset();
            }, options);
        if (m_toolsWorldCallbackId == Hook::ERROR_ID) return;
        options.HookName = TEXT("ManualAuthoringTools");
        m_apiExportCallbackId = Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>&, UEngine*, float, bool) {
                if (m_generateSchemas.exchange(false)) {
                    std::string result;
                    try {std::string name;{std::lock_guard lock(m_schemaStatusMutex);name=m_schemaExportName;}result="Exported: "+PS::JsonSchemaGenerator::GenerateSchemaFiles(m_includeLoadedTables.load(),name);}
                    catch(const std::exception& error){result=std::string("Schema export failed: ")+error.what();}
                    {std::lock_guard lock(m_schemaStatusMutex);m_schemaStatus=std::move(result);}
                    m_schemaBusy=false;
                }
                PS::InspectionTools::Tick();
            }, options);
        if (m_apiExportCallbackId == Hook::ERROR_ID) {
            Hook::UnregisterCallback(m_toolsWorldCallbackId);
            m_toolsWorldCallbackId=Hook::ERROR_ID;
        }
    }

private:
    PS::Network::RoleMonitor m_networkRoleMonitor;
    DragonWilds::DragonWildsMainLoader MainLoader;
    PS::PluginHost m_pluginHost;
    std::atomic<bool> m_generateSchemas = false;
    std::atomic<bool> m_schemaBusy = false;
    std::atomic<bool> m_includeLoadedTables = false;
    std::mutex m_schemaStatusMutex;
    std::string m_schemaStatus;
    std::string m_schemaExportName;
    std::atomic<bool> m_unrealReady = false;
    Hook::GlobalCallbackId m_apiExportCallbackId = Hook::ERROR_ID;
        Hook::GlobalCallbackId m_toolsWorldCallbackId = Hook::ERROR_ID;
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
