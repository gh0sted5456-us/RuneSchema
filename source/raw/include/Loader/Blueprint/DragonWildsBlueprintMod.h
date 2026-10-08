#pragma once

#include <string>
#include "Unreal/NameTypes.hpp"
#include "nlohmann/json.hpp"

namespace DragonWilds {
    class DragonWildsBlueprintMod {
    public:
        DragonWildsBlueprintMod(const RC::Unreal::FName& blueprintName, const nlohmann::json& data);

        virtual ~DragonWildsBlueprintMod() {};
    public:
        const RC::Unreal::FName& GetBlueprintName() const;

        const nlohmann::json& GetData() const;
    private:
        RC::Unreal::FName m_name;
        nlohmann::json m_data;
    };
}
