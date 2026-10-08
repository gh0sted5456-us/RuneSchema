#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "Utility/ModFolderLayout.h"

namespace PS::LogicPaks {
namespace fs = std::filesystem;

struct Package {
    std::string Owner;
    std::string Name;
    fs::path ContainerDirectory;
    std::string ActorPath;
    bool LegacyOwned{};
};

inline bool ValidPackageName(std::string_view value) {
    if (value.empty() || value.size() > 128) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '_';
    });
}

inline bool LegacyPackageExists(const fs::path& logicModsRoot, std::string_view packageName) {
    std::error_code error;
    if (!fs::is_directory(logicModsRoot, error) || error) return false;
    const auto wanted = PS::ModFolderLayout::FoldAscii(std::string(packageName) + ".pak");
    std::size_t visited = 0;
    fs::recursive_directory_iterator current(logicModsRoot,
        fs::directory_options::skip_permission_denied, error), end;
    while (!error && current != end && visited++ < 2048) {
        std::error_code typeError;
        if (current->is_regular_file(typeError) && !typeError
            && PS::ModFolderLayout::FoldAscii(current->path().filename().string()) == wanted)
            return true;
        current.increment(error);
    }
    return false;
}

inline std::vector<Package> Discover(const fs::path& modRoot, std::string owner,
    const fs::path& legacyLogicModsRoot) {
    std::vector<Package> result;
    const auto paks = PS::ModFolderLayout::ResolveLoaderDirectory(
        modRoot, PS::ModFolderLayout::PakDirectory);
    if (!paks) return result;

    std::error_code error;
    std::vector<fs::path> directories;
    fs::directory_iterator current(*paks, fs::directory_options::skip_permission_denied, error), end;
    while (!error && current != end) {
        std::error_code typeError;
        if (current->is_directory(typeError) && !typeError
            && !current->is_symlink(typeError) && !typeError)
            directories.push_back(current->path());
        current.increment(error);
    }
    if (error) return result;
    std::sort(directories.begin(), directories.end());

    for (const auto& directory : directories) {
        std::vector<fs::path> pakFiles;
        fs::directory_iterator files(directory, fs::directory_options::skip_permission_denied, error), filesEnd;
        while (!error && files != filesEnd) {
            std::error_code typeError;
            if (files->is_regular_file(typeError) && !typeError
                && PS::ModFolderLayout::EqualsInsensitive(files->path().extension().string(), ".pak"))
                pakFiles.push_back(files->path());
            files.increment(error);
        }
        if (error || pakFiles.size() != 1) { error.clear(); continue; }

        const auto name = pakFiles.front().stem().string();
        if (!ValidPackageName(name)) continue;
        auto hasSibling = [&](std::string_view extension) {
            std::error_code siblingError;
            for (const auto& sibling : fs::directory_iterator(directory,
                     fs::directory_options::skip_permission_denied, siblingError)) {
                if (siblingError) break;
                std::error_code typeError;
                if (sibling.is_regular_file(typeError) && !typeError
                    && PS::ModFolderLayout::EqualsInsensitive(sibling.path().stem().string(), name)
                    && PS::ModFolderLayout::EqualsInsensitive(sibling.path().extension().string(), extension))
                    return true;
            }
            return false;
        };
        if (!hasSibling(".utoc") || !hasSibling(".ucas")) continue;

        result.push_back({owner, name, directory,
            "/Game/Mods/" + name + "/ModActor.ModActor_C",
            LegacyPackageExists(legacyLogicModsRoot, name)});
    }
    return result;
}

class WorldActivationGate {
public:
    bool Claim(std::uintptr_t world, const std::string& package) {
        if (!world) return false;
        if (world != m_world) { m_world = world; m_claimed.clear(); }
        return m_claimed.emplace(package).second;
    }
    void Release(std::uintptr_t world, const std::string& package) {
        if (world == m_world) m_claimed.erase(package);
    }
    void Reset() noexcept { m_world = 0; m_claimed.clear(); }
private:
    std::uintptr_t m_world{};
    std::unordered_set<std::string> m_claimed;
};
}
