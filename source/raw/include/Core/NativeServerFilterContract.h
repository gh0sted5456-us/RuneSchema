#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace PS::NativeServerFilterContract {
struct Build { std::uint32_t Stamp, ImageSize, ApplyAddress; };
inline constexpr std::array Builds{
    Build{0x30e25127, 0x0c7fb000, 0x05958ff0}, // CL-254097
    Build{0x194c6c05, 0x0c819000, 0x05961180}, // CL-258140
    Build{0xeee4a2f3, 0x0c823000, 0x05962540}, // CL-260223
};
inline constexpr std::array<unsigned char,32> ApplyCode{
    0x40,0x55,0x56,0x41,0x55,0x48,0x8d,0x6c,0x24,0xb9,0x48,0x81,0xec,0xa0,0x00,0x00,
    0x00,0x48,0x8b,0xf1,0x4d,0x85,0xc0,0x0f,0x84,0x37,0x02,0x00,0x00,0x41,0x8b,0x40};
inline std::uint32_t ResolveApplyAddress(std::uint32_t stamp,std::uint32_t size) {
    for(const auto& build:Builds)
        if(stamp==build.Stamp && size==build.ImageSize)return build.ApplyAddress;
    throw std::runtime_error("This dedicated-server build's native filter setup has not been verified");
}
inline void Verify(std::span<const unsigned char> code) {
    if(code.size()!=ApplyCode.size() || !std::equal(code.begin(),code.end(),ApplyCode.begin()))
        throw std::runtime_error("The native server filter settings method changed");
}
static_assert(std::ranges::all_of(Builds,[](const auto& build){
    return build.ApplyAddress+ApplyCode.size()<=build.ImageSize;
}));
}
