#include <cassert>
#include <stdexcept>
#include "Core/CookedPakRegistryManifest.h"

int main()
{
    using namespace PS::CookedPakRegistryManifest;
    Reset();
    const nlohmann::json document = {
        {"SchemaVersion", 1},
        {"Entries", nlohmann::json::array()},
        {"NativeRegistries", {
            {"Items", nlohmann::json::array({
                "/Example/Items/ITEM_Test.ITEM_Test"})},
            {"MeleeAttackClasses", nlohmann::json::array({
                "/Example/Combat/BP_Attack1.BP_Attack1_C",
                "/Example/Combat/BP_Attack2.BP_Attack2_C"})},
            {"RangedAttackClasses", nlohmann::json::array({
                "/Example/Combat/BP_RangedAttack.BP_RangedAttack_C"})}
        }}
    };
    Manifest published;
    assert(Publish(document, "ExampleMod", "pak:/Example.RSREG_Test", &published));
    assert(published.AssetLanes.size() == 1);
    assert(published.AssetLanes[0].Paths.size() == 1);
    assert(published.MeleeAttackClasses.size() == 2);
    assert(published.RangedAttackClasses.size() == 1);
    assert(published.Fingerprint != 0);
    assert(Snapshot().size() == 1);

    // Republishing the same cooked source replaces its declaration instead of
    // appending a duplicate across an initialization retry.
    assert(Publish(document, "ExampleMod", "pak:/Example.RSREG_Test"));
    assert(Snapshot().size() == 1);

    auto invalid = document;
    invalid["NativeRegistries"]["Items"].push_back("../bad");
    bool rejected = false;
    try { Publish(invalid, "BadMod", "pak:/Bad.RSREG_Bad"); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
    assert(Snapshot().size() == 1);

    auto duplicate = document;
    duplicate["NativeRegistries"]["Items"].push_back(
        "/Example/Items/ITEM_Test.ITEM_Test");
    rejected = false;
    try { Publish(duplicate, "BadMod", "pak:/Bad.RSREG_Duplicate"); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
    assert(Snapshot().size() == 1);
}
