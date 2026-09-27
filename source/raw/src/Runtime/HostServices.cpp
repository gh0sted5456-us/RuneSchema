#include "Runtime/HostServices.h"
#include "Runtime/Storefront.h"
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

        std::wstring PackageFamilyName() {
            using GetCurrentPackageFamilyNameFn = LONG(WINAPI*)(UINT32*, PWSTR);
            const auto kernel32 = ::GetModuleHandleW(L"kernel32.dll");
            const auto getFamilyName = kernel32
                ? reinterpret_cast<GetCurrentPackageFamilyNameFn>(
                    ::GetProcAddress(kernel32, "GetCurrentPackageFamilyName"))
                : nullptr;
            if (!getFamilyName) return {};
            UINT32 length = 0;
            if (getFamilyName(&length, nullptr) != ERROR_INSUFFICIENT_BUFFER || length <= 1)
                return {};
            std::vector<wchar_t> value(length);
            if (getFamilyName(&length, value.data()) != ERROR_SUCCESS) return {};
            return std::wstring(value.data());
        }

        std::filesystem::path LegacyStateDirectory() {
            return LocalAppDataDirectory() / L"RSDragonwilds" / L"Saved" / L"RuneSchema";
        }

        std::filesystem::path PackageStateDirectory() {
            if (Storefront::Current() != Storefront::Kind::GamePass) return {};
            const auto family = PackageFamilyName();
            if (family.empty()) return {};
            // Xbox-managed saves live under SystemAppData\wgs. RuneSchema does
            // not edit that provider database directly; native adapters clean
            // provider payloads in-game.
            return LocalAppDataDirectory() / L"Packages" / family / L"LocalState"
                / L"RSDragonwilds" / L"Saved" / L"RuneSchema";
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
        const auto package = PackageStateDirectory();
        return package.empty() ? LegacyStateDirectory() : package;
    }
    std::filesystem::path XboxSaveRoot() {
        if (Storefront::Current() != Storefront::Kind::GamePass) return {};
        const auto family = PackageFamilyName();
        if (family.empty()) return {};
        return LocalAppDataDirectory() / L"Packages" / family
            / L"SystemAppData" / L"wgs";
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
