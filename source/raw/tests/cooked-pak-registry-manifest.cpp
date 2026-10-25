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
    Reset();
    for(const auto& family:PS::NativeDataFamilies::Families){
        const nlohmann::json declared={{"NativeRegistries",{{family.Lane,{"/SharedPlugin/Content/A.A"}}}}};
        const auto parsed=Parse(declared,"DifferentFolderOwner","family.json");
        assert(parsed&&parsed->AssetLanes.size()==1&&parsed->AssetLanes[0].Name==family.Lane);
        for(const auto& bad:nlohmann::json::array({"/SharedPlugin/NoObject","/SharedPlugin/A.","/SharedPlugin/A.A:Child","/Script/Dominion.ItemData",{{"Path","/SharedPlugin/A.A"},{"Source","Schema"}}})){
            auto invalid=declared;invalid["NativeRegistries"][family.Lane]=nlohmann::json::array({bad});bool failed=false;
            try{Parse(invalid,"DifferentFolderOwner","bad.json");}catch(const std::exception&){failed=true;}assert(failed);
        }
        auto duplicate=declared;duplicate["NativeRegistries"][family.Lane].push_back("/sharedplugin/content/a.a");bool failed=false;
        try{Parse(duplicate,"DifferentFolderOwner","duplicate.json");}catch(const std::exception&){failed=true;}assert(failed);
    }
    assert(!PS::NativeDataFamilies::Find("Skills")&&!PS::NativeDataFamilies::Find("Difficulty")&&!PS::NativeDataFamilies::Find("Entitlements"));
    using Identity=PS::NativeDataFamilies::Identity;
    const std::vector<Identity> rows{{"b","second","AssetB","/Plugin/B.B",1},{"a","first","AssetA","/Plugin/A.A",0}};
    const auto ordered=PS::NativeDataFamilies::Order(rows);assert(ordered.size()==2&&ordered[0].SourceIndex==0&&ordered[1].SourceIndex==1);
    for(int field=0;field<3;++field){auto collision=rows;if(field==0)collision[1].Persistence="b";if(field==1)collision[1].Internal="second";if(field==2)collision[1].Asset="AssetB";bool failed=false;
        try{PS::NativeDataFamilies::Order(collision);}catch(const std::exception&){failed=true;}assert(failed);
    }
    auto crossAlias=rows;crossAlias[1].Internal="b";bool failed=false;try{PS::NativeDataFamilies::Order(crossAlias);}catch(const std::exception&){failed=true;}assert(failed);
    for(const auto& bad:std::vector<std::string>{"",std::string("a\0b",3),"a\nb",std::string(1025,'x')}){
        auto malformed=rows;malformed[0].Internal=bad;failed=false;try{PS::NativeDataFamilies::Order(malformed);}catch(const std::exception&){failed=true;}assert(failed);
    }
    failed=false;try{PS::NativeDataFamilies::Order(std::vector<Identity>(4097,rows[0]));}catch(const std::exception&){failed=true;}assert(failed);

    Reset();
    const std::string building="/Example/Buildings/BUILDING_Workbench.BUILDING_Workbench";
    const nlohmann::json bareBuilding={{"NativeRegistries",{{"Buildings",{building}}}}};
    assert(Publish(bareBuilding,"Example","bare.jsonc"));
    assert(Definitions("Buildings").size()==1&&Definitions("Buildings")[0].Body==nlohmann::json({{"Unlock",false}}));
    const nlohmann::json buildingBody={{"PersistenceID","existing-save-id"},{"InternalName","ExistingWorkbench"},
        {"AddTo",{{"Collection","Modded Buildings"},{"PageIndex",0}}}};
    assert(Publish({{"LoaderDefinitions",{{"Buildings",{{building,buildingBody}}}}}},"Example","metadata.jsonc"));
    assert(Definitions("Buildings")[0].Body.at("Unlock")==false);
    assert(Definitions("Buildings")[0].Body.at("PersistenceID")=="existing-save-id");
    const auto rejectBuilding=[&](const nlohmann::json& value){bool failed=false;
        try{Parse(value,"Example","invalid-building.jsonc");}catch(const std::exception&){failed=true;}assert(failed);};
    auto conflicting=buildingBody;conflicting["AddTo"]["PageIndex"]=1;
    bool conflictRejected=false;
    try{Publish({{"LoaderDefinitions",{{"Buildings",{{building,conflicting}}}}}},"Example","conflict.jsonc");}
    catch(const std::exception&){conflictRejected=true;}assert(conflictRejected);
    for(const auto* field:{"Properties","Overrides","Requirements","Asset","$Clone","Class"}){
        auto body=buildingBody;body[field]=nlohmann::json::object();
        rejectBuilding({{"LoaderDefinitions",{{"Buildings",{{building,body}}}}}});
    }
    for(const auto& bad:nlohmann::json::array({"/Example/NoObject","/Example/A.A:Subobject",{{"Path",building},{"Source","Schema"}}}))
        rejectBuilding({{"NativeRegistries",{{"Buildings",nlohmann::json::array({bad})}}}});
    for(const auto& bad:nlohmann::json::array({-1,2147483648LL,"0",0.5})){
        auto body=buildingBody;body["AddTo"]["PageIndex"]=bad;
        rejectBuilding({{"LoaderDefinitions",{{"Buildings",{{building,body}}}}}});
    }
    auto alias=bareBuilding;alias["NativeRegistries"]["Buildings"].push_back("/example/buildings/building_workbench.building_workbench");rejectBuilding(alias);
    for(const auto& bad:nlohmann::json::array({"",std::string("a\0b",3),"wrong\nidentity",42})){
        auto body=buildingBody;body["PersistenceID"]=bad;
        rejectBuilding({{"LoaderDefinitions",{{"Buildings",{{building,body}}}}}});
    }
    // Owner conflicts and explicit-body conflicts must leave publication unchanged.
    rejectDocument(bareBuilding);
    assert(Snapshot().size()==2&&Definitions("Buildings").size()==1);
    Reset();
    for(const auto* lane:{"Items","Recipes","Journals","Lore","WearableMeshData","CacheAssets"}){
        bool duplicateRejected=false;
        try{Parse({{"NativeRegistries",{{lane,{"/Example/A.A","/example/a.a"}}}}},"Example","duplicate.jsonc");}
        catch(const std::exception&){duplicateRejected=true;}assert(duplicateRejected);
    }
    const std::string mixed="/Example/Mixed.Mixed",lower="/example/mixed.mixed";
    for(const auto* lane:{"Recipes","Journals","Lore","Buildings"}){
        Reset();
        assert(Publish({{"NativeRegistries",{{lane,{mixed}}}}},"Example","bare.jsonc"));
        assert(Publish({{"LoaderDefinitions",{{lane,{{lower,{{"Unlock",true}}}}}}}},"Example","explicit.jsonc"));
        assert(Definitions(lane).size()==1&&Definitions(lane)[0].Body.at("Unlock")==true&&Definitions(lane)[0].Path==lower);
        const auto before=Snapshot().size();bool conflict=false;
        try{Publish({{"LoaderDefinitions",{{lane,{{mixed,{{"Unlock",false}}}}}}}},"Example","conflict.jsonc");}
        catch(const std::exception&){conflict=true;}assert(conflict&&Snapshot().size()==before&&Definitions(lane)[0].Body.at("Unlock")==true);
        bool duplicate=false;
        try{Parse({{"LoaderDefinitions",{{lane,{{mixed,nlohmann::json::object()},{lower,nlohmann::json::object()}}}}}},"Example","aliases.jsonc");}
        catch(const std::exception&){duplicate=true;}assert(duplicate);
    }
    Reset();
    assert(Publish({{"NativeRegistries",{{"Journals",{mixed}}}}},"Example","journal.jsonc"));
    bool crossLane=false;
    try{Publish({{"LoaderDefinitions",{{"Lore",{{lower,nlohmann::json::object()}}}}}},"Example","lore.jsonc");}
    catch(const std::exception&){crossLane=true;}assert(crossLane&&Snapshot().size()==1);
    bool schemaAlias=false;
    try{Parse({{"NativeRegistries",{{"Recipes",nlohmann::json::array({{{"Path",mixed},{"Source","Schema"}}})}}},
        {"LoaderDefinitions",{{"Recipes",{{lower,nlohmann::json::object()}}}}}},"Example","schema-alias.jsonc");}
    catch(const std::exception&){schemaAlias=true;}assert(schemaAlias);
    Reset();
}
