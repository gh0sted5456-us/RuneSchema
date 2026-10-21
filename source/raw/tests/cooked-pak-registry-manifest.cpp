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
    auto artwork=document;artwork["NativeRegistries"]["CacheAssets"]={"/Example/Icons/T_Original.T_Original"};
    const auto loose=Parse(artwork,"ExampleMod","registry/example.jsonc");
    const auto cooked=Parse(artwork,"ExampleMod","pak:/Example.RSREG_Test");
    assert(loose&&cooked&&loose->Fingerprint==cooked->Fingerprint);
    assert(loose->CacheAssets==cooked->CacheAssets&&loose->CacheAssets.size()==1);
    assert(loose->AssetLanes.size()==published.AssetLanes.size());
    assert(loose->Fingerprint!=published.Fingerprint);
    auto badArt=artwork;badArt["NativeRegistries"]["CacheAssets"]={"../bad"};
    bool invalidArt=false;try{Parse(badArt,"ExampleMod","registry/bad.jsonc");}catch(const std::runtime_error&){invalidArt=true;}assert(invalidArt);
    for(const auto* bad:{"/Example/Icons/NoObject","/Example/Icons/Empty.","/Example/Icons/T.T:Subobject"}){
        badArt=artwork;badArt["NativeRegistries"]["CacheAssets"]={bad};invalidArt=false;
        try{Parse(badArt,"ExampleMod","registry/bad.jsonc");}catch(const std::runtime_error&){invalidArt=true;}assert(invalidArt);
    }
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
    conflictMetadata=*Parse(conflict,"Example","pak:/Example.RSREG_Duplicate");
    assert(firstMetadata.Fingerprint!=conflictMetadata.Fingerprint);
    rejected=false;
    try{Publish(conflict,"Example","pak:/Example.RSREG_Duplicate");}catch(const std::runtime_error&){rejected=true;}
    assert(rejected&&Definitions("Journals")[0].Body==journalBody);

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
    rejected=false;
    try{Publish({{"NativeRegistries",{{"Lore",{journal}}}}},"Other","pak:/Other.RSREG_Test");}catch(const std::runtime_error&){rejected=true;}
    assert(rejected&&Snapshot().size()==1);
    // Loose /registry files share lane validation but must not synthesize a
    // cooked definition for an asset constructed by a schema loader.
    Reset();const std::string runtime="/Game/RuneSchema/Example/Recipes/R_One.R_One";
    const nlohmann::json schemaRef={{"Path",runtime},{"Source","Schema"}};
    assert(Publish({{"NativeRegistries",{{"Items",{"/Game/RuneSchema/Example/Items/A.A"}},{"Recipes",{schemaRef}}}}},"Example","schema.jsonc"));
    assert(Definitions("Recipes").empty());
    assert(Snapshot()[0].AssetLanes.size()==2);
    assert(Publish({{"NativeRegistries",{{"Recipes",{recipe}}}}},"Example","cooked-reference.jsonc"));
    assert(Definitions("Recipes").size()==1&&Definitions("Recipes")[0].Path==recipe);
    bool runtimeBodyRejected=false;
    try{Publish({{"NativeRegistries",{{"Recipes",{schemaRef}}}},{"LoaderDefinitions",{{"Recipes",{{runtime,nlohmann::json::object()}}}}}},"Example","runtime-body.jsonc");}catch(const std::exception&){runtimeBodyRejected=true;}
    assert(runtimeBodyRejected);
    assert(Publish({{"NativeRegistries",{{"Recipes",{runtime}}}}},"Example","cooked-runeschema-namespace.jsonc"));
    assert(Definitions("Recipes").size()==2); // Namespace alone is not provenance.
}
