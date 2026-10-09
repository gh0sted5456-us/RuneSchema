#pragma once

#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <windows.h>

#include "Loader/ModLoadOrder.h"
#include "Runtime/BPModLoaderPatch.h"
#include "Runtime/LogicPakContract.h"
#include "Utility/ModFolderLayout.h"

namespace PS::BPModLoaderIntegration {
namespace fs = std::filesystem;

inline std::atomic_bool& Active() {
    static std::atomic_bool value{false};
    return value;
}

inline std::string ReadFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not read " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

inline bool AtomicWrite(const fs::path& target, std::string_view content) {
    std::error_code error;
    fs::create_directories(target.parent_path(), error);
    if (error || fs::is_symlink(fs::symlink_status(target, error))) return false;
    const auto temporary = fs::path(target.wstring() + L".runeschema-"
        + std::to_wstring(GetCurrentProcessId()) + L".tmp");
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        output.flush();
        if (!output) { fs::remove(temporary, error); return false; }
    }
    if (!MoveFileExW(temporary.c_str(), target.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        fs::remove(temporary, error);
        return false;
    }
    return true;
}

inline std::optional<bool> HasMultipleHardlinks(const fs::path& path) {
    const auto handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return std::nullopt;
    BY_HANDLE_FILE_INFORMATION info{};
    const bool queried = GetFileInformationByHandle(handle, &info) != 0;
    CloseHandle(handle);
    if (!queried) return std::nullopt;
    return info.nNumberOfLinks > 1;
}

struct Result {
    bool Active{};
    bool Patched{};
    std::size_t Submitted{};
    std::string Detail;
};

inline std::vector<std::string> CollectPackages(const fs::path& ue4ssRoot,
    const fs::path& legacyLogicModsRoot) {
    const auto modsRoot = ue4ssRoot / "Mods" / "RuneSchema" / "mods";
    std::vector<RC::StringType> discovered;
    std::error_code error;
    if (fs::is_directory(modsRoot, error) && !error) {
        for (const auto& entry : fs::directory_iterator(modsRoot))
            if (ModFolderLayout::LooksLikeRuneSchemaMod(entry.path()))
                discovered.push_back(entry.path().filename().native());
    }
    std::set<std::string> seen;
    std::vector<std::string> packages;
    for (const auto& name : DragonWilds::ModLoadOrder::Resolve(modsRoot, discovered)) {
        const auto owner = RC::to_string(name);
        for (const auto& package : LogicPaks::Discover(modsRoot / name, owner, legacyLogicModsRoot)) {
            if (!package.LegacyOwned && seen.insert(package.Name).second)
                packages.push_back(package.Name);
        }
    }
    return packages;
}

inline std::string PackageManifest(const std::vector<std::string>& packages) {
    std::ostringstream manifest;
    manifest << "# RuneSchema enabled LogicMods; generated before UE4SS Lua startup.\n";
    for (const auto& package : packages) manifest << package << '\n';
    return manifest.str();
}

inline Result PrepareLua(const fs::path& ue4ssRoot,
    const fs::path& legacyLogicModsRoot) {
    Active().store(false, std::memory_order_release);
    const auto modsRoot = ue4ssRoot / "Mods";
    if (!EnabledInModsTxt(modsRoot / "mods.txt", "RuneSchema"))
        return {false, false, 0, "RuneSchema is disabled"};
    const auto settingsRoot = modsRoot / "RuneSchema" / "settings";
    if (!AtomicWrite(settingsRoot / "logicmods.generated.txt", "# BPModLoaderMod handoff disabled.\n")
        || !AtomicWrite(settingsRoot / "logicmods.lua.generated.txt", "# Lua ModActor loader inactive.\n"))
        return {false, false, 0, "could not clear previous LogicMod handoff lists"};
    std::error_code error;
    if (!fs::is_regular_file(modsRoot / "RuneSchema" / "scripts" / "logicmods-loader.lua", error) || error)
        return {false, false, 0, "RuneSchema Lua ModActor helper is missing"};
    const auto packages = CollectPackages(ue4ssRoot, legacyLogicModsRoot);
    if (!AtomicWrite(settingsRoot / "logicmods.lua.generated.txt", PackageManifest(packages)))
        return {false, false, 0, "could not write RuneSchema Lua ModActor handoff list"};
    Active().store(true, std::memory_order_release);
    return {true, false, packages.size(), "RuneSchema Lua owns enabled ModActors"};
}

inline Result Prepare(const fs::path& ue4ssRoot,
    const fs::path& legacyLogicModsRoot, bool patchScript) {
    Active().store(false, std::memory_order_release);
    const auto modsTxt = ue4ssRoot / "Mods" / "mods.txt";
    if (!EnabledInModsTxt(modsTxt, "RuneSchema"))
        return {false, false, 0, "RuneSchema is disabled"};
    const auto list = ue4ssRoot / "Mods" / "RuneSchema" / "settings" / "logicmods.generated.txt";
    constexpr std::string_view emptyManifest =
        "# RuneSchema BPModLoader integration inactive; native ModActor fallback owns startup.\n";
    // Clear any previous handoff before choosing an owner. An older patched
    // BPModLoader script may still be installed after the setting is disabled.
    if (!AtomicWrite(list, emptyManifest)
        || !AtomicWrite(ue4ssRoot / "Mods" / "RuneSchema" / "settings" / "logicmods.lua.generated.txt",
            "# RuneSchema Lua ModActor loader inactive.\n"))
        return {false, false, 0, "could not clear the enabled LogicMods list; startup ownership is uncertain"};
    if (!EnabledInModsTxt(modsTxt, "BPModLoaderMod"))
        return {false, false, 0, "BPModLoaderMod is disabled; native ModActor fallback owns startup"};
    if (!patchScript)
        return {false, false, 0, "script patch disabled by settings; native ModActor fallback owns startup"};
    const auto bpScript = ue4ssRoot / "Mods" / "BPModLoaderMod" / "Scripts" / "main.lua";
    const auto helper = ue4ssRoot / "Mods" / "RuneSchema" / "scripts" / "logicmods-register.lua";
    std::error_code error;
    if (!fs::is_regular_file(bpScript, error) || error
        || !fs::is_regular_file(helper, error) || error)
        return {false, false, 0, "BPModLoaderMod script or RuneSchema integration helper is missing"};

    const auto packages = CollectPackages(ue4ssRoot, legacyLogicModsRoot);
    const auto original = ReadFile(bpScript);
    const auto updated = PatchSource(original);
    if (!updated) return {false, false, 0, "BPModLoaderMod script has an unsupported structure; left untouched"};
    const bool changed = *updated != original;
    if (changed) {
        const auto hardlinked = HasMultipleHardlinks(bpScript);
        if (!hardlinked.has_value())
            return {false, false, 0, "could not verify BPModLoaderMod script hardlink ownership; left untouched"};
        if (*hardlinked)
            return {false, false, 0, "BPModLoaderMod script is hardlinked (possibly Vortex-managed); left untouched"};
        const auto backup = ue4ssRoot / "Mods" / "RuneSchema" / "settings" / "backups"
            / ("BPModLoaderMod-main-" + std::to_string(std::hash<std::string>{}(original)) + ".lua");
        if (!fs::exists(backup, error) && !AtomicWrite(backup, original))
            return {false, false, 0, "could not preserve the original BPModLoaderMod script"};
        if (!AtomicWrite(bpScript, *updated))
            return {false, false, 0, "could not atomically update BPModLoaderMod"};
    }
    if (!AtomicWrite(list, PackageManifest(packages)))
        return {false, changed, 0, "could not atomically write the enabled LogicMods list"};
    Active().store(true, std::memory_order_release);
    return {true, changed, packages.size(), "BPModLoaderMod will read RuneSchema's enabled LogicMods list"};
}
}
