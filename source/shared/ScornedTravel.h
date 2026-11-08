#pragma once
#include "Core/MapProjection.h"
#include <string_view>

namespace PS::ScornedTravel {
inline constexpr std::string_view World = "/Game/Maps/World/L_World.L_World";
inline constexpr auto NativeTarget = L"ScornedWilderness_Entrance";
// Cooked L_World TeleportationTarget and its root capsule transform.
inline constexpr MapProjection::Point Entrance{-705215.5616758744, 770592.6120797807};
inline constexpr double EntranceZ = 13144.788235683933;
inline bool Matches(std::string_view world, MapProjection::Point point) {
    return world == World && std::abs(point.X - Entrance.X) < .01 &&
        std::abs(point.Y - Entrance.Y) < .01;
}
inline MapProjection::Projection LandingArea(MapProjection::Point live, double z) {
    if (!std::isfinite(z) || !(std::hypot(live.X - Entrance.X, live.Y - Entrance.Y) < 250) ||
        std::abs(z - EntranceZ) >= 250)
        throw std::runtime_error("The native Rasthin's Rest entrance moved. Use the in-game Dragon Gate.");
    return {live, {1, 0}, {0, -1}, 600, 600, z - 600, z + 600};
}
}
