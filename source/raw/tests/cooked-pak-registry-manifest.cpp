#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <stdexcept>
#include "Core/CookedPakRegistryManifest.h"

int main()
{
    using namespace PS::CookedPakRegistryManifest;
    // A bad declaration is refused (nothing else usable) or left out with a reason; never silently kept.
    const auto Refused=[](auto&& parse){try{const auto result=parse();return !result||!result->Dropped.empty();}catch(const std::exception&){return true;}};
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
    const std::string component="/Game/Mods/ExampleMod/BP_World.BP_World_C";
    const nlohmann::json worldOnly={{"NativeRegistries",{{"WorldComponents",{component}}}}};
    const auto world=Parse(worldOnly,"ExampleMod","registry/world.jsonc");
    assert(world&&world->WorldComponents==std::vector<std::string>{component}&&world->AssetLanes.empty());
    auto withWorld=document;withWorld["NativeRegistries"]["WorldComponents"]={component};
    assert(Parse(withWorld,"ExampleMod","registry/world.jsonc")->Fingerprint!=published.Fingerprint);
    auto emptyWorld=document;emptyWorld["NativeRegistries"]["WorldComponents"]=nlohmann::json::array();
    assert(Parse(emptyWorld,"ExampleMod","registry/world.jsonc")->Fingerprint==published.Fingerprint);
    for(const auto* path:{"/Game/Mods/Other/BP.BP_C","/Game/Mods/ExampleMod/BP.BP","/Script/Engine.ActorComponent","/Game/Mods/ExampleMod/BP.BP_C:Sub"}){
        auto invalidWorld=worldOnly;invalidWorld["NativeRegistries"]["WorldComponents"]={path};bool bad=false;
        bad=Refused([&]{return Parse(invalidWorld,"ExampleMod","registry/world.jsonc");});assert(bad);
    }
    auto duplicateWorld=worldOnly;duplicateWorld["NativeRegistries"]["WorldComponents"].push_back(component);bool badWorld=false;
    badWorld=Refused([&]{return Parse(duplicateWorld,"ExampleMod","registry/world.jsonc");});assert(badWorld);

    const nlohmann::json commands={{"NativeRegistries",{{"ConsoleCommands",nlohmann::json::array({{{"Version",1},{"Id","Weather"},{"Class",component}}})}}}};
    const auto command=Parse(commands,"ExampleMod","registry/commands.jsonc");
    assert(command&&command->ConsoleCommands.size()==1&&command->ConsoleCommands[0].Owner=="ExampleMod");
    auto withCommand=document;withCommand["NativeRegistries"]["ConsoleCommands"]=commands["NativeRegistries"]["ConsoleCommands"];
    assert(Parse(withCommand,"ExampleMod","registry/commands.jsonc")->Fingerprint!=published.Fingerprint);
    auto noCommands=document;noCommands["NativeRegistries"]["ConsoleCommands"]=nlohmann::json::array();
    assert(Parse(noCommands,"ExampleMod","registry/commands.jsonc")->Fingerprint==published.Fingerprint);
    for(const auto& invalid:nlohmann::json::array({
        {{"Version",2},{"Id","Weather"},{"Class",component}},{{"Version",1},{"Id","bad:id"},{"Class",component}},
        {{"Version",1},{"Id","Weather"},{"Class","/Game/Mods/Other/BP.BP_C"}},
        {{"Version",1},{"Id","Weather"},{"Class",component},{"Owner","Other"}},
        {{"Version",1},{"Id","Weather"},{"Class","/Game/Mods/ExampleMod/BP.BP"}}})){
        auto bad=commands;bad["NativeRegistries"]["ConsoleCommands"][0]=invalid;bool rejected=false;
        rejected=Refused([&]{return Parse(bad,"ExampleMod","registry/commands.jsonc");});assert(rejected);
    }
    auto duplicateCommand=commands;duplicateCommand["NativeRegistries"]["ConsoleCommands"].push_back({{"Version",1},{"Id","weather"},{"Class",component}});
    bool rejectedCommand=false;rejectedCommand=Refused([&]{return Parse(duplicateCommand,"ExampleMod","registry/commands.jsonc");});assert(rejectedCommand);

    const nlohmann::json local={{"NativeRegistries",{{"LocalServices",nlohmann::json::array({{{"Version",1},{"Id","templates"},{"Class","/Game/ExampleMod/Core/BPC_Template.BPC_Template_C"},{"Templates",true}}})}}}};
    const auto localService=Parse(local,"ExampleMod","registry/local.jsonc");assert(localService&&localService->LocalServices.size()==1&&localService->LocalServices[0].Templates);
    assert(!Detail::OwnedObjectPath("/Game/ExampleMod/Core/BPC_Template.BPC_Template_C","ExampleMod"));
    auto computeService=local;computeService["NativeRegistries"]["LocalServices"][0].erase("Templates");computeService["NativeRegistries"]["LocalServices"][0]["ComputeProviders"]={"ExampleMod.Math/Check180"};assert(Parse(computeService,"ExampleMod","registry/local.jsonc"));
    for(const auto& row:nlohmann::json::array({
        {{"Version",1},{"Id","t"},{"Class","/Game/Other/BPC.BPC_C"},{"Templates",true}},
        {{"Version",1},{"Id","t"},{"Class",component},{"Templates","true"}},
        {{"Version",1},{"Id","t"},{"Class",component}},
        {{"Version",1},{"Id","t"},{"Class",component},{"ComputeProviders",{"Other.Math/Probe"}}},
        {{"Version",1},{"Id","t"},{"Class",component},{"Templates",true},{"Root","C:/other"}}})){
        auto invalid=local;invalid["NativeRegistries"]["LocalServices"][0]=row;bool rejected=false;rejected=Refused([&]{return Parse(invalid,"ExampleMod","registry/local.jsonc");});assert(rejected);
    }
    auto duplicateLocal=local;duplicateLocal["NativeRegistries"]["LocalServices"].push_back(duplicateLocal["NativeRegistries"]["LocalServices"][0]);bool rejectedLocal=false;rejectedLocal=Refused([&]{return Parse(duplicateLocal,"ExampleMod","registry/local.jsonc");});assert(rejectedLocal);

    auto artwork=document;artwork["NativeRegistries"]["CacheAssets"]={"/Example/Icons/T_Original.T_Original"};
    const auto loose=Parse(artwork,"ExampleMod","registry/example.jsonc");
    const auto cooked=Parse(artwork,"ExampleMod","pak:/Example.RSREG_Test");
    assert(loose&&cooked&&loose->Fingerprint==cooked->Fingerprint);
    assert(loose->CacheAssets==cooked->CacheAssets&&loose->CacheAssets.size()==1);
    assert(loose->AssetLanes.size()==published.AssetLanes.size());
    assert(loose->Fingerprint!=published.Fingerprint);
    auto badArt=artwork;badArt["NativeRegistries"]["CacheAssets"]={"../bad"};
    bool invalidArt=false;invalidArt=Refused([&]{return Parse(badArt,"ExampleMod","registry/bad.jsonc");});assert(invalidArt);
    for(const auto* bad:{"/Example/Icons/NoObject","/Example/Icons/Empty.","/Example/Icons/T.T:Subobject"}){
        badArt=artwork;badArt["NativeRegistries"]["CacheAssets"]={bad};invalidArt=false;
        invalidArt=Refused([&]{return Parse(badArt,"ExampleMod","registry/bad.jsonc");});assert(invalidArt);
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

    // One bad item path drops only that path: the document's good items still register.
    auto invalid = document;
    invalid["NativeRegistries"]["Items"].push_back("../bad");
    Manifest partial;
    assert(Publish(invalid, "BadMod", "pak:/Bad.RSREG_Bad", &partial));
    assert(Snapshot().size() == 2);
    assert(partial.Dropped.size() == 1 && partial.Dropped[0].first == "Items ../bad");
    assert(partial.AssetLanes[0].Name == "Items" && partial.AssetLanes[0].Paths == std::vector<std::string>{"/Example/Items/ITEM_Test.ITEM_Test"});
    assert(partial.WearableMeshData.size() == 1 && partial.MeleeAttackClasses.size() == 2); // Other sections are untouched.
    assert(partial.Fingerprint != Parse(document, "BadMod", "pak:/Bad.RSREG_Bad")->Fingerprint); // A dropped path changes the fingerprint.

    auto duplicate = document;
    duplicate["NativeRegistries"]["Items"].push_back(
        "/Example/Items/ITEM_Test.ITEM_Test");
    assert(Publish(duplicate, "BadMod", "pak:/Bad.RSREG_Duplicate", &partial));
    assert(partial.Dropped.size() == 1 && partial.AssetLanes[0].Paths.size() == 1);

    // A failing side section (here WorldComponents) no longer takes the items with it.
    auto harvesting = document;
    harvesting["NativeRegistries"]["WorldComponents"] = {"/Example/Blueprints/BP_World.BP_World"};
    const auto kept = Parse(harvesting, "Example", "registry/harvesting.jsonc");
    assert(kept && kept->WorldComponents.empty() && kept->Dropped.size() == 1 && kept->Dropped[0].first == "WorldComponents");
    assert(kept->AssetLanes.size() == 1 && kept->AssetLanes[0].Paths.size() == 1);
    // Unknown section names are left out the same way.
    auto unknown = document;unknown["NativeRegistries"]["Teleporters"] = {"/Example/A.A"};
    const auto withoutUnknown = Parse(unknown, "ExampleMod", "registry/unknown.jsonc");
    assert(withoutUnknown && withoutUnknown->Dropped.size() == 1 && withoutUnknown->Dropped[0].first == "Teleporters" && withoutUnknown->AssetLanes.size() == 1);
    // A document whose only content is bad is still refused as a whole, naming the first problem.
    bool onlyBad = false;
    try { Parse(nlohmann::json{{"NativeRegistries", {{"Items", {"../bad"}}}}}, "Example", "registry/bad.jsonc"); }
    catch (const std::runtime_error& error) { onlyBad = std::string(error.what()).find("first problem") != std::string::npos; }
    assert(onlyBad);
    // The document-wide limit still refuses everything.
    auto huge = nlohmann::json::array();for (int i = 0; i < 4096; ++i) huge.push_back("/Example/Items/I" + std::to_string(i) + ".I" + std::to_string(i));
    nlohmann::json tooMany{{"NativeRegistries", nlohmann::json::object()}};
    for (const auto* lane : {"Items", "Recipes", "Quests", "CombatSpells", "UtilitySpells"}) tooMany["NativeRegistries"][lane] = huge;
    bool limited = false;
    try { Parse(tooMany, "Example", "registry/huge.jsonc"); } catch (const std::runtime_error& error) { limited = std::string(error.what()).find("document limit") != std::string::npos; }
    assert(limited);
    // An unchanged, valid document keeps its fingerprint (no change for mods that were already correct).
    assert(Parse(document, "ExampleMod", "pak:/Example.RSREG_Test")->Fingerprint == published.Fingerprint);

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
    bool rejected=false;
    try{Publish(conflict,"Example","pak:/Example.RSREG_Duplicate");}catch(const std::runtime_error&){rejected=true;}
    assert(rejected&&Definitions("Journals")[0].Body==journalBody);

    // Refused outright when nothing is usable; otherwise the bad section is left out and named.
    const auto rejectDocument=[&](const nlohmann::json& value){
        const auto before=Snapshot().size();bool failed=false;Manifest kept;
        try{failed=!Publish(value,"Bad","pak:/Bad.RSREG_Test",&kept)||!kept.Dropped.empty();}
        catch(const std::runtime_error&){failed=true;assert(Snapshot().size()==before);}
        assert(failed);
        for(const auto& [lane,reason]:kept.Dropped)if(lane.starts_with("LoaderDefinitions."))assert(!kept.LoaderDefinitions.contains(lane.substr(18)));
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
    auto nullNative=metadata;nullNative["NativeRegistries"]=nullptr;rejectDocument(nullNative);
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
    // Editing a schema-created recipe through LoaderDefinitions is refused; the schema reference itself stays.
    Manifest runtimeBody;
    assert(Publish({{"NativeRegistries",{{"Recipes",{schemaRef}}}},{"LoaderDefinitions",{{"Recipes",{{runtime,nlohmann::json::object()}}}}}},"Example","runtime-body.jsonc",&runtimeBody));
    assert(runtimeBody.Dropped.size()==1&&runtimeBody.Dropped[0].first=="LoaderDefinitions.Recipes"&&runtimeBody.LoaderDefinitions.empty());
    assert(Definitions("Recipes").size()==1);
    assert(Publish({{"NativeRegistries",{{"Recipes",{runtime}}}}},"Example","cooked-runeschema-namespace.jsonc"));
    assert(Definitions("Recipes").size()==2); // Namespace alone is not provenance.
    Reset();
    for(const auto& family:PS::NativeDataFamilies::Families){
        const nlohmann::json declared={{"NativeRegistries",{{family.Lane,{"/SharedPlugin/Content/A.A"}}}}};
        const auto parsed=Parse(declared,"DifferentFolderOwner","family.json");
        assert(parsed&&parsed->AssetLanes.size()==1&&parsed->AssetLanes[0].Name==family.Lane);
        for(const auto& bad:nlohmann::json::array({"/SharedPlugin/NoObject","/SharedPlugin/A.","/SharedPlugin/A.A:Child","/Script/Dominion.ItemData",{{"Path","/SharedPlugin/A.A"},{"Source","Schema"}}})){
            auto invalid=declared;invalid["NativeRegistries"][family.Lane]=nlohmann::json::array({bad});bool failed=false;
            failed=Refused([&]{return Parse(invalid,"DifferentFolderOwner","bad.json");});assert(failed);
        }
        auto duplicate=declared;duplicate["NativeRegistries"][family.Lane].push_back("/sharedplugin/content/a.a");bool failed=false;
        failed=Refused([&]{return Parse(duplicate,"DifferentFolderOwner","duplicate.json");});assert(failed);
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
        failed=Refused([&]{return Parse(value,"Example","invalid-building.jsonc");});assert(failed);};
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
        duplicateRejected=Refused([&]{return Parse({{"NativeRegistries",{{lane,{"/Example/A.A","/example/a.a"}}}}},"Example","duplicate.jsonc");});assert(duplicateRejected);
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
        duplicate=Refused([&]{return Parse({{"LoaderDefinitions",{{lane,{{mixed,nlohmann::json::object()},{lower,nlohmann::json::object()}}}}}},"Example","aliases.jsonc");});assert(duplicate);
    }
    Reset();
    assert(Publish({{"NativeRegistries",{{"Journals",{mixed}}}}},"Example","journal.jsonc"));
    bool crossLane=false;
    try{Publish({{"LoaderDefinitions",{{"Lore",{{lower,nlohmann::json::object()}}}}}},"Example","lore.jsonc");}
    catch(const std::exception&){crossLane=true;}assert(crossLane&&Snapshot().size()==1);
    bool schemaAlias=false;
    schemaAlias=Refused([&]{return Parse({{"NativeRegistries",{{"Recipes",nlohmann::json::array({{{"Path",mixed},{"Source","Schema"}}})}}},
        {"LoaderDefinitions",{{"Recipes",{{lower,nlohmann::json::object()}}}}}},"Example","schema-alias.jsonc");});assert(schemaAlias);
    Reset();
    const std::string tableSource="/Example/Tables/DT_Extra.DT_Extra",tableTarget="/Game/Tables/DT_Native.DT_Native";
    const auto tableDocument=[&](const nlohmann::json& body){return nlohmann::json{{"LoaderDefinitions",{{"DataTables",{{tableSource,body}}}}}};};
    assert(Publish(tableDocument({{"Target",tableTarget}}),"Example","pak:/Example/DA_Registry.DA_Registry"));
    assert(Definitions("DataTables").size()==1&&Definitions("DataTables")[0].Body==nlohmann::json({{"Target",tableTarget}}));
    for(const auto& bad:nlohmann::json::array({nlohmann::json::object(),{{"Target",7}},{{"Target","DT_Native"}},
        {{"Target","/Game/Tables/DT_Native"}},{{"Target","/Game/Tables/A.A:Subobject"}},{{"Target","/Other/Tables/A.A"}},
        {{"Target",tableTarget},{"Replace",true}},{{"Target",tableTarget},{"Unlock",true}}})){
        bool rejected=false;rejected=Refused([&]{return Parse(tableDocument(bad),"Example","bad.jsonc");});assert(rejected);
    }
    bool foreign=false;foreign=Refused([&]{return Parse(tableDocument({{"Target",tableTarget}}),"OtherOwner","bad.jsonc");});assert(foreign);
    Reset();
    Manifest large;large.Owner="Large";large.Source="large.jsonc";
    constexpr int count=23000;
    for(int i=0;i<count;++i){const auto name="R_"+std::to_string(i);const auto path="/Large/Recipes/"+name+"."+name;
        large.LoaderDefinitions["Recipes"][path]={{"Unlock",false},{"Discovery",{{"AllItems",{"/Large/I.I","/Large/J.J"}}}}};}
    PublishBatch({large});
    const auto* retained=&Detail::Manifests[0]->LoaderDefinitions.get_ref<const nlohmann::json::object_t&>();
    Manifest unrelated;unrelated.Owner="Other";unrelated.Source="items.jsonc";unrelated.AssetLanes.push_back({"Items",{"/Other/I.I"}});
    PublishBatch({unrelated});
    assert(retained==&Detail::Manifests[0]->LoaderDefinitions.get_ref<const nlohmann::json::object_t&>());
    auto owned=Definitions("Recipes");assert(owned.size()==count);
    Manifest lateManifestConflict;lateManifestConflict.Owner="Large";lateManifestConflict.Source="conflict.jsonc";
    lateManifestConflict.LoaderDefinitions["Recipes"][owned.front().Path]={{"Unlock",true}};
    Manifest earlier;earlier.Owner="Early";earlier.Source="early.jsonc";earlier.AssetLanes.push_back({"Items",{"/Early/I.I"}});
    bool atomic=false;try{PublishBatch({earlier,lateManifestConflict});}catch(const std::runtime_error&){atomic=true;}
    assert(atomic&&Detail::Manifests.size()==2&&retained==&Detail::Manifests[0]->LoaderDefinitions.get_ref<const nlohmann::json::object_t&>());
    auto replacement=large;replacement.LoaderDefinitions["Recipes"][owned.front().Path]["Unlock"]=true;
    PublishBatch({replacement,large}); // Last update to the same owner/source wins.
    assert(Detail::Manifests.size()==2&&Definitions("Recipes").front().Body.at("Unlock")==false);
    PublishBatch({replacement});
    assert(Definitions("Recipes").front().Body.at("Unlock")==true&&owned.front().Body.at("Unlock")==false);
    Reset();assert(owned.size()==count&&owned.front().Body.at("Unlock")==false);
    Manifest defaults;defaults.Owner="A";defaults.Source="a";defaults.LoaderDefinitions["Recipes"]["/Shared/R.R"]=nlohmann::json::object();
    auto explicitDefault=defaults;explicitDefault.Source="b";explicitDefault.LoaderDefinitions["Recipes"]["/Shared/R.R"]["Unlock"]=false;
    PublishBatch({defaults,explicitDefault});assert(Definitions("Recipes").size()==1);
    Reset();
    assert(WorldComponentClasses().empty());
    assert(Publish(worldOnly,"ExampleMod","registry/world.jsonc"));
    const auto worldClasses=WorldComponentClasses();
    assert(worldClasses.size()==1&&worldClasses.front().first=="ExampleMod"&&worldClasses.front().second==component);
    Reset();assert(WorldComponentClasses().empty());
}
