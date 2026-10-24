#include "Loader/Blueprint/DragonWildsBlueprintMod.h"
#include <Helpers/String.hpp>

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    DragonWildsBlueprintMod::DragonWildsBlueprintMod(const RC::Unreal::FName& blueprintName, const nlohmann::json& data,
        const RC::StringType& modName, const std::string& source, const std::string& operationId)
        : m_name(blueprintName), m_data(data), m_modName(modName), m_source(source), m_operationId(operationId) {}

    const FName& DragonWildsBlueprintMod::GetBlueprintName() const
    {
        return m_name;
    }

    const nlohmann::json& DragonWildsBlueprintMod::GetData() const
    {
        return m_data;
    }
}
