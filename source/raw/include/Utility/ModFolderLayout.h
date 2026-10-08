#pragma once
#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <stdexcept>
#include <vector>

namespace PS::ModFolderLayout {
inline constexpr const char* PakDirectory = "paks";
inline constexpr const char* LogicModDirectory = "logicmods";

inline std::string AsciiLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return character >= 'A' && character <= 'Z'
            ? static_cast<char>(character + ('a' - 'A'))
            : static_cast<char>(character);
    });
    return value;
}

inline std::string FoldAscii(std::string value) { return AsciiLower(std::move(value)); }

inline bool EqualsInsensitive(std::string_view left, std::string_view right) {
    return AsciiLower(std::string(left)) == AsciiLower(std::string(right));
}

inline constexpr std::array<std::string_view,25> ContentDirectories{{
    "assets","blueprints","buildings","courses","dialogue","effects","enums",
    "equipment","events","journal","lore","nameplates","niagara","npc","players",
    "quests","raw","recipes","registry","spawns","strings","ue4ss","vendors","paks","logicmods"
}};

inline bool IsContentDirectoryName(std::string_view value) {
    return std::any_of(ContentDirectories.begin(),ContentDirectories.end(),
        [&](std::string_view candidate){return EqualsInsensitive(value,candidate);});
}

// Resolve one immediate loader directory without relying on the host file
// system's case rules. Two directories differing only by case are ambiguous
// and therefore rejected instead of being loaded twice in an undefined order.
inline std::optional<std::filesystem::path> ResolveLoaderDirectory(
    const std::filesystem::path& modRoot, std::string_view expected) {
    namespace fs = std::filesystem;
    std::error_code error;
    if (!fs::is_directory(modRoot, error) || error) return std::nullopt;
    std::optional<fs::path> match;
    fs::directory_iterator current(modRoot, fs::directory_options::skip_permission_denied, error), end;
    while (!error && current != end) {
        std::error_code typeError;
        if (current->is_directory(typeError) && !typeError
            && EqualsInsensitive(current->path().filename().string(), expected)) {
            if (match && *match != current->path())
                throw std::runtime_error("multiple loader folders differ only by letter case: "
                    + std::string(expected));
            match = current->path();
        }
        current.increment(error);
    }
    if (error) throw std::runtime_error("loader folder scan failed: " + error.message());
    return match;
}

inline std::filesystem::path FindChildDirectory(
    const std::filesystem::path& parent, std::string_view wanted) {
    namespace fs = std::filesystem;
    std::error_code error;
    if (!fs::is_directory(parent, error) || error) return {};
    const auto folded = FoldAscii(std::string(wanted));
    fs::path found;
    std::size_t entries = 0;
    for (const auto& entry : fs::directory_iterator(parent, error)) {
        if (error) throw std::system_error(error, "Cannot enumerate mod directory");
        if (++entries > 256) throw std::runtime_error("Mod folder contains more than 256 top-level entries");
        std::error_code typeError;
        if (!entry.is_directory(typeError) || typeError) continue;
        if (FoldAscii(entry.path().filename().string()) != folded) continue;
        if (!found.empty() && fs::weakly_canonical(found) != fs::weakly_canonical(entry.path()))
            throw std::runtime_error("Loader folder names differ only by case; keep exactly one");
        found = entry.path();
    }
    return found;
}

inline bool IsPakContainerExtension(const std::filesystem::path& path) {
    const auto extension = FoldAscii(path.extension().string());
    return extension == ".pak" || extension == ".utoc"
        || extension == ".ucas" || extension == ".sig";
}

inline bool ContainsImmediatePakContent(
    const std::filesystem::path& folder, std::error_code& error) {
    namespace fs = std::filesystem;
    error.clear();
    fs::directory_iterator current(folder, fs::directory_options::skip_permission_denied, error), end;
    while (!error && current != end) {
        std::error_code typeError;
        if (current->is_regular_file(typeError) && !typeError
            && IsPakContainerExtension(current->path())) return true;
        current.increment(error);
    }
    return false;
}

// Unreal's startup PAK scan consumes concrete read directories; it does not
// reliably recurse from the RuneSchema mod root into paks/<PackageName>/.
// Return only directories that directly contain container files, preserving
// flat legacy packages and deterministic package-folder order.
inline std::vector<std::filesystem::path> PakReadDirectories(
    const std::filesystem::path& modRoot) {
    namespace fs = std::filesystem;
    std::error_code error;
    if (!fs::is_directory(modRoot, error) || error
        || fs::is_symlink(fs::symlink_status(modRoot, error)) || error)
        throw std::runtime_error("mod PAK root is not a safe directory");

    std::vector<fs::path> result;
    if (ContainsImmediatePakContent(modRoot, error)) result.push_back(modRoot);
    if (error) throw std::runtime_error("mod PAK root scan failed: " + error.message());

    for (const auto loader : {PakDirectory, LogicModDirectory}) {
        const auto containerRoot = ResolveLoaderDirectory(modRoot, loader);
        if (!containerRoot) continue;
        error.clear();
        if (fs::is_symlink(fs::symlink_status(*containerRoot, error)) || error)
            throw std::runtime_error("mod container path is not a safe directory");
        if (ContainsImmediatePakContent(*containerRoot, error)) result.push_back(*containerRoot);
        if (error) throw std::runtime_error("mod container scan failed: " + error.message());

        std::vector<fs::path> packages;
        fs::directory_iterator current(*containerRoot, fs::directory_options::skip_permission_denied, error), end;
        while (!error && current != end) {
            std::error_code typeError;
            if (current->is_directory(typeError) && !typeError
                && !current->is_symlink(typeError) && !typeError)
                packages.push_back(current->path());
            current.increment(error);
        }
        if (error) throw std::runtime_error("mod package scan failed: " + error.message());
        std::sort(packages.begin(), packages.end());
        for (const auto& package : packages) {
            if (ContainsImmediatePakContent(package, error)) result.push_back(package);
            if (error) throw std::runtime_error("mod package scan failed: " + error.message());
        }
    }
    return result;
}

inline bool ContainsLegacyPakContent(const std::filesystem::path& folder, std::error_code& error);
inline bool LooksLikeRuneSchemaMod(const std::filesystem::path& folder) {
    namespace fs = std::filesystem;
    std::error_code error;
    if(!fs::is_directory(folder,error)||error||fs::is_symlink(fs::symlink_status(folder,error)))return false;
    fs::directory_iterator current(folder,fs::directory_options::skip_permission_denied,error),end;
    while(!error&&current!=end) {
        std::error_code typeError;
        if(current->is_directory(typeError)&&!typeError
            && IsContentDirectoryName(current->path().filename().string()))return true;
        current.increment(error);
    }
    if(error)return false;
    std::error_code legacyError;
    return ContainsLegacyPakContent(folder,legacyError)&&!legacyError;
}

inline bool ContainsLegacyPakContent(const std::filesystem::path& folder, std::error_code& error) {
    namespace fs = std::filesystem;
    fs::recursive_directory_iterator current(folder, error), end;
    while (!error && current != end) {
        auto extension = current->path().extension().native();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](auto c) {
            return c >= 'A' && c <= 'Z' ? static_cast<decltype(c)>(c + ('a' - 'A')) : c;
        });
        auto matches = [&](std::string_view suffix) {
            return extension.size() == suffix.size() && std::equal(extension.begin(), extension.end(), suffix.begin());
        };
        if (matches(".pak") || matches(".utoc") || matches(".ucas") || matches(".sig")) {
            if (current->is_regular_file(error)) return true;
            if (error) break;
        }
        current.increment(error);
    }
    return false;
}
}
