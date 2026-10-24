#pragma once

#include <cstddef>

namespace RC::Unreal {
class FString;
class UObject;
class UFunction;
}

namespace PS {
class PersistencePruner {
public:
    void PrepareForStartup() noexcept;
    std::size_t PruneCharacterJson(RC::Unreal::FString& characterJson,bool* prepared=nullptr);
    bool PruneBeforeCharacterLoad(RC::Unreal::UObject* context,
        RC::Unreal::UFunction* function, void* parameters);

private:
    bool m_cleanupDeferredReported = false;
};
}
