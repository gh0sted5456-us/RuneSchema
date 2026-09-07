#include "Loader/EquipmentRules.h"
#include <cassert>
#include <iostream>
using namespace DragonWilds;
int main() {
    EquipmentRules::Rules rules;
    const char* path = "/Game/RuneSchema/Test/Items/cape.cape";
    EquipmentRules::Merge(rules, {{"SurgeEvadeLegs", {{path, true}}}});
    assert(rules.surge.size() == 1 && rules.shadowveil.empty());
    EquipmentRules::Merge(rules, {{"ShadowveilAttackEvadeWearables", {{path, true}}}});
    assert(rules.surge.size() == 1 && rules.shadowveil.size() == 1);
    auto rejects = [&](const nlohmann::json& doc) {
        bool rejected = false;
        auto original = rules;
        try { EquipmentRules::Merge(rules, doc); } catch (const std::exception&) { rejected = true; }
        assert(rejected && rules.surge == original.surge && rules.shadowveil == original.shadowveil);
    };
    rejects({{"ShadowveilAttackEvadeWearables", {{path, false}}}, {"UnknownSpell", true}});
    rejects({{"ShadowveilAttackEvadeWearables", {{"/Game/Bad.*", true}}}});
    rejects({{"ShadowveilAttackEvadeWearables", {{path, "true"}}}});
    rejects(nlohmann::json::array());
    rejects(nlohmann::json::object());
    nlohmann::json oversized = nlohmann::json::object();
    for (int i=0;i<65;++i) oversized["/Game/Item" + std::to_string(i) + ".Item"] = true;
    rejects({{"ShadowveilAttackEvadeWearables", oversized}});
    const auto actionRule = [&](nlohmann::json actions) {
        return nlohmann::json{{"ShadowveilWearables", {{path, {{"PreserveOn", actions}}}}}};
    };
    assert(rules.shadowveil.at(path) == ShadowveilRules::LegacyActions);
    EquipmentRules::Merge(rules, actionRule({"MagicAttack", "UtilityCast"}));
    assert(rules.shadowveil.at(path) == 24 && rules.surge.size() == 1);
    rejects(actionRule({"MeleeAttack", "Unknown"}));
    rejects(actionRule({"MagicAttack", "MagicAttack"}));
    rejects(actionRule({1}));
    rejects(actionRule("MagicAttack"));
    rejects({{"ShadowveilWearables", {{path, true}}}});
    auto misplacedGrant = actionRule({"Evade"});
    misplacedGrant["ShadowveilWearables"][path]["GrantedEffects"] = {"bad"};
    rejects(misplacedGrant);
    rejects({{"ShadowveilWearables", {{path, false}}}, {"ShadowveilAttackEvadeWearables", {{path, true}}}});
    EquipmentRules::Merge(rules, actionRule({"MeleeAttack", "RangedAttack", "Evade", "MagicAttack", "UtilityCast"}));
    assert(rules.shadowveil.at(path) == 31);
    EquipmentRules::Merge(rules, actionRule({"Evade"}));
    assert(rules.shadowveil.at(path) == 4); // Later list replaces, never accumulates stale actions.
    EquipmentRules::Merge(rules, actionRule(nlohmann::json::array()));
    assert(rules.shadowveil.empty());
    EquipmentRules::Merge(rules, actionRule({"MagicAttack"}));
    EquipmentRules::Merge(rules, {{"ShadowveilWearables", {{path, false}}}});
    assert(rules.shadowveil.empty());
    EquipmentRules::Merge(rules, {{"ShadowveilAttackEvadeWearables", {{path, true}}}});
    EquipmentRules::Merge(rules, {{"ShadowveilAttackEvadeWearables", {{path, false}}}});
    assert(rules.shadowveil.empty() && rules.surge.size() == 1);
    std::cout << "PASS: legacy compatibility, independent behaviors, five action masks, replacement/disable overrides, invalid/duplicate actions and atomic rollback.\n";
}
