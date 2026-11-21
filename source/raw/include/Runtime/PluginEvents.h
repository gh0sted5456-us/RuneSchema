#pragma once
// Owned plugin events (E8): a Host=helpy plugin's in-game half reports something
// (VaultOpened, InventoryChanged, CharacterChanged, ...) to its own Helpy-side
// half. Core tags the event with the caller, Helpy.exe hands it only to the
// owner's "<Id>.events" service, and an optional Open selects only a route that
// owner registered. Events never authorize or perform anything by themselves.
#include "Runtime/PluginCatalog.h"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>

namespace PS::PluginEvents {
inline constexpr auto Service = "helpy.plugin.event";
inline constexpr auto Kind = "plugin-event";
inline constexpr size_t MaxPayloadBytes = 16 * 1024;
inline constexpr double Burst = 16, RefillPerSecond = 8;

inline std::string EventsService(const std::string& owner) { return owner + ".events"; }
// "<X>.events" is reserved for plugin X in Helpy.exe.
inline bool ServiceNameAllowed(const std::string& owner, const std::string& name) {
    return !name.ends_with(".events") || name == EventsService(owner);
}
inline bool Route(const std::string& owner, const std::string& route) {
    const auto prefix = owner + ".";
    return route.size() > prefix.size() && route.size() <= 192 && route.starts_with(prefix)
        && std::all_of(route.begin(), route.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'; });
}
// Validates a game-half request and returns the owner-tagged event to push.
inline nlohmann::json Build(const std::string& owner, const std::string& request, uint64_t sequence) {
    if (!PluginCatalog::Token(owner)) throw std::runtime_error("invalid event owner");
    if (request.size() > MaxPayloadBytes + 1024) throw std::runtime_error("plugin event is too large");
    const auto input = nlohmann::json::parse(request, [](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
        if (depth > 16) throw std::runtime_error("plugin event is nested too deeply"); return true; });
    if (!input.is_object()) throw std::runtime_error("plugin event must be an object");
    const auto name = input.value("Event", std::string{});
    if (name.empty() || name.size() > 64 || !PluginCatalog::Token(name)) throw std::runtime_error("plugin event needs an Event token");
    nlohmann::json event = {{"Kind", Kind}, {"Owner", owner}, {"Event", name}, {"Sequence", sequence}};
    if (input.contains("Payload")) {
        if (!input["Payload"].is_object() || input["Payload"].dump().size() > MaxPayloadBytes) throw std::runtime_error("plugin event Payload must be an object of at most 16 KiB");
        event["Payload"] = input["Payload"];
    } else event["Payload"] = nlohmann::json::object();
    if (input.contains("Open")) {
        if (!input["Open"].is_string() || !Route(owner, input["Open"].get<std::string>())) throw std::runtime_error("Open must name one of the caller's own routes");
        event["Open"] = input["Open"];
    }
    return event;
}
// Helpy.exe side: an event is delivered only when it is well formed for its owner.
inline bool Deliverable(const nlohmann::json& event) {
    if (!event.is_object() || event.value("Kind", std::string{}) != Kind) return false;
    const auto owner = event.value("Owner", std::string{}), name = event.value("Event", std::string{});
    if (!PluginCatalog::Token(owner) || owner == "RuneSchema.Core" || owner == "RuneSchema.Helpy" || !PluginCatalog::Token(name)) return false;
    if (event.contains("Open") && (!event["Open"].is_string() || !Route(owner, event["Open"].get<std::string>()))) return false;
    return !event.contains("Payload") || event["Payload"].is_object();
}
// Per-owner token bucket: bounded and nonblocking; an empty bucket refuses the event.
struct Bucket { double Tokens = Burst; uint64_t LastMs = 0; uint64_t Sequence = 0; bool Started = false; };
inline bool Take(Bucket& bucket, uint64_t nowMs) {
    if (bucket.Started && nowMs > bucket.LastMs) bucket.Tokens = (std::min)(Burst, bucket.Tokens + (nowMs - bucket.LastMs) * RefillPerSecond / 1000.0);
    bucket.Started = true; bucket.LastMs = nowMs;
    if (bucket.Tokens < 1) return false;
    bucket.Tokens -= 1; ++bucket.Sequence; return true;
}
}
