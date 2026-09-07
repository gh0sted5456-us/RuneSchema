#pragma once
#include "Loader/ShadowveilRules.h"
namespace DragonWilds {
struct EquipmentShadowveilStatus {
    std::size_t wearables{};
    bool enabled{};
    bool server{};
};
EquipmentShadowveilStatus InitializeEquipmentShadowveil(const ShadowveilRules::Rules& rules);
void ShutdownEquipmentShadowveil();
}
