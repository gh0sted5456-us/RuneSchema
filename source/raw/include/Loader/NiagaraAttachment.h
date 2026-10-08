#pragma once

#include <nlohmann/json.hpp>

namespace RC::Unreal { class UObject; }

namespace DragonWilds::NiagaraAttachment {
bool CanRenderLocally();
RC::Unreal::UObject* Attach(RC::Unreal::UObject* owner,
    RC::Unreal::UObject* sceneComponent, const nlohmann::json& effect);
void Destroy(RC::Unreal::UObject* component);
}
