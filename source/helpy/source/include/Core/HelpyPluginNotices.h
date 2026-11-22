#pragma once
// helpy.notify: a Helpy-side plugin posts a notice into Helpy's notifications,
// optionally with an in-game toast and a route of its own to open. A notice
// never runs anything; Open only selects a route that plugin registered.
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace PS::HelpyPluginNotices {
inline constexpr auto Service = "helpy.notify";
inline constexpr size_t MaxPending = 64;
inline constexpr double Burst = 16, RefillPerSecond = 1;
struct Notice { std::string Owner, Id, Title, Detail, Route; bool Toast = true; };
struct Bucket { double Tokens = Burst; uint64_t LastMs = 0; bool Started = false; };

inline bool Plain(const std::string& text, size_t limit, bool empty = false) {
    return (empty || !text.empty()) && text.size() <= limit
        && std::none_of(text.begin(), text.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
inline bool Token(const std::string& text, size_t limit) {
    return !text.empty() && text.size() <= limit && std::all_of(text.begin(), text.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-' || c == ':'; });
}
// {"Id":"sale-123","Title":"Listing sold","Detail":"...","Open":"<owner>.route","Toast":true}
inline Notice Parse(const std::string& owner, std::string_view input) {
    if (!Token(owner, 128) || input.size() > 8192) throw std::runtime_error("invalid notice");
    const auto value = nlohmann::json::parse(input, [](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
        if (depth > 4) throw std::runtime_error("notice is nested too deeply"); return true; });
    if (!value.is_object()) throw std::runtime_error("notice must be an object");
    Notice notice{owner, value.at("Id").get<std::string>(), value.at("Title").get<std::string>(),
        value.value("Detail", std::string{}), value.value("Open", std::string{}), value.value("Toast", true)};
    if (!Token(notice.Id, 128) || !Plain(notice.Title, 120) || !Plain(notice.Detail, 400, true))
        throw std::runtime_error("notice needs an Id token, a Title of at most 120 characters and a Detail of at most 400");
    const auto prefix = owner + ".";
    if (!notice.Route.empty() && (notice.Route.size() <= prefix.size() || notice.Route.size() > 192
        || !notice.Route.starts_with(prefix) || notice.Route.find(':') != std::string::npos || !Token(notice.Route, 192)))
        throw std::runtime_error("Open must name one of the caller's own routes");
    return notice;
}
inline bool Take(Bucket& bucket, uint64_t nowMs) {
    if (bucket.Started && nowMs > bucket.LastMs)
        bucket.Tokens = (std::min)(Burst, bucket.Tokens + (nowMs - bucket.LastMs) * RefillPerSecond / 1000.0);
    bucket.Started = true; bucket.LastMs = nowMs;
    if (bucket.Tokens < 1) return false;
    bucket.Tokens -= 1; return true;
}
class Queue {
    std::mutex m_gate; std::deque<Notice> m_pending; std::map<std::string, Bucket> m_buckets;
public:
    // true = queued. Throws on an invalid notice; false = over the owner's rate or the queue is full.
    bool Accept(const std::string& owner, std::string_view input, uint64_t nowMs) {
        auto notice = Parse(owner, input);
        std::lock_guard lock(m_gate);
        auto& bucket = m_buckets[owner]; auto probe = bucket;
        if (m_pending.size() >= MaxPending || !Take(probe, nowMs)) return false;
        bucket = probe; m_pending.push_back(std::move(notice)); return true;
    }
    std::deque<Notice> Drain() { std::lock_guard lock(m_gate); return std::exchange(m_pending, {}); }
    void Forget(const std::string& owner) {
        std::lock_guard lock(m_gate); m_buckets.erase(owner);
        std::erase_if(m_pending, [&](const Notice& notice) { return notice.Owner == owner; });
    }
};
inline Queue& Pending() { static Queue queue; return queue; }
}
