#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace DragonWilds::VendorIdentity {
// Separate from /spawns' 0x48435352 magic: its orphan sweep must never own vendors.
inline constexpr std::uint32_t Magic = 0x56435352;
using Words = std::array<std::uint32_t, 4>;
inline bool RetirementConfirmed(bool sameObjectSlot,bool usable) {
    return !sameObjectSlot || !usable;
}
inline bool RetiredInstanceMatches(const void* retired,const void* current,
    int retiredIndex,int currentIndex,int retiredSerial,int currentSerial,bool samePath) {
    return retired && current==retired && retiredIndex>=0 && retiredIndex==currentIndex
        && retiredSerial==currentSerial && samePath;
}
inline Words ForOwner(std::string_view owner) {
    Words result{2166136261u, 2166136261u ^ 0x9E3779B9u,
        2166136261u ^ 0x85EBCA6Bu, 2166136261u ^ 0xC2B2AE35u};
    for (unsigned char byte : owner)
        for (auto& lane : result) lane = (lane ^ byte) * 16777619u;
    result[0] = Magic;
    return result;
}
}
