#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <span>
#include "Core/NetworkIdentitySync.h"

namespace PS::NativeWelcomeLayout {
    inline constexpr unsigned char SteamCode[]{0x48,0x8b,0x8e,0xa8,0x01,0x00,0x00,0x4c,0x8d,0x45,0x90,0x48,0x8b,0xd3,0x48,0x8b,0x01,0xff,0x90,0x60,0x08,0x00,0x00};
    inline constexpr unsigned char GamePassCode[]{0x4c,0x8d,0x45,0x90,0x48,0x8b,0x8e,0xa8,0x01,0x00,0x00,0x49,0x8b,0xd6,0x48,0x8b,0x5c,0x24,0x60,0x89,0x44,0x24,0x58,0x8b,0x44,0x24,0x6c,0x89,0x44,0x24,0x5c,0x48,0x8b,0x01,0x48,0x89,0x5c,0x24,0x50,0xff,0x90,0x60,0x08,0x00,0x00};
    struct Site { std::uint32_t Stamp, ImageSize, Address; std::span<const unsigned char> Code; };
    // Fail closed after an executable update; verify its UWorld::WelcomePlayer call site first.
    inline constexpr std::array Sites{
        Site{0x00a58497, 0x0de46000, 0x0476b5ba, SteamCode}, // Steam
        Site{0x30e25127, 0x0c7fb000, 0x03fba12a, SteamCode}, // Windows dedicated server
        Site{0x315b2d33, 0x0dbc7000, 0x04f32fe1, GamePassCode}, // Game Pass, read from its loaded image
    };
    inline const Site& Find(std::uint32_t stamp, std::uint32_t imageSize) {
        for (const auto& site : Sites) if (site.Stamp == stamp && site.ImageSize == imageSize) return site;
        throw std::runtime_error("This executable's native welcome layout has not been verified");
    }
    inline std::size_t Verify(std::span<const unsigned char> code, std::span<const unsigned char> expected = SteamCode) {
        NetworkIdentitySync::Require(code.size() == expected.size() && std::equal(code.begin(), code.end(), expected.begin()), "Native welcome call-site verification failed");
        return 0x860;
    }
}
