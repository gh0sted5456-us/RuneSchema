#include "Loader/Blueprint/DragonWildsBlueprintMod.h"
#include <Helpers/String.hpp>

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    DragonWildsBlueprintMod::DragonWildsBlueprintMod(const RC::Unreal::FName& blueprintName, const nlohmann::json& data) : m_name(blueprintName), m_data(data) {}

    const FName& DragonWildsBlueprintMod::GetBlueprintName() const
    {
        return m_name;
    }

    const nlohmann::json& DragonWildsBlueprintMod::GetData() const
    {
        return m_data;
    }
}
