#pragma once

#include <Windows.h>
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string>

namespace PS::Storefront {
    enum class Kind { SteamGog, GamePass, Unknown };

    struct Detection {
        Kind Value{Kind::Unknown};
        std::wstring Reason{L"no storefront evidence"};
        std::filesystem::path Executable;
        std::filesystem::path Ue4ssRoot;
        std::filesystem::path SignatureRoot;
        bool HasPackageIdentity{};
        bool HasGamePassSignatures{};
    };

    inline std::wstring Lower(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), towlower);
        return value;
    }

    inline bool HasPackageIdentity() noexcept
    {
        using GetCurrentPackageFullNameFn = LONG(WINAPI*)(UINT32*, PWSTR);
        const HMODULE kernel32 = ::GetModuleHandleW(L"kernel32.dll");
        if (!kernel32) return false;

        const auto get_current_package_full_name =
            reinterpret_cast<GetCurrentPackageFullNameFn>(
                ::GetProcAddress(kernel32, "GetCurrentPackageFullName"));
        if (!get_current_package_full_name) return false;

        UINT32 length = 0;
        const auto result = get_current_package_full_name(&length, nullptr);
        return result == ERROR_INSUFFICIENT_BUFFER && length > 1;
    }

    inline Detection DetectFrom(const std::filesystem::path& executable, bool packageIdentity)
    {
        Detection result;
        result.Executable = executable;
        result.HasPackageIdentity = packageIdentity;
        const auto directory = executable.parent_path();
        result.Ue4ssRoot = directory / L"ue4ss";
        result.SignatureRoot = result.Ue4ssRoot / L"UE4SS_Signatures";
        std::error_code error;
        result.HasGamePassSignatures = std::filesystem::is_regular_file(
            result.SignatureRoot / L"StaticConstructObject.lua", error);

        const auto path = Lower(executable.wstring());
        if (path.find(L"\\wingdk\\") != std::wstring::npos
            || path.find(L"-wingdk-") != std::wstring::npos
            || path.find(L"windowsapps") != std::wstring::npos
            || path.find(L"microsoftgame") != std::wstring::npos) {
            result.Value = Kind::GamePass;
            result.Reason = L"WinGDK/Windows package executable path";
        } else if (packageIdentity) {
            result.Value = Kind::GamePass;
            result.Reason = L"Windows package identity";
        } else if (path.find(L"\\win64\\") != std::wstring::npos
            || path.find(L"-win64-") != std::wstring::npos
            || path.find(L"steam") != std::wstring::npos
            || path.find(L"gog") != std::wstring::npos) {
            result.Value = Kind::SteamGog;
            result.Reason = L"Win64/Steam/GOG executable path";
        } else if (result.HasGamePassSignatures) {
            result.Value = Kind::GamePass;
            result.Reason = L"UE4SS_Signatures/StaticConstructObject.lua fallback";
        }
        return result;
    }

    inline Detection Detect() noexcept
    {
        try {
            wchar_t executable[32768]{};
            const auto length = GetModuleFileNameW(nullptr, executable, _countof(executable));
            if (!length || length >= _countof(executable)) return {};
            return DetectFrom(std::filesystem::path(std::wstring(executable, length)), HasPackageIdentity());
        } catch (...) { return {}; }
    }

    inline const Detection& CurrentDetection() noexcept
    {
        static const Detection value = Detect();
        return value;
    }

    inline Kind Current() noexcept
    {
        return CurrentDetection().Value;
    }

    inline const char* Name(Kind value) noexcept
    {
        switch (value) {
        case Kind::SteamGog: return "Steam/GOG";
        case Kind::GamePass: return "Game Pass/WinGDK";
        default: return "Unknown";
        }
    }

    inline bool AllowsSteamNativeSignatures() noexcept
    {
        return Current() == Kind::SteamGog;
    }
}
