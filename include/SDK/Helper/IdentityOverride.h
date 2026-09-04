#pragma once

#include "Helpers/String.hpp"
#include "nlohmann/json_fwd.hpp"

namespace RC::Unreal {
    class UObject;
}

namespace DragonWilds {
    struct IdentityOverrideResult {
        int Written = 0;
        int Rejected = 0;
    };

    class IdentityOverride {
    public:
        static bool IsIdentityProperty(const std::string& propertyName);

        static IdentityOverrideResult Apply(
            RC::Unreal::UObject* object,
            const nlohmann::json& body,
            const RC::StringType& context,
            bool allowPropertiesContainer = false);
    };
}

