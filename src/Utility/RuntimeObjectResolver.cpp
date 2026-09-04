#include "Utility/RuntimeObjectResolver.h"

namespace PS {
    namespace {
        RuntimeObjectResolver& Resolver()
        {
            static RuntimeObjectResolver resolver;
            return resolver;
        }
    }

    void SetRuntimeObjectResolverFallback(RuntimeObjectResolver resolver)
    {
        Resolver() = std::move(resolver);
    }

    RC::Unreal::UObject* TryResolveRuntimeObjectFallback(
        const RC::StringType& authoredPath) noexcept
    {
        try
        {
            auto& resolver = Resolver();
            return resolver ? resolver(authoredPath) : nullptr;
        }
        catch (...) { return nullptr; }
    }
}
