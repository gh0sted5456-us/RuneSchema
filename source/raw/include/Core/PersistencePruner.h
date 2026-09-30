#pragma once

#include <atomic>

namespace RC::Unreal {
class UObject;
class UFunction;
}

namespace PS {
class PersistencePruner {
public:
    void PrepareForStartup() noexcept;
    void PruneBeforeCharacterLoad(RC::Unreal::UObject* context,
        RC::Unreal::UFunction* function, void* parameters);

private:
    static inline std::atomic_bool s_cleanupConsumedForProcess{false};
    bool m_cleanupDeferredReported = false;
};
}
