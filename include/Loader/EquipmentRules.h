#pragma once
#include "Loader/SurgeEvadeRules.h"
#include "Loader/ShadowveilRules.h"
#include "Loader/EquipmentEffectRules.h"
namespace DragonWilds::EquipmentRules {
struct Rules {
    SurgeEvadeRules::Rules surge;
    ShadowveilRules::Rules shadowveil;
    EquipmentEffectRules::Rules effects;
};
inline void Merge(Rules& current, const nlohmann::json& document) {
    if (!document.is_object() || document.empty()) throw std::runtime_error("Expected equipment behavior rules");
    auto staged = current;
    if (document.contains("ShadowveilWearables") && document.contains("ShadowveilAttackEvadeWearables"))
        throw std::runtime_error("Use one Shadowveil rule format per document");
    for (const auto& [key, value] : document.items()) {
        if (key == "SurgeEvadeLegs") SurgeEvadeRules::Merge(staged.surge, {{key, value}});
        else if (key == "ShadowveilAttackEvadeWearables" || key == "ShadowveilWearables") ShadowveilRules::Merge(staged.shadowveil, {{key, value}});
        else if (key == "GrantedEffects") EquipmentEffectRules::Merge(staged.effects, value);
        else throw std::runtime_error("Unsupported equipment behavior: " + key);
    }
    current = std::move(staged);
}
}
