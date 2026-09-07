#pragma once
#include <algorithm>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace PS::ModFolderLayout {
inline constexpr const char* PakDirectory = "paks";

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
