#pragma once
#include <vector>
#include "Unreal/UObject.hpp"
#include "nlohmann/json.hpp"
namespace DragonWilds::GhostMaterials {
    struct Set { RC::Unreal::UObject* Overlay{}; RC::Unreal::UObject* Body{}; };
    bool CanRender(RC::Unreal::UObject* context);
    Set Create(RC::Unreal::UObject* actor, const nlohmann::json& effect,
        std::vector<RC::Unreal::UObject*>& roots);
    bool Apply(RC::Unreal::UObject* actor, const Set& materials, const RC::StringType& context);
}
