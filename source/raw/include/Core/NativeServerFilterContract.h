#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace PS::NativeServerFilterContract {
inline constexpr std::uint32_t Stamp=0x30e25127, ImageSize=0x0c7fb000, ApplyAddress=0x5958ff0;
inline constexpr std::array<unsigned char,32> ApplyCode{
    0x40,0x55,0x56,0x41,0x55,0x48,0x8d,0x6c,0x24,0xb9,0x48,0x81,0xec,0xa0,0x00,0x00,
    0x00,0x48,0x8b,0xf1,0x4d,0x85,0xc0,0x0f,0x84,0x37,0x02,0x00,0x00,0x41,0x8b,0x40};
inline bool Supports(std::uint32_t stamp,std::uint32_t size) {return stamp==Stamp && size==ImageSize;}
inline void Verify(std::uint32_t stamp,std::uint32_t size,std::span<const unsigned char> code) {
    if(!Supports(stamp,size))throw std::runtime_error("This dedicated-server build's native filter setup has not been verified");
    if(code.size()!=ApplyCode.size() || !std::equal(code.begin(),code.end(),ApplyCode.begin()))
        throw std::runtime_error("The native server filter settings method changed");
}
static_assert(ApplyAddress+ApplyCode.size()<ImageSize);
}
