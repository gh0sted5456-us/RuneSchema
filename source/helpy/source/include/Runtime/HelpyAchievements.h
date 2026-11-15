#pragma once
// Runtime glue for mod achievements: loads definitions from enabled mods' cache/achievements.json, reads
// the core GameplayEvents feed, applies progress, persists it in the RuneSchema profile folder and queues
// unlock popups for the bottom HUD. Model and validation live in Core/ModAchievements.h.
#include "Core/ModAchievements.h"
#include "Core/RuneSchemaProfile.h"
#include "Core/ConfigFiles.h"
#include "Core/HelpyCoreLink.h"
#include "Runtime/PluginRuntimeServices.h"
#include "Generator/ToolRequest.h"
#include <chrono>
#include <deque>
#include <filesystem>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace PS::HelpyAchievements {
using Json = nlohmann::json; using Clock = std::chrono::steady_clock;
namespace A = ModAchievements;
struct Loaded { std::vector<A::Definition> Definitions; std::map<std::string, std::string> Errors; std::map<std::string, size_t> Counts; };

inline std::vector<A::Definition> Definitions;
inline std::map<std::string, std::string> Errors;  // owner -> first validation problem, for Profile and Mods.
inline std::map<std::string, size_t> Counts;        // owner -> achievements defined.
inline A::Progress Progress;
inline std::deque<A::Unlock> Popups;                // Consumed by the bottom HUD overlay.
inline std::string Status;
inline bool ProgressLoaded = false, Dirty = false, PollPending = false;
inline uint64_t SnapshotRevision = ~uint64_t{}, Revision = 0;
inline std::future<Loaded> Loading;
inline Json WatchSent;
inline Clock::time_point NextPoll{}, NextSave{};
struct FeedScope {
    uint64_t Connection, World;
    uint32_t Pid;
    auto operator<=>(const FeedScope&) const = default;
    static FeedScope Current() {
        return {PluginRuntimeServices::ConnectionEpoch.load(), SpawnToolRequests::CurrentGeneration(), PluginRuntimeServices::GameProcessId.load()};
    }
};
inline std::optional<FeedScope> LastFeed;
// Summary for other players. The profile service reads it on the game thread, so it gets its own copy.
inline std::mutex SummaryGate;
inline Json PublishedSummary = nullptr;
inline uint64_t SummaryRevision = ~uint64_t{};
inline void Publish() {
    if (!ProgressLoaded || SummaryRevision == Revision) return;
    SummaryRevision = Revision;
    auto summary = A::Summary(Progress);
    std::lock_guard lock(SummaryGate); PublishedSummary = std::move(summary);
}

inline std::string Owner(const std::filesystem::path& folder) {
    auto name = folder.filename().u8string();
    std::string text(name.begin(), name.end());
    for (auto& c : text) c = char(std::tolower(static_cast<unsigned char>(c)));
    return "mods/" + text;
}
// Reads achievements.json from every enabled, present mod in a ModsRead snapshot. Runs off the UI thread.
inline Loaded Read(const Json& rows, bool includeDisabled = false) {
    Loaded result;
    if (!rows.is_array()) return result;
    for (const auto& row : rows) try {
        if (!row.is_object() || (!includeDisabled && !row.value("Enabled", false)) || !row.value("Present", false) || !row.contains("Folder") || !row["Folder"].is_string()) continue;
        const auto folder = std::filesystem::u8path(row["Folder"].get<std::string>());
        const auto file = folder / "cache" / "achievements.json";
        std::error_code error;
        if (!std::filesystem::is_regular_file(file, error)) continue;
        const auto owner = Owner(folder);
        std::string name = row.value("Name", std::string{});
        if (row.contains("Metadata") && row["Metadata"].is_object() && row["Metadata"].contains("name") && row["Metadata"]["name"].is_string()) name = row["Metadata"]["name"].get<std::string>();
        try {
            const auto text = ConfigFiles::Read(file, 256 * 1024);
            auto parsed = A::Parse(Json::parse(text, nullptr, true, true), owner, name);
            result.Counts[owner] = parsed.size();
            for (auto& definition : parsed) result.Definitions.push_back(std::move(definition));
        } catch (const std::exception& problem) { result.Errors[owner] = problem.what(); }
    } catch (...) {}
    return result;
}
inline std::filesystem::path File() { return RuneSchemaProfile::Data("achievements.json"); }
inline void LoadProgress() {
    if (ProgressLoaded || !RuneSchemaProfile::Ready.load()) return;
    ProgressLoaded = true;
    try {
        const auto file = File();
        std::error_code error;
        if (std::filesystem::is_regular_file(file, error)) Progress = A::Progress::Parse(Json::parse(ConfigFiles::Read(file, 4 * 1024 * 1024)));
    } catch (const std::exception& problem) {
        // Never overwrite unreadable progress: keep it aside and start fresh.
        Status = std::string("Achievement progress could not be read and was kept as achievements.json.bad: ") + problem.what();
        std::error_code ignored; std::filesystem::rename(File(), File().string() + ".bad", ignored);
        Progress = {};
    }
    ++Revision; Publish();
    RuneSchemaProfile::LocalAchievements = [] { std::lock_guard lock(SummaryGate); return PublishedSummary; };
}
inline void Save(bool force) {
    if (!ProgressLoaded || !Dirty || (!force && Clock::now() < NextSave)) return;
    try { ConfigFiles::Write(File(), Progress.Document().dump(1)); Dirty = false; NextSave = Clock::now() + std::chrono::seconds(10); }
    catch (const std::exception& problem) { Status = std::string("Achievement progress was not saved: ") + problem.what(); NextSave = Clock::now() + std::chrono::seconds(30); }
}
// Reloads definitions when the Mods snapshot changes. `rows` is ModsPanel::Rows, `revision` its SnapshotRevision.
inline void Observe(const Json& rows, uint64_t revision) {
    if (Loading.valid() && Loading.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto loaded = Loading.get();
        Definitions = std::move(loaded.Definitions); Errors = std::move(loaded.Errors); Counts = std::move(loaded.Counts);
        WatchSent = nullptr; ++Revision;
    }
    if (revision == SnapshotRevision || Loading.valid()) return;
    SnapshotRevision = revision;
    Loading = std::async(std::launch::async, [rows] { return Read(rows); });
}
// Applies one GameplayEvents reply. Runs on the UI thread (from HelpyCoreLink::Drain).
inline void Apply(bool ok, const Json& reply, const Json& watch, bool watching) {
    const auto now = Clock::now();
    try {
        if (!ok || !reply.is_object() || !reply.value("Available", true) || !reply.contains("Events") || !reply["Events"].is_array()) { NextPoll = now + std::chrono::seconds(5); return; }
        if (watching) WatchSent = watch;
        if (Progress.Resume(reply.value("Session", std::string{}))) {
            // New game process: everything in its feed is new. Re-read from the start of that session.
            Dirty = true; NextPoll = now; WatchSent = nullptr; return;
        }
        bool unlocked = false;
        for (const auto& event : reply["Events"]) {
            if (!event.is_object()) continue;
            const auto sequence = event.contains("Sequence") && event["Sequence"].is_number_unsigned() ? event["Sequence"].get<uint64_t>() : 0;
            if (sequence && sequence <= Progress.Cursor) continue;
            for (auto& unlock : Progress.Apply(Definitions, event)) { unlocked = true; Popups.push_back(std::move(unlock)); if (Popups.size() > 16) Popups.pop_front(); }
            Progress.Cursor = std::max(Progress.Cursor, sequence);
            Dirty = true;
        }
        if (reply.contains("Cursor") && reply["Cursor"].is_number_unsigned() && reply["Events"].empty() && reply["Cursor"].get<uint64_t>() > Progress.Cursor) { Progress.Cursor = reply["Cursor"].get<uint64_t>(); Dirty = true; }
        if (!reply["Events"].empty()) { ++Revision; NextPoll = now + std::chrono::milliseconds(250); }
        Publish();
        Save(unlocked); // Unlocks are written at once; counters at most every 10 seconds.
    } catch (...) { NextPoll = now + std::chrono::seconds(5); }
}
// Asks the core for new gameplay events about once a second while a game is connected. The request runs on
// Helpy's core link thread, so a busy game or desktop pipe never stalls the UI.
inline void Poll(HelpyCoreLink::Link& link, const HelpyCoreLink::Remote& remote, bool gameRunning) {
    LoadProgress();
    const auto scope = FeedScope::Current();
    if (LastFeed != scope) { LastFeed = scope; WatchSent = nullptr; NextPoll = {}; }
    if (!remote || !gameRunning || !scope.Pid || !ProgressLoaded) { Save(true); return; }
    const auto now = Clock::now();
    if (now < NextPoll || PollPending) { Save(false); return; }
    NextPoll = now + std::chrono::seconds(1);
    Json request{{"Action", "GameplayEvents"}, {"Cursor", Progress.Cursor}};
    const auto watch = A::WatchList(Definitions);
    const bool watching = WatchSent != watch;
    if (watching) request["Watch"] = watch;
    PollPending = link.Post("achievements", std::move(request),
        [remote, scope](const Json& request, Json& reply) { return scope == FeedScope::Current() && remote(request, reply); },
        [watch, watching, scope](bool ok, const Json& reply) {
            PollPending = false;
            if (scope == FeedScope::Current()) Apply(ok, reply, watch, watching);
        });
    if (!PollPending) NextPoll = now + std::chrono::seconds(5);
}
// Shared with other players through the public profile exchange. Null until progress is loaded.
inline Json Summary() { std::lock_guard lock(SummaryGate); return PublishedSummary; }
inline void Shutdown() { PollPending = false; LastFeed.reset(); RuneSchemaProfile::LocalAchievements = {}; Save(true); if (Loading.valid()) Loading.wait(); }
}
