#pragma once
#include "Unreal/UObject.hpp"
#include <nlohmann/json.hpp>
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace DragonWilds::PlayerGhost {
bool Apply(RC::Unreal::UObject* player,const nlohmann::json& effect);
void Flush();
bool IsDead(RC::Unreal::UObject* player);
bool ApplyTimed(RC::Unreal::UObject* player, const nlohmann::json& effect, double seconds,
    bool restart = false);
void TickTimed(double deltaSeconds);
// Clears actors and transient materials for a world transition while preserving
// registered lifecycle callbacks and authored item rules.
void ClearWorld();
void Clear();
bool HasItemRules();
void TrackEquipment(RC::Unreal::UObject* player);
// Session-only diagnostic overlay for currently equipped head/body/legs/cape meshes.
// Returns the number of live armor meshes receiving the effect.
size_t ApplyArmorTest(RC::Unreal::UObject* player,const nlohmann::json& effect,
    uint8_t slots = 0x0f);
void ClearArmorTest();
void SetItemEffect(RC::Unreal::UObject* item,const nlohmann::json& value,
    std::string_view sourceHint = {});
}
