#ifdef NDEBUG
#undef NDEBUG
#endif
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
            {"WearableMeshData", nlohmann::json::array({
                "/Example/MeshData/WEMD_Test.WEMD_Test"})},
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
    assert(published.WearableMeshData.size() == 1);
    assert(published.MeleeAttackClasses.size() == 2);
    assert(published.RangedAttackClasses.size() == 1);
    assert(published.Fingerprint != 0);
    assert(Snapshot().size() == 1);

    auto differentMesh = document;
    differentMesh["NativeRegistries"]["WearableMeshData"][0] =
        "/Example/MeshData/WEMD_Other.WEMD_Other";
    Manifest changed;
    assert(Publish(differentMesh, "ExampleMod", "pak:/Example.RSREG_Test", &changed));
    assert(changed.Fingerprint != published.Fingerprint);
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

    Reset();
    const std::string recipe="/Example/Recipes/RECIPE_Dye.RECIPE_Dye";
    const std::string journal="/Example/Journal/JOURNAL_Trader.JOURNAL_Trader";
    const std::string lore="/Example/Journal/JOURNAL_Note.JOURNAL_Note";
    const nlohmann::json enrolled={{"NativeRegistries",{
        {"Recipes",{recipe}},{"Journals",{journal}},{"Lore",{lore}}}}};
    assert(Publish(enrolled,"Example","pak:/Example.RSREG_Base"));
    for(const auto* lane:{"Recipes","Journals","Lore"}){
        const auto values=Definitions(lane);
        assert(values.size()==1 && values[0].Body==nlohmann::json({{"Unlock",false}}));
    }
    const nlohmann::json recipeBody={{"Unlock",false},{"AddTo",nlohmann::json::array({
        {{"DataTable","/Game/Example/DT_Stations.DT_Stations"},{"Row","DyeStation"},{"Category","Dyes"}}})}};
    const nlohmann::json journalBody={{"Unlock",true},{"AddTo",{{"Section","Knowledge"},{"Category","People"}}}};
    const nlohmann::json loreBody={{"Unlock",false},{"UnlockOnAcquire",true},{"ItemData","/Example/Items/ITEM_Note.ITEM_Note"}};
    const nlohmann::json metadata={{"LoaderDefinitions",{
        {"Recipes",{{recipe,recipeBody}}},{"Journals",{{journal,journalBody}}},{"Lore",{{lore,loreBody}}}}}};
    Manifest firstMetadata;
    assert(Publish(metadata,"Example","pak:/Example.RSREG_Metadata",&firstMetadata));
    assert(Definitions("Recipes")[0].Body==recipeBody);
    assert(Definitions("Journals")[0].Body==journalBody);
    assert(Definitions("Lore")[0].Body==loreBody);
    // Identical repeated declarations are idempotent; an explicit body beats
    // a bare native path, regardless of discovery order.
    assert(Publish(metadata,"Example","pak:/Example.RSREG_Duplicate"));
    assert(Publish(enrolled,"Example","pak:/Example.RSREG_LaterBare"));
    assert(Definitions("Journals").size()==1);
    assert(Definitions("Journals")[0].Body==journalBody);
    auto conflict=metadata;
    conflict["LoaderDefinitions"]["Journals"][journal]["Unlock"]=false;
    Manifest conflictMetadata;
    assert(Publish(conflict,"Example","pak:/Example.RSREG_Duplicate",&conflictMetadata));
    assert(firstMetadata.Fingerprint!=conflictMetadata.Fingerprint);
    rejected=false;
    try{Definitions("Journals");}catch(const std::runtime_error&){rejected=true;}
    assert(rejected);

    const auto rejectDocument=[&](const nlohmann::json& value){
        const auto before=Snapshot().size();bool failed=false;
        try{Publish(value,"Bad","pak:/Bad.RSREG_Test");}catch(const std::runtime_error&){failed=true;}
        assert(failed && Snapshot().size()==before);
    };
    for(const auto* directive:{"$Clone","$Patch","$Target","VendorID","RuneSchemaVendors","VanillaVendors"}){
        auto value=metadata;value["LoaderDefinitions"]["Recipes"][recipe][directive]="not allowed";rejectDocument(value);
    }
    auto value=metadata;value["LoaderDefinitions"]["Lore"][lore]["Unlock"]="yes";rejectDocument(value);
    rejectDocument({{"LoaderDefinitions",{{"Unknown",nlohmann::json::object()}}}});
    rejectDocument({{"LoaderDefinitions",{{"Journals",{{"/Example/Journal/NoObjectName",nlohmann::json::object()}}}}}});
    rejectDocument({{"LoaderDefinitions",{{"Journals",{{"/Example/Journal/Empty.",nlohmann::json::object()}}}}}});
    rejectDocument({{"LoaderDefinitions",{{"Journals",{{journal,1}}}}}});
    rejectDocument({{"NativeRegistries",nlohmann::json::object()}});
    auto oversized=metadata;
    for(int i=0;i<4097;++i)oversized["LoaderDefinitions"]["Lore"]["/Example/Note"+std::to_string(i)+".Note"] = nlohmann::json::object();
    rejectDocument(oversized);
    Reset();
    assert(Publish(enrolled,"Example","pak:/Example.RSREG_Base"));
    assert(Publish({{"NativeRegistries",{{"Lore",{journal}}}}},"Other","pak:/Other.RSREG_Test"));
    rejected=false;
    try{Definitions("Journals");}catch(const std::runtime_error&){rejected=true;}
    assert(rejected);
}
