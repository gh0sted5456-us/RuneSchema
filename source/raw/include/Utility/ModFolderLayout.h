#pragma once
#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <stdexcept>

namespace PS::ModFolderLayout {
inline constexpr const char* PakDirectory = "paks";

inline std::string FoldAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    });
    return value;
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
