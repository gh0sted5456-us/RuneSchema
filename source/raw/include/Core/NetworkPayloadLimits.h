#pragma once
#include <cstddef>

namespace PS::NetworkPayloadLimits {
// FString can serialize as UTF-16. Leave room for RPC metadata and other properties in the bunch.
inline constexpr std::size_t InlineBytes=16*1024;
inline constexpr std::size_t LegacyAppearanceBytes=4*1024;
}
