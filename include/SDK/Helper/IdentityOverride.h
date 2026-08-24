#pragma once

#include "Helpers/String.hpp"
#include "nlohmann/json_fwd.hpp"

namespace RC::Unreal {
    class UObject;
}

namespace DragonWilds {
    enum class IdentityOverrideTarget {
        Asset,
        Recipe,
        Journal,
    };

    struct IdentityOverrideResult {
        int Written = 0;
        int Rejected = 0;
        int Previewed = 0;
    };

    class IdentityOverride {
    public:
        static bool IsIdentityProperty(const std::string& propertyName);

        static IdentityOverrideResult Apply(
            RC::Unreal::UObject* object,
            const nlohmann::json& body,
            const RC::StringType& context,
            IdentityOverrideTarget target,
            bool allowPropertiesContainer = false);
    };
}
