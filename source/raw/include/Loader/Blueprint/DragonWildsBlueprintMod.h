#pragma once

#include <string>
#include "Unreal/NameTypes.hpp"
#include "nlohmann/json.hpp"

namespace DragonWilds {
    class DragonWildsBlueprintMod {
    public:
        DragonWildsBlueprintMod(const RC::Unreal::FName& blueprintName, const nlohmann::json& data,
            const RC::StringType& modName={}, const std::string& source={}, const std::string& operationId={});

        virtual ~DragonWildsBlueprintMod() {};
    public:
        const RC::Unreal::FName& GetBlueprintName() const;

        const nlohmann::json& GetData() const;
        const RC::StringType& GetModName() const {return m_modName;}
        const std::string& GetSource() const {return m_source;}
        const std::string& GetOperationId() const {return m_operationId;}
    private:
        RC::Unreal::FName m_name;
        nlohmann::json m_data;
        RC::StringType m_modName;
        std::string m_source, m_operationId;
    };
}
