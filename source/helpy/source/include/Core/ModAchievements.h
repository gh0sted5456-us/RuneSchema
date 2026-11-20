#pragma once
// Mod achievements: definitions from each mod's cache/achievements.json, profile-wide progress, points,
// and the bounded summary other players see. Portable: no Win32 or Unreal types.
//
//   <mod>/cache/achievements.json
//   {"format":1,"achievements":[{"id":"goblin-slayer","title":"Goblin Slayer","description":"Defeat 25 goblins.",
//     "points":10,"hidden":false,"conditions":[{"event":"kill","subject":"/Game/Mods/Goblins/*","count":25}]}]}
//
// Events come from the RuneSchema core feed (Core/GameplayEventFeed.h in raw/). Progress belongs to the
// RuneSchema profile, not a character, and is stored locally: points are an honour system.
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace PS::ModAchievements {
using Json = nlohmann::json;
inline constexpr std::array<std::string_view, 11> Events{"kill", "craft", "recipe-unlocked", "building-unlocked", "quest-updated",
    "skill-xp", "death", "building-placed", "item-used", "custom", "report"};
inline constexpr size_t MaximumPerMod = 256, MaximumConditions = 8, MaximumWhere = 8, MaximumModPoints = 1000;
inline constexpr int MaximumPoints = 100;

inline bool Visible(std::string_view text, size_t limit) {
    return !text.empty() && text.size() <= limit && text.front() != ' ' && text.back() != ' '
        && std::none_of(text.begin(), text.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
inline bool Identifier(std::string_view text) {
    return !text.empty() && text.size() <= 64 && std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '_' || c == '.'; });
}
// "/Game/..." paths, optional trailing "*" for a prefix match. Functions need ":Name".
inline bool Pattern(std::string_view text, bool function) {
    if (text.ends_with('*')) text.remove_suffix(1);
    if (text.empty() || text.size() > 512 || text.front() != '/' || text.find("..") != text.npos) return false;
    if (function && (text.find(':') == text.npos || text.find('*') != text.npos)) return false;
    return std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isalnum(c) || c == '/' || c == '_' || c == '.' || c == ':' || c == '-'; });
}
// "mods/<folder>" for RuneSchema mods, "ue4ss/<folder>" for UE4SS mods (see Core/AchievementSources.h).
inline std::string_view OwnerPrefix(std::string_view owner) {
    for (const std::string_view prefix : {std::string_view("mods/"), std::string_view("ue4ss/")})
        if (owner.starts_with(prefix) && owner.size() > prefix.size()) return prefix;
    return {};
}
inline bool Owner(std::string_view owner) { return !OwnerPrefix(owner).empty() && owner.size() <= 160 && Visible(owner, 160); }
// The folder part of a valid owner, shown when a file gives no usable mod name.
inline std::string OwnerFolder(const std::string& owner) { return owner.substr(OwnerPrefix(owner).size()); }

struct Condition {
    std::string Event, Subject; // Subject: exact path, "prefix*", or empty for any. For "report": the report name.
    double Count = 1;           // Sum of event amounts needed (kills, crafts, XP gained...).
    double AtLeast = 0;         // skill-xp and report: reach this reported value instead of adding up Count.
    Json Where = Json::object(); // Dotted parameter paths -> expected scalar.
};
struct Definition {
    std::string Owner, ModName, Id, Title, Description;
    int Points = 0; bool Hidden = false, Any = false;
    std::vector<Condition> Conditions;
    std::string Key() const { return Owner + "|" + Id; }
};

namespace Detail {
inline const Json* Field(const Json& root, std::string_view path) {
    const Json* node = &root;
    while (!path.empty()) {
        const auto dot = path.find('.');
        const std::string key(path.substr(0, dot));
        if (!node->is_object() || !node->contains(key)) return nullptr;
        node = &(*node)[key];
        if (dot == path.npos) break;
        path.remove_prefix(dot + 1);
    }
    return node;
}
inline bool SubjectMatches(const std::string& pattern, const std::string& subject) {
    if (pattern.empty()) return true;
    if (pattern.ends_with('*')) return subject.starts_with(std::string_view(pattern).substr(0, pattern.size() - 1));
    return subject == pattern;
}
inline double Number(const Json& value, const char* name, double fallback, double low, double high) {
    if (!value.contains(name)) return fallback;
    if (!value[name].is_number()) throw std::runtime_error(std::string("Achievement ") + name + " must be a number");
    const auto number = value[name].get<double>();
    if (!std::isfinite(number) || number < low || number > high) throw std::runtime_error(std::string("Achievement ") + name + " is out of range");
    return number;
}
}

// Parses one mod's achievements.json. Throws with a modder-readable message on the first problem.
inline std::vector<Definition> Parse(const Json& document, const std::string& owner, const std::string& modName) {
    if (!Owner(owner)) throw std::runtime_error("Invalid achievement owner");
    if (!document.is_object() || document.value("format", 0) != 1 || !document.contains("achievements") || !document["achievements"].is_array())
        throw std::runtime_error("achievements.json needs {\"format\":1,\"achievements\":[...]}");
    const auto& rows = document["achievements"];
    if (rows.size() > MaximumPerMod) throw std::runtime_error("A mod may define at most 256 achievements");
    std::vector<Definition> result; std::set<std::string> ids; int total = 0;
    for (const auto& row : rows) {
        if (!row.is_object()) throw std::runtime_error("Each achievement must be an object");
        Definition definition; definition.Owner = owner; definition.ModName = Visible(modName, 96) ? modName : OwnerFolder(owner);
        definition.Id = row.value("id", std::string{});
        if (!Identifier(definition.Id) || !ids.insert(definition.Id).second) throw std::runtime_error("Achievement id must be unique, 1-64 letters, digits, '-', '_' or '.': " + definition.Id);
        const auto context = " (" + definition.Id + ")";
        definition.Title = row.value("title", std::string{});
        if (!Visible(definition.Title, 64)) throw std::runtime_error("Achievement title must be 1-64 visible characters" + context);
        definition.Description = row.value("description", std::string{});
        if (!definition.Description.empty() && !Visible(definition.Description, 200)) throw std::runtime_error("Achievement description must be at most 200 visible characters" + context);
        definition.Points = int(Detail::Number(row, "points", 0, 0, MaximumPoints));
        total += definition.Points;
        if (total > int(MaximumModPoints)) throw std::runtime_error("A mod's achievements may award at most 1000 points in total");
        definition.Hidden = row.value("hidden", false);
        definition.Any = row.value("match", std::string("all")) == "any";
        if (row.contains("match") && row["match"] != "all" && row["match"] != "any") throw std::runtime_error("Achievement match must be \"all\" or \"any\"" + context);
        if (!row.contains("conditions") || !row["conditions"].is_array() || row["conditions"].empty() || row["conditions"].size() > MaximumConditions)
            throw std::runtime_error("Achievement needs 1-8 conditions" + context);
        for (const auto& value : row["conditions"]) {
            if (!value.is_object()) throw std::runtime_error("Each condition must be an object" + context);
            Condition condition; condition.Event = value.value("event", std::string{});
            if (std::find(Events.begin(), Events.end(), condition.Event) == Events.end()) throw std::runtime_error("Unknown condition event \"" + condition.Event + "\"" + context);
            const bool custom = condition.Event == "custom", report = condition.Event == "report";
            if (report) {
                // Reported by the mod itself (RuneSchema_ReportModEventV1) and matched only for this owner.
                if (value.contains("subject") || value.contains("function")) throw std::runtime_error("A report condition uses \"name\", not \"subject\" or \"function\"" + context);
                condition.Subject = value.contains("name") && value["name"].is_string() ? value["name"].get<std::string>() : std::string{};
                if (!Identifier(condition.Subject)) throw std::runtime_error("A report condition needs \"name\": 1-64 letters, digits, '-', '_' or '.'" + context);
            } else {
                condition.Subject = value.value(custom ? "function" : "subject", std::string{});
                if (custom && condition.Subject.empty()) throw std::runtime_error("A custom condition needs \"function\": \"/Game/...:EventName\"" + context);
                if (!condition.Subject.empty() && !Pattern(condition.Subject, custom)) throw std::runtime_error("Invalid condition path \"" + condition.Subject + "\"" + context);
            }
            condition.Count = Detail::Number(value, "count", 1, 1, 1e9);
            condition.AtLeast = Detail::Number(value, "atLeast", 0, 0, 1e12);
            if (condition.AtLeast > 0 && condition.Event != "skill-xp" && !report) throw std::runtime_error("atLeast applies only to skill-xp and report" + context);
            if (value.contains("where")) {
                const auto& where = value["where"];
                if (!where.is_object() || where.size() > MaximumWhere) throw std::runtime_error("where must be an object with at most 8 fields" + context);
                for (const auto& [path, expected] : where.items())
                    if (path.empty() || path.size() > 128 || !(expected.is_boolean() || expected.is_number() || expected.is_string()) || (expected.is_string() && expected.get_ref<const std::string&>().size() > 512))
                        throw std::runtime_error("where values must be a boolean, number or string" + context);
                condition.Where = where;
            }
            definition.Conditions.push_back(std::move(condition));
        }
        result.push_back(std::move(definition));
    }
    return result;
}

// `owner` is the definition's owner. A report event counts only for the mod that reported it, so one mod can
// never complete another's achievements; other kinds carry no owner and are unchanged.
inline bool Matches(const Condition& condition, const Json& event, std::string_view owner) {
    if (!event.is_object() || event.value("Kind", std::string{}) != condition.Event) return false;
    if (condition.Event == "report") {
        if (owner.empty() || !event.contains("Owner") || !event["Owner"].is_string() || event["Owner"].get_ref<const std::string&>() != owner) return false;
        if (event.value("Subject", std::string{}) != condition.Subject) return false;
    } else if (!Detail::SubjectMatches(condition.Subject, event.value("Subject", std::string{}))) return false;
    if (!condition.Where.empty()) {
        const auto parameters = event.contains("Parameters") ? event["Parameters"] : Json::object();
        for (const auto& [path, expected] : condition.Where.items()) {
            const auto* actual = Detail::Field(parameters, path);
            if (!actual || *actual != expected) return false;
        }
    }
    return true;
}

struct Unlock { std::string Key, Owner, ModName, Title, Description; int Points = 0; uint64_t Time = 0; };

// Profile-wide progress plus the core feed position, persisted as runeschema.profile/achievements.json.
class Progress {
public:
    struct Entry { std::vector<double> Counters; uint64_t Unlocked = 0; int Points = 0; std::string Title, Owner, ModName; };
    std::map<std::string, Entry> Entries;
    std::string Session; uint64_t Cursor = 0;
    uint64_t Revision = 0;

    // Applies one feed event to every definition. Returns achievements unlocked by it.
    std::vector<Unlock> Apply(const std::vector<Definition>& definitions, const Json& event) {
        std::vector<Unlock> unlocked;
        const double amount = event.contains("Amount") && event["Amount"].is_number() ? event["Amount"].get<double>() : 1;
        const double value = event.contains("Value") && event["Value"].is_number() ? event["Value"].get<double>() : 0;
        if (!std::isfinite(amount) || amount <= 0 || !std::isfinite(value)) return unlocked;
        const uint64_t time = event.contains("Time") && event["Time"].is_number_unsigned() ? event["Time"].get<uint64_t>() : 0;
        for (const auto& definition : definitions) {
            auto found = Entries.find(definition.Key());
            if (found != Entries.end() && found->second.Unlocked) continue;
            bool touched = false;
            for (size_t index = 0; index < definition.Conditions.size(); ++index) {
                const auto& condition = definition.Conditions[index];
                if (!Matches(condition, event, definition.Owner)) continue;
                if (found == Entries.end()) found = Entries.emplace(definition.Key(), Entry{}).first;
                auto& counters = found->second.Counters;
                counters.resize(definition.Conditions.size(), 0);
                const double next = condition.AtLeast > 0 ? std::max(counters[index], value) : std::min(1e12, counters[index] + amount);
                if (next != counters[index]) { counters[index] = next; touched = true; }
            }
            if (!touched) continue;
            ++Revision;
            if (!Complete(definition, found->second)) continue;
            auto& entry = found->second;
            entry.Unlocked = time ? time : 1; entry.Points = definition.Points; entry.Title = definition.Title; entry.Owner = definition.Owner; entry.ModName = definition.ModName;
            unlocked.push_back({definition.Key(), definition.Owner, definition.ModName, definition.Title, definition.Description, definition.Points, entry.Unlocked});
        }
        return unlocked;
    }
    static bool Met(const Condition& condition, double counter) { return condition.AtLeast > 0 ? counter >= condition.AtLeast : counter >= condition.Count; }
    static bool Complete(const Definition& definition, const Entry& entry) {
        size_t met = 0;
        for (size_t index = 0; index < definition.Conditions.size(); ++index)
            met += index < entry.Counters.size() && Met(definition.Conditions[index], entry.Counters[index]) ? 1 : 0;
        return definition.Any ? met > 0 : met == definition.Conditions.size();
    }
    // 0..1 for progress bars; counts each condition's share.
    double Fraction(const Definition& definition) const {
        const auto found = Entries.find(definition.Key());
        if (found == Entries.end()) return 0;
        if (found->second.Unlocked) return 1;
        double best = 0, sum = 0;
        for (size_t index = 0; index < definition.Conditions.size(); ++index) {
            const auto& condition = definition.Conditions[index];
            const double counter = index < found->second.Counters.size() ? found->second.Counters[index] : 0;
            const double share = std::clamp(counter / (condition.AtLeast > 0 ? condition.AtLeast : condition.Count), 0.0, 1.0);
            best = std::max(best, share); sum += share;
        }
        return definition.Any ? best : sum / double(std::max<size_t>(1, definition.Conditions.size()));
    }
    bool Unlocked(const std::string& key) const { const auto found = Entries.find(key); return found != Entries.end() && found->second.Unlocked; }
    // Points are snapshotted at unlock, so disabling or updating a mod never changes earned totals.
    int Points() const { int total = 0; for (const auto& [key, entry] : Entries) if (entry.Unlocked) total += std::clamp(entry.Points, 0, MaximumPoints); return total; }
    size_t UnlockedCount() const { return size_t(std::count_if(Entries.begin(), Entries.end(), [](const auto& item) { return item.second.Unlocked != 0; })); }

    // Feed bookkeeping. A new core session restarts at cursor 0; events from the old session were already applied.
    bool Resume(const std::string& session) { if (session == Session) return false; Session = session; Cursor = 0; ++Revision; return true; }

    Json Document() const {
        Json entries = Json::object();
        for (const auto& [key, entry] : Entries) {
            Json row{{"Counters", entry.Counters}};
            if (entry.Unlocked) { row["Unlocked"] = entry.Unlocked; row["Points"] = entry.Points; row["Title"] = entry.Title; row["Owner"] = entry.Owner; row["ModName"] = entry.ModName; }
            entries[key] = std::move(row);
        }
        return {{"Format", 1}, {"Session", Session}, {"Cursor", Cursor}, {"Entries", std::move(entries)}};
    }
    static Progress Parse(const Json& document) {
        if (!document.is_object() || document.value("Format", 0) != 1 || !document.contains("Entries") || !document["Entries"].is_object() || document["Entries"].size() > 16384)
            throw std::runtime_error("Achievement progress has an invalid shape");
        Progress result;
        result.Session = document.value("Session", std::string{}); if (result.Session.size() > 64) result.Session.clear();
        result.Cursor = document.contains("Cursor") && document["Cursor"].is_number_unsigned() ? document["Cursor"].get<uint64_t>() : 0;
        for (const auto& [key, row] : document["Entries"].items()) {
            const auto bar = key.find('|');
            if (bar == key.npos || !Owner(key.substr(0, bar)) || !Identifier(key.substr(bar + 1)) || !row.is_object()) throw std::runtime_error("Invalid achievement progress key");
            Entry entry;
            const auto& counters = row.at("Counters");
            if (!counters.is_array() || counters.size() > MaximumConditions) throw std::runtime_error("Invalid achievement counters");
            for (const auto& counter : counters) { if (!counter.is_number() || !std::isfinite(counter.get<double>()) || counter.get<double>() < 0) throw std::runtime_error("Invalid achievement counter"); entry.Counters.push_back(counter.get<double>()); }
            if (row.contains("Unlocked")) {
                entry.Unlocked = row.at("Unlocked").get<uint64_t>();
                entry.Points = std::clamp(row.value("Points", 0), 0, MaximumPoints);
                entry.Title = row.value("Title", std::string{}); if (!Visible(entry.Title, 64)) entry.Title = key.substr(bar + 1);
                entry.Owner = key.substr(0, bar);
                entry.ModName = row.value("ModName", std::string{}); if (!Visible(entry.ModName, 96)) entry.ModName = OwnerFolder(entry.Owner);
            }
            result.Entries.emplace(key, std::move(entry));
        }
        return result;
    }
};

// Custom Blueprint functions the core should watch for these definitions.
inline Json WatchList(const std::vector<Definition>& definitions) {
    std::set<std::string> functions;
    for (const auto& definition : definitions) for (const auto& condition : definition.Conditions)
        if (condition.Event == "custom" && functions.size() < 64) functions.insert(condition.Subject);
    return Json(functions);
}

// What other players see: totals plus the top mods and the latest unlocks. Kept well under 1 KiB.
inline constexpr size_t SummaryBytes = 1024;
inline Json Summary(const Progress& progress) {
    struct ModRow { std::string Name; int Points = 0, Unlocked = 0; };
    std::map<std::string, ModRow> mods; std::vector<const Progress::Entry*> recent;
    for (const auto& [key, entry] : progress.Entries) {
        if (!entry.Unlocked) continue;
        auto& row = mods[entry.Owner]; row.Name = entry.ModName; row.Points += entry.Points; ++row.Unlocked;
        recent.push_back(&entry);
    }
    std::vector<std::pair<std::string, ModRow>> ranked(mods.begin(), mods.end());
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.second.Points > b.second.Points; });
    std::sort(recent.begin(), recent.end(), [](const auto* a, const auto* b) { return a->Unlocked > b->Unlocked; });
    Json summary{{"Format", 1}, {"Points", progress.Points()}, {"Unlocked", progress.UnlockedCount()}, {"Mods", Json::array()}, {"Recent", Json::array()}};
    for (size_t i = 0; i < ranked.size() && i < 5; ++i)
        summary["Mods"].push_back({{"Name", ranked[i].second.Name}, {"Points", ranked[i].second.Points}, {"Unlocked", ranked[i].second.Unlocked}});
    for (size_t i = 0; i < recent.size() && i < 3; ++i)
        summary["Recent"].push_back({{"Title", recent[i]->Title}, {"Mod", recent[i]->ModName}, {"Points", recent[i]->Points}});
    while (summary.dump().size() > SummaryBytes && !summary["Recent"].empty()) summary["Recent"].erase(summary["Recent"].size() - 1);
    while (summary.dump().size() > SummaryBytes && !summary["Mods"].empty()) summary["Mods"].erase(summary["Mods"].size() - 1);
    return summary;
}
// Validates a summary received from another player. Returns null when absent or invalid.
inline Json ValidateSummary(const Json& value) {
    if (value.is_null()) return value;
    if (!value.is_object() || value.dump().size() > SummaryBytes || value.value("Format", 0) != 1) return nullptr;
    const auto count = [](const Json& row, const char* name, int64_t high) { return row.contains(name) && row[name].is_number_integer() && row[name].get<int64_t>() >= 0 && row[name].get<int64_t>() <= high; };
    if (!count(value, "Points", 1000000) || !count(value, "Unlocked", 100000) || !value.contains("Mods") || !value["Mods"].is_array() || value["Mods"].size() > 5
        || !value.contains("Recent") || !value["Recent"].is_array() || value["Recent"].size() > 3) return nullptr;
    for (const auto& row : value["Mods"]) if (!row.is_object() || row.size() != 3 || !row.contains("Name") || !row["Name"].is_string() || !Visible(row["Name"].get<std::string>(), 96) || !count(row, "Points", 1000000) || !count(row, "Unlocked", 100000)) return nullptr;
    for (const auto& row : value["Recent"]) if (!row.is_object() || row.size() != 3 || !row.contains("Title") || !row["Title"].is_string() || !Visible(row["Title"].get<std::string>(), 64)
        || !row.contains("Mod") || !row["Mod"].is_string() || !Visible(row["Mod"].get<std::string>(), 96) || !count(row, "Points", MaximumPoints)) return nullptr;
    if (value.size() != 5) return nullptr;
    return value;
}
}
