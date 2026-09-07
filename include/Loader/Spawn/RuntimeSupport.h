#pragma once
#include "Unreal/UObject.hpp"
#include "nlohmann/json.hpp"
namespace DragonWilds::SpawnRuntime {
    RC::Unreal::UObject* CallWorldContextGetter(const TCHAR* functionPath,
        const TCHAR* objectPath, RC::Unreal::UObject* worldContext);
    RC::Unreal::UObject* GetGameMode(RC::Unreal::UObject* worldContext);
    nlohmann::json ValidateVisualEffect(const nlohmann::json& value);
}
