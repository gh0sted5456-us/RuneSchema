#pragma once

#include <filesystem>
#include <mutex>
#include <span>
#include <vector>

namespace PS::PakLoader {
inline bool AlreadyRegistered(const std::filesystem::path& candidate,
    std::span<const std::filesystem::path> registered)
{
    for (const auto& root : registered) {
        std::error_code error;
        if (std::filesystem::equivalent(candidate, root, error)) return true;
    }
    return false;
}

inline std::mutex RootsMutex;
inline std::vector<std::filesystem::path> StartupRoots;

inline bool Register(const wchar_t* directory) noexcept
{
    try {
        if (!directory) return false;
        auto paks = std::filesystem::canonical(directory);
        if (paks.filename() != L"paks" || !std::filesystem::is_directory(paks)) return false;
        std::lock_guard lock(RootsMutex);
        if (!AlreadyRegistered(paks, StartupRoots)) StartupRoots.push_back(std::move(paks));
        return true;
    } catch (...) {
        return false;
    }
}

inline std::vector<std::filesystem::path> Roots()
{
    std::lock_guard lock(RootsMutex);
    return StartupRoots;
}
}
