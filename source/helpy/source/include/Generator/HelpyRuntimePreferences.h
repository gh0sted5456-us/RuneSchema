#pragma once
#include <atomic>
#include <cstdint>

namespace PS::HelpyRuntimePreferences {
inline std::atomic<int> IconCacheLimitMB{200};
inline std::atomic<std::uint64_t> IconCacheEpoch{0};
}
