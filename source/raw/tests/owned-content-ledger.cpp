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
    const Record journal{"Journal","Enabled","RS_Journal_Dawnveil_Armor",
        "RS_Journal_Dawnveil_Armor","RS_Journal_Dawnveil_Armor"};
    Merge(file,{a,b});
    const auto read=Read(file);assert(read.size()==2);
    Merge(file,{journal});
    const auto withJournal=Read(file);
    assert(std::any_of(withJournal.begin(),withJournal.end(),[](const auto& row){
        return row.Kind=="Journal" && row.PersistenceID=="RS_Journal_Dawnveil_Armor";}));
    bool badJournal=false;
    try {auto invalid=journal;invalid.PersistenceID="RS_Journal\nBad";Validate(invalid);}
    catch(const std::exception&){badJournal=true;}
    assert(badJournal);
    Merge(file,{a});assert(Read(file).size()==3); // historical ownership survives removal
    auto changed=b;changed.Owner="Other";changed.Source="/Game/Moved/B.B";
    Merge(file,{changed});
    const auto transferred=Read(file);
    const auto transferredRow=std::find_if(transferred.begin(),transferred.end(),[&](const auto& row){return row.PersistenceID==b.PersistenceID;});
    assert(transferredRow!=transferred.end() && transferredRow->Owner=="Other"
        && transferredRow->Source=="/Game/Moved/B.B");
    const auto snapshot=root/"snapshot.json";
    Merge(snapshot,{a,b});
    BeginSnapshot(snapshot);
    auto movedA=a;movedA.Owner="Coinage";movedA.Source="/Game/Mods/Coinage/ITEM_A.ITEM_A";
    Merge(snapshot,{movedA});
    const auto missing=CompareSnapshot(snapshot);
    assert(missing.size()==1 && missing[0].PersistenceID==b.PersistenceID);
    CommitSnapshot(snapshot);
    const auto current=Read(snapshot);
    assert(current.size()==1 && current[0].PersistenceID==a.PersistenceID
        && current[0].Owner=="Coinage" && current[0].Source==movedA.Source);
    const auto settings=root/"settings";
    Write(LegacyLedgerPath(settings),{{a.PersistenceID,a}});
    const auto canonical=LedgerPath(settings);
    assert(canonical==settings/"safesave"/"OwnedContentLedger.json");
    BeginSnapshot(canonical);
    assert(std::filesystem::exists(canonical) && !std::filesystem::exists(LegacyLedgerPath(settings))
        && CompareSnapshot(canonical).size()==1);
    Merge(canonical,{a});CommitSnapshot(canonical);
    assert(Read(canonical).size()==1);
    const auto incomplete=root/"incomplete.json";
    Merge(incomplete,{a,b});BeginSnapshot(incomplete);Merge(incomplete,{a});
    MarkSnapshotIncomplete();
    bool refusedIncomplete=false;
    try {(void)CompareSnapshot(incomplete);}
    catch(const std::exception&){refusedIncomplete=true;}
    assert(refusedIncomplete && Read(incomplete).size()==2);
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
    std::filesystem::remove_all(root);
    return 0;
}
int main(){try{return RunOwnedContentLedger();}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
