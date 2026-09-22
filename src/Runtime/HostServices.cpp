#include "Runtime/HostServices.h"
#include "UE4SSProgram.hpp"
#include "Runtime/Layout.h"

namespace PS::HostServices {
    std::filesystem::path WorkingDirectory() {
        return RC::UE4SSProgram::get_program().get_working_directory();
    }
    std::filesystem::path RuntimeDirectory() {
        const auto path = ModDirectory() / "runtime" / "live";
        std::error_code error;
        std::filesystem::create_directories(path, error);
        return path;
    }
    std::filesystem::path ModDirectory() { return WorkingDirectory() / "Mods" / "RuneSchema"; }
    std::filesystem::path SettingsDirectory() { return ModDirectory() / "settings"; }
    std::filesystem::path SavedDirectory() { return RuntimeDirectory() / "saved"; }
    std::filesystem::path CacheDirectory() { return SavedDirectory() / "cache"; }
    std::filesystem::path ProgressDirectory() { return SavedDirectory() / "progress"; }
    std::filesystem::path ReferencesDirectory() { return SavedDirectory() / "references"; }
    std::filesystem::path ExportsDirectory() { return JobsDirectory() / "exports"; }
    std::filesystem::path SearchesDirectory() { return JobsDirectory() / "searches"; }
    std::filesystem::path PresetsDirectory() { return SearchesDirectory() / "presets"; }
    std::filesystem::path TraceProfilesDirectory() { return SearchesDirectory() / "trace-profiles"; }
    std::filesystem::path JobsDirectory() { return RuntimeDirectory() / "jobs"; }
    void MigrateLegacyLayout() { RuntimeLayout::Migrate(ModDirectory()); }
    bool GuiEnabled() {
        // on_ui_init is the host's GUI capability boundary. Reading the
        // settings-manager object here coupled RuneSchema to a private UE4SS
        // layout that changed when DebugConsoleEnabled became GuiConsoleEnabled.
        return true;
    }
    void InitializeGui() {
        using RC::UE4SSProgram;
        UE4SS_ENABLE_IMGUI()
    }
}
