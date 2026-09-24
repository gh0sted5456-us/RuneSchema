#include "Runtime/HostServices.h"
#include "UE4SSProgram.hpp"
#include "Runtime/Layout.h"
#include <Windows.h>
#include <stdexcept>
#include <vector>

namespace PS::HostServices {
    namespace {
        std::filesystem::path LocalAppDataDirectory() {
            const auto required = ::GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
            if (!required) throw std::runtime_error("LOCALAPPDATA is unavailable");
            std::vector<wchar_t> value(required);
            if (::GetEnvironmentVariableW(L"LOCALAPPDATA", value.data(), required) + 1 != required)
                throw std::runtime_error("LOCALAPPDATA changed while it was read");
            return std::filesystem::path(value.data());
        }

        void MigrateSafeSaveLedger(const std::filesystem::path& modDirectory,
            const std::filesystem::path& stateDirectory) {
            const auto destination = stateDirectory / "safesave" / "OwnedContentLedger.json";
            if (std::filesystem::exists(destination)) return;
            const std::filesystem::path candidates[] = {
                modDirectory / "settings" / "safesave" / "OwnedContentLedger.json",
                modDirectory / "settings" / "OwnedContentLedger.json",
            };
            for (const auto& source : candidates) {
                if (!std::filesystem::is_regular_file(source)) continue;
                std::filesystem::create_directories(destination.parent_path());
                auto temporary = destination;
                temporary += ".migrating";
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
                std::filesystem::copy_file(source, temporary,
                    std::filesystem::copy_options::none);
                if (std::filesystem::file_size(source) != std::filesystem::file_size(temporary)) {
                    std::filesystem::remove(temporary, ignored);
                    throw std::runtime_error("SafeSave ledger migration verification failed");
                }
                std::filesystem::rename(temporary, destination);
                std::filesystem::remove(source, ignored);
                return;
            }
        }
    }
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
    std::filesystem::path StateDirectory() {
        return LocalAppDataDirectory() / L"RSDragonwilds" / L"Saved" / L"RuneSchema";
    }
    std::filesystem::path SavedDirectory() { return RuntimeDirectory() / "saved"; }
    std::filesystem::path CacheDirectory() { return SavedDirectory() / "cache"; }
    std::filesystem::path ProgressDirectory() { return SavedDirectory() / "progress"; }
    std::filesystem::path ReferencesDirectory() { return SavedDirectory() / "references"; }
    std::filesystem::path ExportsDirectory() { return JobsDirectory() / "exports"; }
    std::filesystem::path SearchesDirectory() { return JobsDirectory() / "searches"; }
    std::filesystem::path PresetsDirectory() { return SearchesDirectory() / "presets"; }
    std::filesystem::path TraceProfilesDirectory() { return SearchesDirectory() / "trace-profiles"; }
    std::filesystem::path JobsDirectory() { return RuntimeDirectory() / "jobs"; }
    void MigrateLegacyLayout() {
        RuntimeLayout::Migrate(ModDirectory());
        MigrateSafeSaveLedger(ModDirectory(), StateDirectory());
    }
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
