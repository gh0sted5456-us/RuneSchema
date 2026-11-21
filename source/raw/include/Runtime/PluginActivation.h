#pragma once
// Pre-discovery recovery for an interrupted whole-plugin activation.
//
// An external activator replaces Helpy/plugins/<Id> with two directory renames.
// A power loss between them leaves the plugin absent (never mixed). Before any
// plugin scan, RuneSchema core and Helpy.exe call Recover(). It acts only when
//   Helpy/plugin-updates/<Id>/activation.json has "State": "Swapping"
// and the live Helpy/plugins/<Id> folder is missing. It moves back exactly one
// already-verified folder whose plugin.json matches, records what it did, and
// never deletes anything. Running it again is harmless.
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#include <windows.h>
#endif

namespace PS::PluginActivation {
namespace fs = std::filesystem;
inline constexpr auto Schema = "runeschema.plugin-activation/1";

inline bool Name(const std::string& value) {
    if (value.empty() || value.size() > 96 || value == "." || value == "..") return false;
    for (const unsigned char c : value)
        if (!(std::isalnum(c) || c == '.' || c == '_' || c == '-')) return false;
    return true;
}
inline bool PlainDirectory(const fs::path& path) {
    std::error_code error;
    const auto status = fs::symlink_status(path, error);
    return !error && fs::is_directory(status) && !fs::is_symlink(status);
}
inline bool Matches(const fs::path& folder, const std::string& id, const std::string& version) {
    if (version.empty() || !PlainDirectory(folder)) return false;
    try {
        std::ifstream stream(folder / "plugin.json", std::ios::binary);
        if (!stream) return false;
        nlohmann::json data; stream >> data;
        return data.value("Id", std::string{}) == id && data.value("Version", std::string{}) == version;
    } catch (...) { return false; }
}
inline void Record(const fs::path& file, nlohmann::json document, const std::string& state, const std::string& detail) {
    document["State"] = state; document["RecoveredBy"] = "RuneSchema"; document["RecoveryDetail"] = detail;
    const auto temporary = fs::path(file).concat(L".recovering");
    { std::ofstream stream(temporary, std::ios::binary | std::ios::trunc); stream << document.dump(2); stream.flush(); if (!stream) throw std::runtime_error("activation record could not be written"); }
    fs::rename(temporary, file);
}
// runtime = Mods/RuneSchema. Returns one line per plugin that needed attention.
inline std::vector<std::string> Recover(const fs::path& runtime) {
    std::vector<std::string> notes;
    const auto updates = runtime / "Helpy" / "plugin-updates", plugins = runtime / "Helpy" / "plugins";
    if (!PlainDirectory(updates)) return notes;
    std::error_code scan;
    for (fs::directory_iterator entry(updates, scan), end; !scan && entry != end; entry.increment(scan)) {
        const auto folder = entry->path(); const auto id = folder.filename().string();
        const auto file = folder / "activation.json";
        try {
            if (!Name(id) || !PlainDirectory(folder) || !fs::is_regular_file(fs::symlink_status(file))) continue;
            nlohmann::json document;
            { std::ifstream stream(file, std::ios::binary); stream >> document; }
            if (document.value("Schema", std::string{}) != Schema || document.value("Id", std::string{}) != id
                || document.value("State", std::string{}) != "Swapping") continue;
            const auto live = plugins / id;
            std::error_code probe;
            // Act only on a definite absence; a present or unreadable live path is left alone.
            if (fs::symlink_status(live, probe).type() != fs::file_type::not_found) continue;
            const auto staged = document.value("Staged", std::string{}), previous = document.value("Previous", std::string{});
            if (!Name(staged) || !Name(previous) || staged == previous) { notes.push_back(id + ": activation record names are invalid; plugin left absent"); continue; }
            if (!PlainDirectory(plugins)) { notes.push_back(id + ": Helpy/plugins is missing; plugin left absent"); continue; }
            if (Matches(folder / staged, id, document.value("Version", std::string{}))) {
                fs::rename(folder / staged, live);
                Record(file, document, "RecoveredForward", "staged generation moved into place before plugin discovery");
                notes.push_back(id + ": interrupted activation completed with the staged generation");
            } else if (Matches(folder / previous, id, document.value("PreviousVersion", std::string{}))) {
                fs::rename(folder / previous, live);
                Record(file, document, "RecoveredBack", "previous generation restored before plugin discovery");
                notes.push_back(id + ": interrupted activation rolled back to the previous generation");
            } else notes.push_back(id + ": interrupted activation has no matching staged or previous folder; plugin left absent");
        } catch (const std::exception& error) {
            notes.push_back(id + ": activation recovery failed: " + error.what());
        }
    }
    return notes;
}
#ifdef _WIN32
// The game and Helpy.exe may start together; only one recovers at a time.
inline std::vector<std::string> RecoverGuarded(const fs::path& runtime) {
    const auto gate = CreateMutexW(nullptr, FALSE, L"Local\\RuneSchema.PluginActivation");
    if (!gate) return {"activation recovery skipped: lock unavailable"};
    const auto wait = WaitForSingleObject(gate, 5000);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) { CloseHandle(gate); return {"activation recovery skipped: another process holds the lock"}; }
    std::vector<std::string> notes;
    try { notes = Recover(runtime); } catch (const std::exception& error) { notes.push_back(std::string("activation recovery failed: ") + error.what()); }
    ReleaseMutex(gate); CloseHandle(gate);
    return notes;
}
#endif
}
