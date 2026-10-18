#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace RC::Unreal { class UWorld; }

namespace PS::ItemIconThumbnail {
    int CacheLimitMB(); // Same persisted preference used by capture/eviction.
    // Game-thread, read-only cache lookup; never resets an in-flight capture,
    // touches timestamps, scans the cache or requires a menu/world renderer.
    std::optional<nlohmann::json> Cached(const std::string& IconPath);
    nlohmann::json Render(RC::Unreal::UWorld* World, const std::string& IconPath);
    void Reset() noexcept; // Game-thread only; releases pending capture leases.
    void ShutdownDiskCache() noexcept; // Join filesystem worker before DLL unload, not on world transitions.
}
