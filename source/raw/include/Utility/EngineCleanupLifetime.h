#pragma once
#include <atomic>

namespace PS {
// No Unreal access during static destruction.
class EngineCleanupLifetime {
    std::atomic<bool> allowed{true};
public:
    void Stop() noexcept { allowed.store(false, std::memory_order_release); }
    template<class Cleanup> void Run(Cleanup&& cleanup) const {
        if (allowed.load(std::memory_order_acquire)) cleanup();
    }
};
struct StopStaticEngineCleanup {
    EngineCleanupLifetime& lifetime;
    ~StopStaticEngineCleanup() { lifetime.Stop(); }
};
}
