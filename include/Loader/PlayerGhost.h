#pragma once
#include "Unreal/UObject.hpp"
#include <nlohmann/json.hpp>
#include <string_view>
namespace DragonWilds::PlayerGhost {
bool Apply(RC::Unreal::UObject* player,const nlohmann::json& effect);
void Flush();
void Clear();
bool HasItemRules();
void TrackEquipment(RC::Unreal::UObject* player);
void SetItemEffect(RC::Unreal::UObject* item,const nlohmann::json& value,
    std::string_view sourceHint = {});
}
