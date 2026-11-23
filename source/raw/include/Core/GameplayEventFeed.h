#pragma once
// Local-player gameplay events for Helpy achievements. The game-side observer pushes normalized events;
// Helpy reads them with a cursor through the "GameplayEvents" service. Portable: no Unreal types here.
//
// Every event is about the local player on this machine. Remote players' server RPCs that execute on a
// host are filtered out by the observer; confirmed kills are relayed from the host to the credited client.
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace PS::GameplayEventFeed {
using Json = nlohmann::json;

// Native functions observed for every player. Mods may add their own Blueprint functions (kind "custom"),
// or report events directly through RuneSchema_ReportModEventV1 (kind "report", carrying the mod's Owner).
inline constexpr std::string_view CraftFunction = "/Script/Dominion.InventoryController:Server_CraftRecipe";
inline constexpr std::string_view RecipesUnlockedFunction = "/Script/Dominion.ProgressComponent:Client_OnRecipesUnlocked";
inline constexpr std::string_view BuildingsUnlockedFunction = "/Script/Dominion.ProgressComponent:Client_OnBuildingsUnlocked";
inline constexpr std::string_view QuestUpdatedFunction = "/Script/Dominion.QuestProgressComponent:Client_OnQuestUpdated";
inline constexpr std::string_view SkillXPFunction = "/Game/Gameplay/Character/Components/BP_Components_Skill.BP_Components_Skill_C:BP_OnSkillXPChanged";
inline constexpr std::string_view RespawnFunction = "/Game/Gameplay/Character/Player/BP_PlayerCharacter.BP_PlayerCharacter_C:OnPlayerRespawn";
inline constexpr std::string_view BuildFunction = "/Script/Dominion.BuildModeComponent:Server_SpawnBuilding";
inline constexpr std::string_view UseItemFunction = "/Script/Dominion.InventoryController:Server_UseItemFromInventory";
inline constexpr std::array<std::string_view, 8> NativeFunctions{CraftFunction, RecipesUnlockedFunction, BuildingsUnlockedFunction,
    QuestUpdatedFunction, SkillXPFunction, RespawnFunction, BuildFunction, UseItemFunction};
inline constexpr std::array<std::string_view, 11> Kinds{"kill", "craft", "recipe-unlocked", "building-unlocked", "quest-updated",
    "skill-xp", "death", "building-placed", "item-used", "custom", "report"};
// Reads are capped so one reply stays well under the 8 KiB first-try buffer most of the time.
inline constexpr size_t Capacity = 1024, MaximumRead = 64, MaximumWatch = 64, MaximumParameters = 1024;
// Raw parametersJson accepted by RuneSchema_ReportModEventV1 before parsing; the parsed object is then held
// to MaximumParameters like captured parameters.
inline constexpr size_t MaximumReportInput = 4096;

struct Event {
    uint64_t Sequence = 0;
    std::string Kind, Subject;
    std::string Owner; // "report" only: the reporting mod, "mods/<folder>" or "ue4ss/<folder>", lower-case.
    double Amount = 1, Value = 0;
    Json Parameters = Json::object();
    uint64_t Time = 0; // Unix seconds.
};

inline bool KnownKind(std::string_view kind) { return std::find(Kinds.begin(), Kinds.end(), kind) != Kinds.end(); }
// Object or function path: "/Game/...", "/Script/...", optional ".Name" and ":Function". Same alphabet as activity paths.
inline bool Path(std::string_view path, bool function) {
    if (path.empty() || path.size() > 512 || path.front() != '/' || path.find("..") != path.npos) return false;
    const auto colon = path.find(':');
    if (function != (colon != path.npos) || (function && (colon != path.rfind(':') || colon + 1 >= path.size()))) return false;
    return std::all_of(path.begin(), path.end(), [](unsigned char c) { return std::isalnum(c) || c == '/' || c == '_' || c == '.' || c == ':' || c == '-'; });
}
inline uint64_t Now() { return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()); }

namespace Detail {
// First object path in a captured parameter tree (depth-first, field order), e.g. the recipe a craft used.
inline std::string FirstPath(const Json& value, int depth = 0) {
    if (depth > 4) return {};
    if (value.is_string()) { const auto& text = value.get_ref<const std::string&>(); return Path(text, false) ? text : std::string{}; }
    if (value.is_object()) for (const auto& [key, child] : value.items()) { if (key.starts_with('$')) continue; if (auto found = FirstPath(child, depth + 1); !found.empty()) return found; }
    return {};
}
inline double PositiveCount(const Json& parameters) {
    for (const auto& name : {"Count", "Amount", "Quantity", "NumToCraft", "CraftCount"})
        if (parameters.contains(name) && parameters[name].is_number_integer()) { const auto value = parameters[name].get<int64_t>(); if (value >= 1 && value <= 100000) return double(value); }
    return 1;
}
// Unlock callbacks pass arrays; the capture records only {"Count": n}.
inline double ArrayCount(const Json& parameters) {
    for (const auto& [key, value] : parameters.items())
        if (value.is_object() && value.size() == 1 && value.contains("Count") && value["Count"].is_number_integer()) return double(std::max<int64_t>(0, value["Count"].get<int64_t>()));
    return 1;
}
inline Json Bounded(Json parameters) {
    if (!parameters.is_object()) return Json::object();
    if (parameters.dump().size() <= MaximumParameters) return parameters;
    return Json{{"$truncated", true}};
}
}

// Maps an observed function call to an event. Returns nothing for calls that are not achievement signals.
inline std::optional<Event> Normalize(std::string_view function, const Json& parameters, const std::set<std::string, std::less<>>& watched = {}) {
    Event event;
    const Json captured = parameters.is_object() ? parameters : Json::object();
    if (function == CraftFunction) { event.Kind = "craft"; event.Subject = Detail::FirstPath(captured); event.Amount = Detail::PositiveCount(captured); }
    else if (function == RecipesUnlockedFunction || function == BuildingsUnlockedFunction) {
        event.Kind = function == RecipesUnlockedFunction ? "recipe-unlocked" : "building-unlocked";
        event.Amount = Detail::ArrayCount(captured); if (event.Amount <= 0) return std::nullopt;
    }
    else if (function == QuestUpdatedFunction) {
        event.Kind = "quest-updated";
        event.Subject = Detail::FirstPath(captured.contains("UpdatedQuest") ? captured["UpdatedQuest"] : captured);
    }
    else if (function == SkillXPFunction) {
        if (!captured.contains("CurrentXP") || !captured["CurrentXP"].is_number() || !captured.contains("PreviousXP") || !captured["PreviousXP"].is_number()) return std::nullopt;
        const double current = captured["CurrentXP"].get<double>(), previous = captured["PreviousXP"].get<double>();
        if (!std::isfinite(current) || !std::isfinite(previous) || current <= previous || previous < 0) return std::nullopt;
        event.Kind = "skill-xp"; event.Subject = captured.contains("SkillData") && captured["SkillData"].is_string() ? captured["SkillData"].get<std::string>() : std::string{};
        event.Amount = current - previous; event.Value = current;
    }
    else if (function == RespawnFunction) event.Kind = "death";
    else if (function == BuildFunction) { event.Kind = "building-placed"; event.Subject = Detail::FirstPath(captured); }
    else if (function == UseItemFunction) event.Kind = "item-used";
    else if (watched.contains(function)) { event.Kind = "custom"; event.Subject = std::string(function); }
    else return std::nullopt;
    if (!event.Subject.empty() && !Path(event.Subject, event.Kind == "custom")) event.Subject.clear();
    event.Parameters = Detail::Bounded(captured);
    return event;
}
inline Event Kill(std::string victimClass) {
    Event event; event.Kind = "kill"; event.Subject = Path(victimClass, false) ? std::move(victimClass) : std::string{}; return event;
}

// Mod-reported events. Owner is the mod's achievement owner exactly as Helpy forms it: "mods/<folder>" for a
// RuneSchema mod or "ue4ss/<folder>" for a UE4SS mod; matched case-insensitively and stored lower-case.
// A name uses the achievement id alphabet. Returns nothing when any input is invalid.
inline std::optional<std::string> ReportOwner(std::string_view owner) {
    std::string_view prefix;
    for (const std::string_view candidate : {std::string_view("mods/"), std::string_view("ue4ss/")})
        if (owner.size() > candidate.size() && std::equal(candidate.begin(), candidate.end(), owner.begin(),
            [](char a, char b) { return a == char(std::tolower(static_cast<unsigned char>(b))); })) { prefix = candidate; break; }
    if (prefix.empty() || owner.size() > 160) return std::nullopt;
    const auto folder = owner.substr(prefix.size());
    if (folder == "." || folder == ".." || folder.front() == ' ' || folder.back() == ' '
        || std::any_of(folder.begin(), folder.end(), [](unsigned char c) { return c < 32 || c == 127 || c == '/' || c == '\\' || c == ':'; })) return std::nullopt;
    std::string result(owner);
    for (auto& c : result) c = char(std::tolower(static_cast<unsigned char>(c)));
    return result;
}
inline bool ReportName(std::string_view name) {
    return !name.empty() && name.size() <= 64 && std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '_' || c == '.'; });
}
inline std::optional<Event> Report(std::string_view owner, std::string_view name, double amount, double value, std::string_view parameters = {}) {
    const auto normalized = ReportOwner(owner);
    if (!normalized || !ReportName(name) || !std::isfinite(amount) || amount <= 0 || amount > 1e9 || !std::isfinite(value)) return std::nullopt;
    Event event; event.Kind = "report"; event.Owner = *normalized; event.Subject = std::string(name); event.Amount = amount; event.Value = value;
    if (!parameters.empty()) {
        if (parameters.size() > MaximumReportInput) return std::nullopt;
        Json parsed = Json::parse(parameters, nullptr, false);
        if (parsed.is_discarded() || !parsed.is_object()) return std::nullopt;
        event.Parameters = Detail::Bounded(std::move(parsed));
    }
    return event;
}

inline Json ToJson(const Event& event) {
    Json result{{"Sequence", event.Sequence}, {"Kind", event.Kind}, {"Amount", event.Amount}, {"Time", event.Time}};
    if (!event.Subject.empty()) result["Subject"] = event.Subject;
    if (!event.Owner.empty()) result["Owner"] = event.Owner;
    if (event.Value != 0) result["Value"] = event.Value;
    if (!event.Parameters.empty()) result["Parameters"] = event.Parameters;
    return result;
}

class Feed {
    mutable std::mutex m_gate;
    std::deque<Event> m_events;
    uint64_t m_next = 1, m_session = 0;
    std::set<std::string, std::less<>> m_watched;
    std::atomic<uint64_t> m_watchRevision{0}; // Read lock-free on the game thread's ProcessEvent path.
public:
    explicit Feed(uint64_t session = 0) : m_session(session ? session : static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())) {}
    void Push(Event event) {
        if (!KnownKind(event.Kind) || !std::isfinite(event.Amount) || event.Amount <= 0 || event.Amount > 1e9 || !std::isfinite(event.Value)) return;
        std::lock_guard lock(m_gate);
        if (!event.Time) event.Time = Now();
        event.Sequence = m_next++;
        if (m_events.size() >= Capacity) m_events.pop_front();
        m_events.push_back(std::move(event));
    }
    // Events after `cursor`. A new Session or Dropped means the reader missed events and should resync.
    Json Since(uint64_t cursor, size_t maximum = MaximumRead) const {
        std::lock_guard lock(m_gate);
        const uint64_t first = m_events.empty() ? m_next : m_events.front().Sequence;
        Json events = Json::array();
        for (const auto& event : m_events) {
            if (event.Sequence <= cursor) continue;
            if (events.size() >= std::min(maximum, MaximumRead)) break;
            events.push_back(ToJson(event));
        }
        const uint64_t next = events.empty() ? std::max(cursor, m_next - 1) : events.back().at("Sequence").get<uint64_t>();
        return {{"Session", std::to_string(m_session)}, {"Events", std::move(events)}, {"Cursor", std::min<uint64_t>(next, m_next - 1)},
            {"Dropped", cursor + 1 < first && cursor != 0}, {"Watching", m_watched.size()}};
    }
    // Replaces the set of mod Blueprint functions to observe. Invalid paths are ignored; the result says which were kept.
    std::vector<std::string> Watch(const Json& functions) {
        std::set<std::string, std::less<>> next;
        if (functions.is_array()) for (const auto& value : functions) {
            if (next.size() >= MaximumWatch) break;
            if (!value.is_string()) continue;
            const auto& path = value.get_ref<const std::string&>();
            if (Path(path, true) && std::find(NativeFunctions.begin(), NativeFunctions.end(), path) == NativeFunctions.end()) next.insert(path);
        }
        std::lock_guard lock(m_gate);
        if (next != m_watched) { m_watched = std::move(next); m_watchRevision.fetch_add(1, std::memory_order_release); }
        return {m_watched.begin(), m_watched.end()};
    }
    std::set<std::string, std::less<>> Watched() const { std::lock_guard lock(m_gate); return m_watched; }
    uint64_t WatchRevision() const { return m_watchRevision.load(std::memory_order_acquire); }
    void Clear() { std::lock_guard lock(m_gate); m_events.clear(); }
};
inline Feed Current;

// RuneSchema_ReportModEventV1 (dllmain.cpp) forwards here. Callable from any thread; never throws.
// Returns RS_MOD_OK (0), RS_MOD_INVALID_ARGUMENT (1) or RS_MOD_FAILED (6), as in shared/RuneSchemaModApi.h.
inline constexpr int32_t ReportOk = 0, ReportInvalidArgument = 1, ReportFailed = 6;
inline int32_t ReportV1(Feed& feed, const char* owner, const char* name, double amount, double value, const char* parametersJson) noexcept {
    try {
        if (!owner || !name) return ReportInvalidArgument;
        // Bounded scans: an unterminated or oversized string is rejected without reading past the limit.
        const auto bounded = [](const char* text, size_t limit) -> std::optional<std::string_view> {
            size_t size = 0; while (size <= limit && text[size]) ++size;
            if (size > limit) return std::nullopt;
            return std::string_view(text, size);
        };
        const auto ownerText = bounded(owner, 160), nameText = bounded(name, 64);
        std::optional<std::string_view> parameters = std::string_view{};
        if (parametersJson) parameters = bounded(parametersJson, MaximumReportInput);
        if (!ownerText || !nameText || !parameters) return ReportInvalidArgument;
        auto event = Report(*ownerText, *nameText, amount, value, *parameters);
        if (!event) return ReportInvalidArgument;
        feed.Push(std::move(*event));
        return ReportOk;
    } catch (...) { return ReportFailed; }
}

// Service handler: {"Action":"GameplayEvents","Cursor":n,"Watch":[...]?} -> Since(cursor) plus accepted watches.
inline Json Service(const Json& request, Feed& feed = Current) {
    uint64_t cursor = 0;
    if (request.contains("Cursor")) { if (!request["Cursor"].is_number_unsigned() && !request["Cursor"].is_number_integer()) throw std::runtime_error("Invalid gameplay event cursor"); cursor = request["Cursor"].get<uint64_t>(); }
    std::vector<std::string> accepted;
    const bool watch = request.contains("Watch");
    if (watch) accepted = feed.Watch(request["Watch"]);
    auto result = feed.Since(cursor);
    if (watch) result["Watched"] = accepted;
    return result;
}
}
