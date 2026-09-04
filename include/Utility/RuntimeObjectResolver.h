#pragma once

#include <functional>
#include "Unreal/NameTypes.hpp"

namespace RC::Unreal { class UObject; }

namespace PS {
    using RuntimeObjectResolver =
        std::function<RC::Unreal::UObject*(const RC::StringType& authoredPath)>;

    void SetRuntimeObjectResolverFallback(RuntimeObjectResolver resolver);
    RC::Unreal::UObject* TryResolveRuntimeObjectFallback(
        const RC::StringType& authoredPath) noexcept;
}
