#include "Loader/OwnedContentLedger.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#undef assert
#define assert(expression) do { if(!(expression)) { std::cerr<<"Assertion failed: " #expression " at line "<<__LINE__<<'\n'; return 1; } } while(false)

int RunOwnedContentLedger() {
    using namespace DragonWilds::OwnedContent;
    const auto root=std::filesystem::temp_directory_path()/
        ("runeschema-owned-content-"+std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto file=root/"ledger.json";
    const Record a{"Item","Enabled","AAAAAAAAAAAAAAAAAAAAAA","ITEM_A","/Game/A.A"};
    const Record b{"Item","Disabled","BBBBBBBBBBBBBBBBBBBBBA","ITEM_B","/Game/B.B"};
    Merge(file,{a,b});
    const auto read=Read(file);assert(read.size()==2);
    const auto absent=Absent(read,{"Enabled"});
    assert(absent.size()==1 && absent[0].Owner=="Disabled"
        && absent[0].PersistenceID==b.PersistenceID);
    assert(Absent(read,{"enabled"}).size()==1); // owner matching is case-insensitive
    Merge(file,{a});assert(Read(file).size()==2); // historical ownership survives removal
    bool transfer=false;try {auto changed=b;changed.Owner="Other";Merge(file,{changed});}
    catch(const std::exception&){transfer=true;}assert(transfer);
    const auto snapshot=root/"snapshot.json";
    Merge(snapshot,{a,b});
    BeginSnapshot(snapshot);
    Merge(snapshot,{a});
    const auto missing=CompareSnapshot(snapshot);
    assert(missing.size()==1 && missing[0].PersistenceID==b.PersistenceID);
    CommitSnapshot(snapshot);
    const auto current=Read(snapshot);
    assert(current.size()==1 && current[0].PersistenceID==a.PersistenceID);
    const auto settings=root/"settings";
    Write(LegacyLedgerPath(settings),{{a.PersistenceID,a}});
    const auto canonical=LedgerPath(settings);
    assert(canonical==settings/"safesave"/"OwnedContentLedger.json");
    BeginSnapshot(canonical);
    assert(std::filesystem::exists(canonical) && !std::filesystem::exists(LegacyLedgerPath(settings))
        && CompareSnapshot(canonical).size()==1);
    Merge(canonical,{a});CommitSnapshot(canonical);
    assert(Read(canonical).size()==1);
    const auto assets=root/"mods"/"Disabled"/"assets";
    std::filesystem::create_directories(assets);
    PS::ConfigFiles::Write(assets/"items.jsonc",R"({
      "/Game/Mods/Disabled/Items/ITEM_B.ITEM_B": {
        "$Clone": "/Game/Gameplay/Items/ITEM_Source.ITEM_Source",
        "InternalName": "ITEM_B",
        "PersistenceID": "BBBBBBBBBBBBBBBBBBBBBA"
      }
    })");
    const auto discovered=DiscoverDisabledDefinitions(root/"mods",{});
    assert(discovered.size()==1 && discovered[0].Owner=="Disabled"
        && discovered[0].PersistenceID==b.PersistenceID);
    const auto declarations=Declarations(nlohmann::json::parse(R"({"$declaration":[
      {"Kind":"Item","Path":"/Game/Mods/Pack/Items/ITEM_Pak.ITEM_Pak","PersistenceID":"DDDDDDDDDDDDDDDDDDDDDA"},
      {"Kind":"Recipe","Path":"/Game/Mods/Pack/Recipes/REC_Pak.REC_Pak","PersistenceID":"EEEEEEEEEEEEEEEEEEEEEA","InternalName":"REC_Pak"}
    ]})"),"Pack");
    assert(declarations.size()==2 && declarations[0].InternalName=="ITEM_Pak"
        && declarations[1].Kind=="Recipe" && declarations[1].InternalName=="REC_Pak");
    const auto building=Declarations(nlohmann::json::parse(R"({"$declaration":{"Path":"/Game/Mods/Pack/Buildings/BLD_Pak.BLD_Pak","PersistenceID":"GGGGGGGGGGGGGGGGGGGGGA"}})"),"Pack","Building");
    assert(building.size()==1 && building[0].Kind=="Building" && !building[0].InternalNameAsserted);
    bool badDeclaration=false;try { (void)Declarations(nlohmann::json::parse(R"({"$declaration":{"Kind":"Item","Path":"/Game/X.X","PersistenceID":"short"}})"),"Pack"); }
    catch(const std::exception&){badDeclaration=true;}assert(badDeclaration);
    const auto disabledDeclarations=root/"mods"/"DeclaredDisabled"/"assets";
    std::filesystem::create_directories(disabledDeclarations);
    PS::ConfigFiles::Write(disabledDeclarations/"declaration.json",R"({"$declaration":{"Kind":"Recipe","Path":"/Game/Mods/DeclaredDisabled/REC.REC","PersistenceID":"FFFFFFFFFFFFFFFFFFFFFA"}})");
    const auto discoveredAll=DiscoverDisabledDefinitions(root/"mods",{});
    assert(discoveredAll.size()==2 && std::any_of(discoveredAll.begin(),discoveredAll.end(),[](const auto& row){return row.Kind=="Recipe" && row.Owner=="DeclaredDisabled";}));
    const auto manifest=root/"asset-clones-current.json";
    PS::ConfigFiles::Write(manifest,R"({"Kind":"RuneSchemaCloneManifest","Version":1,"Records":[{
      "Kind":"RuneSchemaAssetClone","Registered":true,"CreatingMod":"Removed",
      "SourceAsset":"/Game/Gameplay/Items/ITEM_Source.ITEM_Source",
      "AssetPath":"/Game/Mods/Removed/Items/ITEM_C.ITEM_C",
      "InternalName":"ITEM_C","PersistenceID":"CCCCCCCCCCCCCCCCCCCCCA"}]})");
    const auto recovered=ReadCloneManifest(manifest);
    assert(recovered.size()==1 && recovered[0].Owner=="Removed");
    std::filesystem::remove_all(root);
    return 0;
}
int main(){try{return RunOwnedContentLedger();}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
