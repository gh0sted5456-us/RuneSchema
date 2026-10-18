#pragma once
#include <filesystem>

namespace PS::HostServices {
    std::filesystem::path WorkingDirectory();
    std::filesystem::path ModDirectory();
    std::filesystem::path SettingsDirectory();
    // Helpy-owned writable state. Game save/loader paths remain host-owned.
    std::filesystem::path StateDirectory();
    std::filesystem::path SavedDirectory();
    std::filesystem::path CacheDirectory();
    std::filesystem::path ProgressDirectory();
    std::filesystem::path ReferencesDirectory();
    std::filesystem::path ExportsDirectory();
    std::filesystem::path SearchesDirectory();
    std::filesystem::path PresetsDirectory();
    std::filesystem::path TraceProfilesDirectory();
    std::filesystem::path JobsDirectory();
    std::filesystem::path RuntimeDirectory();
}
