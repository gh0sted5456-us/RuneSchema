#pragma once

#include <Windows.h>
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string>

namespace PS::Storefront {
    enum class Kind { SteamGog, GamePass, Unknown };

    inline Kind Detect() noexcept
    {
        wchar_t executable[32768]{};
        const auto length = GetModuleFileNameW(nullptr, executable, _countof(executable));
        if (!length || length >= _countof(executable)) return Kind::Unknown;

        std::wstring path(executable, length);
        std::transform(path.begin(), path.end(), path.begin(), towlower);
        if (path.find(L"\\wingdk\\") != std::wstring::npos
            || path.find(L"windowsapps") != std::wstring::npos
            || path.find(L"microsoftgame") != std::wstring::npos)
            return Kind::GamePass;
        if (path.find(L"\\win64\\") != std::wstring::npos
            || path.find(L"steam") != std::wstring::npos
            || path.find(L"gog") != std::wstring::npos)
            return Kind::SteamGog;
        return Kind::Unknown;
    }

    inline Kind Current() noexcept
    {
        static const Kind value = Detect();
        return value;
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
