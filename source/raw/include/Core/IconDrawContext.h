#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

namespace PS::IconCapture {
// Only for the engine's DrawToRenderTargetContext, after checking its live
// struct identity and NoDestructor contract. It contains opaque native state
// (not just UPROPERTY fields) which EndDraw must receive exactly once.
inline void TransferDrawContext(std::span<std::uint8_t> destination,
    std::span<std::uint8_t> source,bool noDestructor) {
    if(!noDestructor||source.empty()||source.size()>256||destination.size()!=source.size())
        throw std::runtime_error("Item icon native draw context layout changed");
    const auto from=reinterpret_cast<std::uintptr_t>(source.data());
    const auto to=reinterpret_cast<std::uintptr_t>(destination.data());
    if(from==to||(from<to?to-from<source.size():from-to<source.size()))
        throw std::runtime_error("Item icon draw context storage overlaps");
    // The EndDraw frame must still hold its empty, freshly initialized context.
    for(auto byte:destination)if(byte)throw std::runtime_error("Item icon draw context destination is not empty");
    std::memcpy(destination.data(),source.data(),source.size());
    std::memset(source.data(),0,source.size());
}
}
