#pragma once
// Finds achievements.json files for the Achievements plugin. Any mod can award Helpy achievements:
//   - UE4SS mods:      ue4ss/Mods/<Mod>/achievements.json (enabled in mods.txt or by enabled.txt)
//   - RuneSchema mods: RuneSchema/mods/<Mod>/achievements.json, or cache/achievements.json
// Parsing and progress stay in Helpy (Core/ModAchievements.h); this only decides which files count.
#include <nlohmann/json.hpp>
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

namespace PS::AchievementSources {
using Json = nlohmann::json;
namespace fs = std::filesystem;

inline std::string Lower(std::string text) {
    for (auto& c : text) c = char(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
inline std::string Text(const fs::path& path) { const auto text = path.u8string(); return {text.begin(), text.end()}; }
inline std::string Trim(std::string text) {
    const auto begin = text.find_first_not_of(" \t\r\n\xEF\xBB\xBF");
    if (begin == std::string::npos) return {};
    return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}
// Real folders and files only: links and junctions never pull in content from elsewhere.
inline bool Plain(const fs::path& path, bool directory) {
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT)
        && bool(attributes & FILE_ATTRIBUTE_DIRECTORY) == directory;
}
// UE4SS mods.txt: "ModName : 1" enables, "ModName : 0" disables. Names compare case-insensitively.
inline std::map<std::string, bool> ModsList(const fs::path& mods) {
    std::map<std::string, bool> result;
    const auto file = mods / "mods.txt";
    std::error_code error;
    if (!Plain(file, false) || fs::file_size(file, error) > 256 * 1024) return result;
    std::ifstream input(file, std::ios::binary);
    for (std::string line; std::getline(input, line);) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line.starts_with("//")) continue;
        const auto split = line.find(':');
        if (split == std::string::npos) continue;
        const auto name = Lower(Trim(line.substr(0, split)));
        if (!name.empty()) result[name] = Trim(line.substr(split + 1)) == "1";
    }
    return result;
}
inline fs::path FirstFile(const fs::path& folder, bool cacheCopy) {
    if (Plain(folder / "achievements.json", false)) return folder / "achievements.json";
    if (cacheCopy && Plain(folder / "cache" / "achievements.json", false)) return folder / "cache" / "achievements.json";
    return {};
}
inline Json Source(const std::string& owner, const std::string& name, const fs::path& file, bool enabled, const char* kind) {
    return {{"Owner", owner}, {"Name", name}, {"File", Text(file)}, {"Enabled", enabled}, {"Kind", kind}};
}
// `mods` is the UE4SS Mods folder. `runeschema` is the compact RuneSchema mod list Helpy already has
// ([{Folder, Name, Enabled, Present}]), so RuneSchema's own enable rules stay authoritative.
inline Json Discover(const fs::path& mods, const Json& runeschema, bool includeUe4ss, bool includeDisabled) {
    Json sources = Json::array();
    if (runeschema.is_array()) for (const auto& row : runeschema) {
        if (!row.is_object() || !row.value("Present", false) || !row.contains("Folder") || !row["Folder"].is_string()) continue;
        const bool enabled = row.value("Enabled", false);
        if (!enabled && !includeDisabled) continue;
        const auto folder = fs::u8path(row["Folder"].get<std::string>());
        if (!Plain(folder, true)) continue;
        const auto file = FirstFile(folder, true);
        if (file.empty()) continue;
        auto name = row.value("Name", std::string{});
        if (name.empty()) name = Text(folder.filename());
        sources.push_back(Source("mods/" + Lower(Text(folder.filename())), name, file, enabled, "runeschema"));
        if (sources.size() >= 2048) return {{"Sources", sources}, {"Truncated", true}};
    }
    if (includeUe4ss && !mods.empty() && Plain(mods, true)) {
        const auto listed = ModsList(mods);
        std::error_code error;
        size_t seen = 0;
        for (fs::directory_iterator it(mods, error), end; !error && it != end && ++seen <= 2048; it.increment(error)) {
            const auto folder = it->path();
            const auto name = Text(folder.filename()), key = Lower(name);
            // RuneSchema reports its own mods above; "shared" is UE4SS's helper library.
            if (key == "runeschema" || key == "shared" || !Plain(folder, true)) continue;
            const auto found = listed.find(key);
            const bool enabled = (found != listed.end() && found->second) || Plain(folder / "enabled.txt", false);
            if (!enabled && !includeDisabled) continue;
            const auto file = FirstFile(folder, false);
            if (file.empty()) continue;
            sources.push_back(Source("ue4ss/" + key, name, file, enabled, "ue4ss"));
            if (sources.size() >= 2048) return {{"Sources", sources}, {"Truncated", true}};
        }
    }
    return {{"Sources", sources}};
}
}
