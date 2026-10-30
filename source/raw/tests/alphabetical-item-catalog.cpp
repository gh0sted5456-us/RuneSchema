#include "Core/AlphabeticalItemCatalog.h"
#include "Core/PersistentItemCatalog.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
using namespace PS::AlphabeticalItemCatalog;
template<class F> void Reject(F fn){bool rejected=false;try{fn();}catch(...){rejected=true;}assert(rejected);}
int main(int argc,char** argv){try{
    assert(PS::ItemNameOrder::Key("  aPPLe \t","path")==PS::ItemNameOrder::Key("Apple","path"));
    Cache cache;cache.Scope("world/source/clone-1");unsigned scans=0;
    const auto source=[&](size_t offset){
        ++scans;assert(offset==0||offset==4096);
        if(!offset)return Json{{"Rows",Json::array({{{"Path","/Game/Z"},{"Name","Zinc"}},{{"Path","/Mod/B"},{"Name","apple"}}})},{"NextOffset",4096},{"HasMore",true}};
        return Json{{"Rows",Json::array({{{"Path","/Game/A"},{"Name","Apple"}},{{"Path","/Game/B"},{"Name","Bronze"}},{{"Path","/Game/Z"},{"Name","Zinc"}}})},{"NextOffset",8192},{"HasMore",false}};
    };
    auto first=cache.Read("all","",2,source);
    assert(scans==1&&first["Rows"].empty()&&first["HasMore"]==true&&first["AlphabeticalReady"]==false);
    const auto scanCursor=first["NextCursor"].get<std::string>();
    first=cache.Read("all",scanCursor,2,source);
    assert(scans==2&&first["AlphabeticalReady"]==true&&first["IndexedItems"]==4);
    assert(first["Rows"][0]["Path"]=="/Game/A"&&first["Rows"][1]["Path"]=="/Mod/B"&&first["HasMore"]==true);
    const auto cursor=first["NextCursor"].get<std::string>();
    auto second=cache.Read("all",cursor,2,source);
    assert(scans==2&&second["Rows"][0]["Name"]=="Bronze"&&second["Rows"][1]["Name"]=="Zinc"&&!second.value("HasMore",true));
    auto replay=cache.Read("all",cursor,2,source);assert(replay==second&&scans==2);
    assert(cache.Read("all","",2,source)["Rows"]==first["Rows"]&&scans==2);
    // A saved presentation index restores the complete sorted roster without
    // any UObject scan, and remains isolated to its verified source scope.
    const auto folder=std::filesystem::temp_directory_path()/("rsmk-item-manifest-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(folder);const auto file=folder/"items.json";
    const Json oldStone={{"Path","/Game/Gameplay/Items/Resources/Mineral/ITEM_Resources_Stone.ITEM_Resources_Stone"},
        {"Name","Stone"},{"Category","Building"},{"Tags","Item.BuildingResource ItemFilter.Material.Mineral"}};
    const auto stoneFile=folder/"stone.json";
    PS::PagedItemCatalog::Save(stoneFile,"old-category-policy",{std::make_shared<const Json>(oldStone)});
    const auto stonePage=PS::PagedItemCatalog::Read(PS::PagedItemCatalog::File(stoneFile),"old-category-policy","__helpycat:resources Stone","",24);
    assert(stonePage["Rows"].size()==1&&stonePage["Rows"][0]["Category"]=="Resources");
    Cache oldCache;oldCache.Restore(Json::array({oldStone}));
    assert(oldCache.Snapshot()[0]->at("Category")=="Resources");
    std::filesystem::remove(PS::PagedItemCatalog::File(stoneFile));
    PS::PersistentItemCatalog::Save(file,"mounted-content-1",cache.Snapshot());
    auto restored=PS::PersistentItemCatalog::Load(file,"mounted-content-1");assert(restored&&restored->Ready());
    auto saved=restored->Read("all","",2,[&](size_t){assert(false);return Json{};});
    assert(saved["Rows"]==first["Rows"]&&scans==2);
    const auto pages=PS::PagedItemCatalog::File(file);
    auto diskPage=PS::PagedItemCatalog::Read(pages,"mounted-content-1","","",2);
    assert(diskPage["Rows"]==first["Rows"]&&diskPage["ReadRecords"]==2&&diskPage["HasMore"]==true);
    auto page2=PS::PagedItemCatalog::Read(pages,"mounted-content-1","",diskPage["NextCursor"],2);
    assert(page2["Rows"]==second["Rows"]&&page2["HasMore"]==false);
    assert(PS::PagedItemCatalog::Read(pages,"mounted-content-1","bronze","",2)["Rows"].size()==1);
    assert(PS::PagedItemCatalog::Read(pages,"mounted-content-1","none","",2)["Rows"].empty());
    assert(PS::PagedItemCatalog::Read(pages,"mounted-content-1","","A:old",2)["Reset"]==true);
    Reject([&]{PS::PagedItemCatalog::Read(pages,"different-content","","",2);});
    Reject([&]{PS::PagedItemCatalog::Read(pages,"mounted-content-1","","",129);});
    {PS::PagedItemCatalog::Store paged;auto loading=paged.Lookup(file,"mounted-content-1","","",2);assert(loading&&loading->at("PopulationStage")=="reading-catalogue");
        for(int i=0;i<2000&&loading&&loading->at("PopulationStage")=="reading-catalogue";++i){Sleep(1);loading=paged.Lookup(file,"mounted-content-1","","",2);}
        assert(loading&&loading->at("Rows")==first["Rows"]);assert(paged.Lookup(file,"mounted-content-1","","",2)->at("Rows")==first["Rows"]);
        auto stale=paged.Lookup(file,"different-content","","",2);for(int i=0;i<2000&&stale;++i){Sleep(1);stale=paged.Lookup(file,"different-content","","",2);}assert(!stale);paged.Shutdown();}
    auto filteredSaved=restored->Read("bronze","",2,[&](size_t){assert(false);return Json{};},[](const Json& row){return row.at("Name")=="Bronze";});
    assert(filteredSaved["Rows"].size()==1&&filteredSaved["Rows"][0]["Name"]=="Bronze");
    // Sparse matches several disk slices away return through one worker job,
    // rather than repeated empty host/UI round trips.
    const auto sparseFile=folder/"sparse.json";std::vector<std::shared_ptr<const Json>> diskRows;
    for(int i=0;i<6000;++i)diskRows.push_back(std::make_shared<const Json>(Json{{"Path","/Game/Item"+std::to_string(i)},{"Name",i==5000?"Health Tincture":"Other item"}}));
    PS::PagedItemCatalog::Save(sparseFile,"sparse-content",diskRows);
    {PS::PagedItemCatalog::Store paged;auto result=paged.Lookup(sparseFile,"sparse-content","Health","",24);
        assert(result&&result->at("PopulationStage")=="filtering-cache");
        for(int i=0;i<5000&&result&&!result->value("AlphabeticalReady",false);++i){Sleep(1);result=paged.Lookup(sparseFile,"sparse-content","Health","",24);}
        assert(result&&result->at("Rows").size()==1&&result->at("Rows")[0]["Name"]=="Health Tincture");
        assert(result->at("PopulationDone")==6000&&result->at("CacheHit")==true);paged.Shutdown();}
    std::filesystem::remove(PS::PagedItemCatalog::File(sparseFile));
    const auto familyFile=folder/"family.json";diskRows.clear();
    for(int i=0;i<5000;++i)diskRows.push_back(std::make_shared<const Json>(Json{{"Path","/Game/A"+std::to_string(i)},{"Name","Earlier item "+std::to_string(i)}}));
    for(const auto* slot:{"Body","Helm","Legs"})diskRows.push_back(std::make_shared<const Json>(Json{{"Path",std::string("/Game/Splitbark")+slot},{"Name",std::string("Splitbark ")+slot}}));
    PS::PagedItemCatalog::Save(familyFile,"family-scope",diskRows);
    const auto familyPage=PS::PagedItemCatalog::Read(PS::PagedItemCatalog::File(familyFile),"family-scope","__helpyfamily:splitbark","",24);
    assert(familyPage["Rows"].size()==3&&familyPage["Rows"][0]["Name"]=="Splitbark Body");
    std::set<std::string> paths;auto rotating=PS::PagedItemCatalog::Read(PS::PagedItemCatalog::File(familyFile),"family-scope","item","",128);
    for(;;){for(const auto& row:rotating["Rows"])assert(paths.insert(row.at("Path").get<std::string>()).second);if(!rotating.value("HasMore",false))break;rotating=PS::PagedItemCatalog::Read(PS::PagedItemCatalog::File(familyFile),"family-scope","item",rotating.at("NextCursor"),128);}
    assert(paths.size()==5000);std::filesystem::remove(PS::PagedItemCatalog::File(familyFile));
    const auto previewFile=folder/"preview.json";
    const auto teaFile=folder/"native-tea.json";const std::string teaPath="/UmbralSands/Gameplay/Items/Resources/Processed/ITEM_Resources_Tea_Dried.ITEM_Resources_Tea_Dried";
    PS::PagedItemCatalog::Save(teaFile,"tea-scope",std::vector<std::shared_ptr<const Json>>{std::make_shared<const Json>(Json{{"Path",teaPath},{"Name","Dried Tea Leaves"},{"SourceKind","modded"},{"SourceName","maxcape"}})});
    const auto officialTea=PS::PagedItemCatalog::Read(PS::PagedItemCatalog::File(teaFile),"tea-scope","__helpysource:expansion Dried Tea","",4,false,{},std::set<std::string>{"umbralsands"});
    assert(officialTea["Rows"].size()==1&&officialTea["Rows"][0]["SourceKind"]=="expansion");
    const auto notModded=PS::PagedItemCatalog::Read(PS::PagedItemCatalog::File(teaFile),"tea-scope","__helpysource:modded Dried Tea","",4,false,{},std::set<std::string>{"umbralsands"});
    assert(notModded["Rows"].empty());std::filesystem::remove(PS::PagedItemCatalog::File(teaFile));
    const auto previewScope=PS::PersistentItemCatalog::DiskScope({{"CatalogKey","content-A"}},Json::array(),"en-US");
    // A real mod roster exceeds the old 16 KiB sidecar header limit.
    Json roster=Json::array();for(int i=0;i<13000;++i)roster.push_back("/Game/Mods/Siege/Workshop/Contract"+std::to_string(i)+".Contract"+std::to_string(i));
    const auto largeScope=Json::array({"item-browse-v2","content-A",roster,"en-US"}).dump();
    assert(largeScope.size()>16384);
    const auto compactScope=PS::PersistentItemCatalog::DiskScope({{"CatalogKey","content-A"}},roster,"en-US");
    assert(compactScope.size()<256&&PS::PagedItemCatalog::CompactScope(largeScope)==compactScope);
    PS::PersistentItemCatalog::Save(previewFile,largeScope,cache.Snapshot());
    assert(PS::PagedItemCatalog::Read(PS::PagedItemCatalog::File(previewFile),compactScope,"bronze","",2)["Rows"].size()==1);
    bool compactMigrated=false;assert(PS::PersistentItemCatalog::Load(previewFile,compactScope,{},&compactMigrated)&&compactMigrated);
    const auto staleTime=std::filesystem::last_write_time(previewFile)-std::chrono::seconds(2);
    std::filesystem::last_write_time(PS::PagedItemCatalog::File(previewFile),staleTime);
    Reject([&]{PS::PagedItemCatalog::Read(PS::PagedItemCatalog::File(previewFile),compactScope,"bronze","",2);});
    {PS::PagedItemCatalog::Store worker;auto value=worker.Lookup(previewFile,compactScope,"bronze","",2);
        for(int i=0;i<2000&&value;++i){Sleep(1);value=worker.Lookup(previewFile,compactScope,"bronze","",2);}assert(!value);
        PS::PagedItemCatalog::Save(previewFile,compactScope,cache.Snapshot());
        for(int i=0;i<2000&&(!value||!value->value("AlphabeticalReady",false));++i){Sleep(1);value=worker.Lookup(previewFile,compactScope,"bronze","",2);}
        assert(value&&value->at("Rows").size()==1);worker.Shutdown();}
    PS::PersistentItemCatalog::Save(previewFile,previewScope,cache.Snapshot());
    const auto preview=PS::PagedItemCatalog::Read(PS::PagedItemCatalog::File(previewFile),"unverified","bronze","",2,true,"en-US");
    assert(preview["Rows"].size()==1&&preview["Rows"][0]["Name"]=="Bronze");
    Reject([&]{PS::PagedItemCatalog::Read(PS::PagedItemCatalog::File(previewFile),"wrong-current-scope","bronze","",2);});
    Reject([&]{PS::PagedItemCatalog::Read(PS::PagedItemCatalog::File(previewFile),"unverified","bronze","",2,true,"de-DE");});
    {PS::PagedItemCatalog::Store worker;auto result=worker.Preview(previewFile,"en-US","bronze","",2);
        for(int i=0;i<2000&&result&&!result->value("AlphabeticalReady",false);++i){Sleep(1);result=worker.Preview(previewFile,"en-US","bronze","",2);}
        assert(result&&result->at("Rows").size()==1&&result->at("CachePreview")==true&&result->at("CheckingChanges")==true);worker.Shutdown();}
    {PS::PagedItemCatalog::Store strict,presentation;
        auto verified=strict.Lookup(previewFile,"changed-mount-scope","bronze","",2);
        auto cached=presentation.Preview(previewFile,"en-US","bronze","",2);
        for(int i=0;i<2000&&(verified||!cached||!cached->value("CachePreview",false));++i){
            Sleep(1);verified=strict.Lookup(previewFile,"changed-mount-scope","bronze","",2);cached=presentation.Preview(previewFile,"en-US","bronze","",2);
        }
        assert(!verified&&!strict.Failure.empty()&&cached&&cached->at("CachePreview")==true&&cached->at("Rows").size()==1);
        // Alternating requests must not erase the strict failure and restart
        // its scope probe forever, nor discard the still-usable presentation.
        for(int i=0;i<8;++i){assert(!strict.Lookup(previewFile,"changed-mount-scope","bronze","",2));assert(presentation.Preview(previewFile,"en-US","bronze","",2)->at("Rows").size()==1);}
        strict.Shutdown();presentation.Shutdown();}
    {PS::PersistentItemCatalog::Store worker;worker.Repair(previewFile,previewScope,[](Json& row){row["SourceName"]="exact-mod";row["SourceEvidence"]="Exact RuneSchema asset definition";return true;});worker.Shutdown();}
    auto repaired=PS::PersistentItemCatalog::Load(previewFile,previewScope);assert(repaired&&repaired->Snapshot()[0]->at("SourceName")=="exact-mod");
    std::filesystem::remove(PS::PagedItemCatalog::File(previewFile));std::filesystem::remove(previewFile);
    assert(!PS::PersistentItemCatalog::DurableRow({{"Path","/Engine/Transient/Preview.Preview"},{"Name","Ephemeral preview"}}));
    assert(!PS::PersistentItemCatalog::DurableRow({{"Path","/Mod/Incomplete.Incomplete"},{"Name","Incomplete"},{"Available",false}}));
    assert(PS::PersistentItemCatalog::DurableRow({{"Path","/Game/RuneSchema/Clone.Clone"},{"Name","Registered clone"},{"Available",true}}));
    assert(!PS::PersistentItemCatalog::Load(file,"changed-mods"));
    const Json sources={{"CatalogKey","content-1"},{"Key","icon-plan-1"},{"PlanRevision",1}};
    const auto diskScope=PS::PersistentItemCatalog::DiskScope(sources,Json::array(),"en-US");
    auto refined=sources;refined["Key"]="icon-plan-2";refined["PlanRevision"]=2;
    assert(diskScope==PS::PersistentItemCatalog::DiskScope(refined,Json::array(),"en-US"));
    refined["CatalogKey"]="content-2";assert(diskScope!=PS::PersistentItemCatalog::DiskScope(refined,Json::array(),"en-US"));
    assert(diskScope!=PS::PersistentItemCatalog::DiskScope(sources,Json::array({"/Game/New.New"}),"en-US"));
    assert(diskScope!=PS::PersistentItemCatalog::DiskScope(sources,Json::array(),"de-DE"));
    bool migrated=false;auto legacy=PS::PersistentItemCatalog::Load(file,diskScope,"mounted-content-1",&migrated);
    assert(legacy&&legacy->Ready()&&migrated);
    assert(!PS::PersistentItemCatalog::Load(file,diskScope,"different-old-content"));
    // Worker results from an old scope must not replace the current roster.
    PS::PersistentItemCatalog::Store disk;auto fresh=std::make_unique<Cache>();fresh->Scope("new");
    assert(!disk.Restore(file,"mounted-content-1",fresh));
    for(int i=0;i<2000&&!disk.Restore(file,"changed-mods",fresh);++i)Sleep(1);
    assert(!fresh->Ready());disk.Shutdown();
    {std::ofstream broken(file);broken<<"{broken";}
    assert(!PS::PersistentItemCatalog::Load(file,"mounted-content-1"));
    Cache malformed;Reject([&]{malformed.Restore(Json::array({{{"Path","../bad"},{"Name","Bad"}}}));});
    Cache partial;partial.Read("all","",2,[](size_t){return Json{{"Rows",Json::array()},{"NextOffset",1},{"HasMore",false},{"MountedMetadataReady",false}};});
    assert(partial.Ready()&&!partial.Persistable()); // A live-only fallback must not become tomorrow's full manifest.
    {std::ofstream corrupt(pages,std::ios::binary|std::ios::trunc);corrupt<<"broken";}
    Reject([&]{PS::PagedItemCatalog::Read(pages,"mounted-content-1","","",2);});
    std::filesystem::remove(pages);std::filesystem::remove(file);std::filesystem::remove(folder);
    for(int i=0;i<8;++i){auto filtered=cache.Read("search"+std::to_string(i),"",2,source,[](const Json& row){return row.at("Name")=="Bronze";});
        assert(filtered["Rows"].size()==1&&filtered["Rows"][0]["Name"]=="Bronze"&&!filtered.value("HasMore",true));}
    assert(scans==2);std::cout<<"Shared index: 2 native slices for 9 queries; former per-query index required 18.\n";
    const auto stale=cursor;cache.Scope("world/source/clone-after-change");
    auto recovered=cache.ReadRecovering("all",stale,2,source);
    assert(recovered["Reset"]==true&&recovered["CursorRecovered"]==true&&recovered["HasMore"]==true);
    recovered=cache.ReadRecovering("all",recovered["NextCursor"].get<std::string>(),2,source);
    assert(recovered["Rows"].size()==2&&recovered["Rows"][0]["Name"]=="Apple");
    // Recreate the all-query ticket if LRU eviction removed it.
    first=cache.Read("all","",2,source);
    Reject([&]{cache.Read("all","A:999:0:0",2,source);});
    Reject([&]{cache.Read("all","A:1:-1:0",2,source);});
    Reject([&]{cache.Read("all","A:1:999:0",2,source);});
    Reject([&]{cache.Read("all","A:1:0:0:bad",2,source);});
    Reject([&]{cache.Read("all","",129,source);});
    const auto noRows=[](size_t){return Json{{"Rows",Json::array()},{"NextOffset",10},{"HasMore",false}};};
    for(size_t i=0;i<Cache::MaxQueries;++i)cache.Read("filter"+std::to_string(i),"",10,noRows);
    assert(cache.Queries()==Cache::MaxQueries);
    Reject([&]{cache.Read("all",cursor,2,source);});
    cache.Scope("single-player/source/clone-2");assert(cache.Queries()==0);
    Reject([&]{cache.Read("all",cursor,2,source);});
    auto empty=cache.Read("empty","",10,noRows);assert(empty["Rows"].empty()&&!empty.value("HasMore",true));
    Cache stalled;Reject([&]{stalled.Read("stalled","",10,[](size_t){return Json{{"Rows",Json::array()},{"NextOffset",0},{"HasMore",true}};});});
    // Empty filters and sparse matches advance without another native scan.
    Cache large;size_t sourceCalls=0;const auto largeSource=[&](size_t){++sourceCalls;Json rows=Json::array();for(size_t i=0;i<5000;++i)rows.push_back({{"Path","/Game/"+std::to_string(i)},{"Name",std::to_string(i)}});return Json{{"Rows",rows},{"NextOffset",5000},{"HasMore",false}};};
    std::string largeCursor;Json page;for(int i=0;i<12;++i){page=large.Read("none",largeCursor,2,largeSource,[](const Json&){return false;});largeCursor=page["NextCursor"];if(!page.value("HasMore",true))break;}
    assert(sourceCalls==1&&page["Rows"].empty()&&!page.value("HasMore",true));
    Cache sparse;size_t calls=0;std::string next;
    for(size_t i=0;i<12;++i){auto result=sparse.Read("sparse",next,1,[&](size_t offset){++calls;return Json{{"Rows",Json::array()},{"NextOffset",offset+2048},{"HasMore",true}};});
        assert(result["Rows"].empty()&&result["HasMore"]==true);assert(result["NextCursor"]!=next);next=result["NextCursor"];
    }assert(calls==12);
    if(argc>=3){
        const auto read=[](const char* path){std::ifstream input(path,std::ios::binary);assert(input.good());return std::string(std::istreambuf_iterator<char>(input),{});};
        assert(read(argv[1])==read(argv[2]));
        if(argc==4){const auto native=read(argv[3]);assert(native.find("PS::AssetSearch::AlphabeticalItemWindow(query,cursor,limit,fieldSet,PS::HelpyBrowsePolicy::Read")!=std::string::npos);
            assert(native.find("PS::AssetSearch::ItemWindow(query,offset,limit,0,false,fieldSet,4096,1800,true)")!=std::string::npos);}
    }
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
