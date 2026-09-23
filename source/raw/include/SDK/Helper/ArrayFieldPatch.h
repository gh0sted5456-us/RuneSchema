#pragma once
#include "nlohmann/json.hpp"
namespace RC::Unreal { class FArrayProperty; }
namespace DragonWilds::PropertyHelper {
    void PatchArrayFields(void* data, RC::Unreal::FArrayProperty* property,
        const nlohmann::json& value);
}
