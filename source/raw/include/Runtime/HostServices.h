#pragma once
#include <filesystem>

namespace PS::HostServices {
    std::filesystem::path WorkingDirectory();
    std::filesystem::path ModDirectory();
    std::filesystem::path SettingsDirectory();
    // Mutable compatibility state belongs beside the game's Saved data, not
    // inside the installed UE4SS mod. This root is shared by SafeSave and the
    // per-world building manifests.
    std::filesystem::path StateDirectory();
    // Provider-owned Xbox Game Save root. This is exposed for diagnostics and
    // format validation only; runtime cleanup must flow through the active
    // save provider rather than rewriting containers.index in-process.
    std::filesystem::path XboxSaveRoot();
    std::filesystem::path SavedDirectory();
    std::filesystem::path CacheDirectory();
    std::filesystem::path ProgressDirectory();
    std::filesystem::path ReferencesDirectory();
    std::filesystem::path ExportsDirectory();
    std::filesystem::path SearchesDirectory();
    std::filesystem::path PresetsDirectory();
    std::filesystem::path TraceProfilesDirectory();
    std::filesystem::path JobsDirectory();
    void MigrateLegacyLayout();
    std::filesystem::path RuntimeDirectory();
    bool GuiEnabled();
    void InitializeGui();
}
