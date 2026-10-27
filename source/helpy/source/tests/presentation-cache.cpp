#include "Core/HelpyIconPack.h"
#include "Core/HelpyEnvironment.h"
#include "Runtime/HelpyPaths.h"
#include "Generator/HelpyCachedCatalog.h"
#include <cassert>
#include <thread>
#include <iostream>
#include <cstring>
using namespace PS;using Json=nlohmann::json;namespace fs=std::filesystem;
template<class F>void Reject(F action){bool rejected=false;try{action();}catch(...){rejected=true;}assert(rejected);}
int main(int argc,char** argv){
    if((argc>=4&&argc<=6)&&std::string_view(argv[1])=="--probe"){
        const fs::path cache=argv[2],root=argv[3];HelpyPaths::Configure(root/"plugins"/"RuneSchema.Helpy");HelpyEnvironment::Current=HelpyEnvironment::Kind::GamePass;
        if(argc>=5){std::ifstream input(argc==6?fs::path(argv[5]):cache/"sources.json");HelpySourceCache::AcceptRemote(Json::parse(input));}
        const auto records=CacheBundleStore::Read(cache,root,HelpySourceCache::Current,CacheBundleStore::Locale(),"items");std::cout<<"References="<<records.size()<<" Cache="<<HelpySourceCache::Directory(false)<<std::endl;
        unsigned calls=0;LazyCatalog::RemoteSubmit=[&](const Json&){++calls;return false;};HelpyRuntimePreferences::PassiveRefresh=false;HelpyCachedCatalog::Configure();
        const auto started=std::chrono::steady_clock::now();
        if(argc>=5)LazyCatalog::Search(0,argv[4]);else LazyCatalog::FirstOpen(SpawnToolRequests::CurrentGeneration());
        const auto wait=[](){for(int n=0;n<30000&&LazyCatalog::For(0).Pending;++n){LazyCatalog::CheckTimeouts();std::this_thread::sleep_for(std::chrono::milliseconds(1));}};wait();
        const auto& state=LazyCatalog::For(0);std::cout<<Json{{"Rows",state.Rows.size()},{"Pending",state.Pending},{"Error",state.LastError},{"NativeRequests",calls},{"SourceReady",HelpySourceCache::Ready()},{"LocalRequest",HelpyCachedCatalog::Pending.has_value()},{"Milliseconds",std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count()},{"Names",state.Rows}}.dump()<<std::endl;
        if(argc>=5&&!state.Rows.empty()){
            LazyCatalog::Search(0,"stone");wait();const auto warm=std::chrono::steady_clock::now();LazyCatalog::Search(0,argv[4]);wait();
            std::cout<<Json{{"RepeatRows",state.Rows.size()},{"NativeRequests",calls},{"RepeatMilliseconds",std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-warm).count()}}.dump()<<std::endl;
        }
        LazyCatalog::ClearRemote();return state.Rows.empty()?1:0;
    }
    const auto fixture=fs::temp_directory_path()/("rs-presentation-"+std::to_string(GetCurrentProcessId()));fs::create_directories(fixture);
    _wputenv_s(L"LOCALAPPDATA",fixture.c_str());HelpyPaths::Configure(fixture/"Game"/"Mods"/"RuneSchema"/"plugins"/"Helpy");
    HelpyEnvironment::Current=HelpyEnvironment::Kind::Steam;const auto steam=HostServices::CacheDirectory();
    HelpyEnvironment::Current=HelpyEnvironment::Kind::GamePass;const auto gamepass=HostServices::CacheDirectory();
    assert(steam!=gamepass&&gamepass==RuneSchemaData::PlatformDirectory("gamepass")/"cache");
    HelpyEnvironment::Current=HelpyEnvironment::Kind::Steam;const auto cache=HelpySourceCache::Directory(false);assert(cache==steam/"item-icons");
    const auto locale=CacheBundleStore::Locale();
    const std::string path="/Example/Items/DA_Tea.DA_Tea",art="/Example/Art/T_Tea.T_Tea";
    Json row={{"Kind","items"},{"Owner","mods/published-name"},{"Path",path},{"Data",{{"Name","Cached tea"},{"Icon",art},{"PowerLevel",4},{"Grant",true},{"ObservedCurrent",true}}}};
    std::vector<char> pixels(20,100);std::memcpy(pixels.data(),"RSI3",4);const uint32_t sizes[]{1,1,4};std::memcpy(pixels.data()+4,sizes,12);
    const auto pack=[&](const fs::path& file,const Json& records,uint64_t version,char fingerprint){
        const Json manifest={{"Version",2},{"Kind","RuneSchemaCacheBundle"},{"Locale",locale},{"CacheVersion",version},{"Sources",{{"example",std::string(64,fingerprint)}}},{"ContentOwner","mods/published-name"},{"Records",records},
            {"Icons",Json::array({{{"Icon",art},{"Source","example"},{"SHA256",HelpyIconPack::Hash(pixels)},{"Bytes",pixels.size()}}})}};
        const auto text=manifest.dump();const uint32_t length=uint32_t(text.size());fs::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary);
        out.write(HelpyIconPack::BundleMagic,8);out.write(reinterpret_cast<const char*>(&length),4);out<<text;out.write(pixels.data(),pixels.size());
    };
    auto file=fixture/"Old folder"/"cache"/"test.rsmk-cache";pack(file,Json::array({row}),1,'a');
    HelpyIconPack::Progress progress;HelpySourceCache::Snapshot unavailable;
    auto active=unavailable;active.Authoritative=true;active.Key="active-sources";active.CatalogKey="active-items";active.SourceKeys={{"example","0123456789abcdef"},{"game","fedcba9876543210"}};active.Owners={{"example",{"published-name"}}};active.DeclaredOwners=active.Owners;
    const auto import=[&](const fs::path& f){progress.Cancel=false;return HelpyIconPack::Import(f,fixture/"renamed runtime",cache,unavailable,1024*1024,progress,HelpyIconPack::ImportPolicy::Presentation);};
    auto result=import(file);assert(result.Imported==1&&result.RecordsImported==1);
    const auto records=CacheBundleStore::Read(cache,fixture/"different install",active,locale,"items");assert(records.size()==1&&!records[0]["Data"].at("ObservedCurrent").get<bool>()&&!records[0]["Data"].contains("Grant"));
    assert(CacheBundleStore::Read(cache,fixture,unavailable,locale,"items").empty());
    assert(CacheBundleStore::Read(cache,fixture,unavailable,locale,"items",false).empty());
    const auto picture=HelpyPresentationCache::IconFile(cache,art);assert(fs::is_regular_file(picture));
    assert(CacheBundleIO::Hash(CacheBundleIO::Read(picture,1024))==picture.stem().string());
    const auto index=HelpyPresentationCache::Directory(cache)/"index.json";const auto before=CacheBundleIO::Read(index,1024*1024);
    const auto renamed=fixture/"Different mod folder"/"cache"/"test.rsmk-cache";fs::create_directories(renamed.parent_path());fs::copy_file(file,renamed);
    result=import(renamed);assert(result.Imported==0&&result.RecordsImported==0&&CacheBundleIO::Read(index,1024*1024)==before);
    assert(HelpyPresentationCache::Records(gamepass/"item-icons",locale).empty());
    // A completely unavailable game bridge must not hold up browsing or paging.
    HelpySourceCache::Current=active;HelpyRuntimePreferences::PassiveRefresh=false;unsigned liveRequests=0;std::vector<Json> nativeQueries;LazyCatalog::RemoteSubmit=[&](const Json& request){++liveRequests;nativeQueries.push_back(request);return true;};
    HelpyCachedCatalog::Configure();LazyCatalog::FirstOpen(0);
    for(unsigned i=0;i<1000&&LazyCatalog::For(0).Pending;++i){LazyCatalog::CheckTimeouts();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(liveRequests==0&&LazyCatalog::For(0).Rows.size()==1&&LazyCatalog::For(0).Rows[0]["Name"]=="Cached tea");
    assert(LazyCatalog::Search(0,"cannon"));
    for(unsigned i=0;i<1000&&nativeQueries.empty();++i){LazyCatalog::CheckTimeouts();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(liveRequests==1&&nativeQueries[0]["Mode"]=="Search"&&nativeQueries[0]["Query"]=="cannon"&&!nativeQueries[0].contains("Cursor"));
    const auto reply=[&](size_t request,bool more){const auto q=nativeQueries.at(request);return Json{{"Category",0},{"Mode","Search"},{"Query","cannon"},{"RequestId",q.at("RequestId")},{"CancelGeneration",q.at("CancelGeneration")},{"RuntimeGeneration",q.at("ClientGeneration")},{"Reset",q.at("Reset")},{"Rows",Json::array({{{"Path","/Example/Items/Cannon"+std::to_string(request)+".Cannon"},{"Name","Mod cannon"}}})},{"NextCursor","S:64"},{"HasMore",more}};};
    LazyCatalog::ApplyWindow(reply(0,true),1);
    assert(nativeQueries.size()==2&&nativeQueries[1]["Mode"]=="Search"&&nativeQueries[1]["Cursor"]=="S:64");
    LazyCatalog::ApplyWindow(reply(1,false),2);assert(LazyCatalog::For(0).Rows.size()==2&&!LazyCatalog::For(0).Pending);LazyCatalog::ClearRemote();
    // A partial base cache must merge native mod discoveries even when its
    // downloaded index is newer than the already saved native observations.
    const auto native=cache.parent_path()/"item-catalogues"/"native-items.json";
    const Json discovered={{"Name","Discovered cannon"},{"Path","/Example/Items/DA_Cannon.DA_Cannon"},{"SourceKind","modded"},{"SourceName","published-name"}};
    CacheBundleIO::Publish(native,Json{{"Version",1},{"Scope",Json::array({"item-browse-v2",active.CatalogKey,Json::array(),locale}).dump()},{"Rows",Json::array({discovered})}}.dump());
    fs::last_write_time(native,fs::file_time_type::clock::now()-std::chrono::hours(24));
    const auto lookup=[&](const auto& snapshot){CacheBundleItems::Store store;std::optional<Json> page;for(unsigned i=0;i<1000;++i){page=store.Lookup(cache,fixture,snapshot,locale,"cannon","",28);if(page||!store.Awaiting())break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}store.Shutdown();return page;};
    const auto combined=lookup(active);assert(combined&&combined->at("Rows").size()==1&&combined->at("Rows")[0]["Name"]=="Discovered cannon");
    const auto savedScope=Json::array({"item-browse-v2","older-game-version",Json::array(),locale}).dump();
    PagedItemCatalog::Save(native,savedScope,{std::make_shared<const Json>(discovered)});
    const auto paged=lookup(active);assert(paged&&paged->at("Rows").size()==1&&paged->at("NextCursor").get<std::string>().starts_with("P:"));
    {CacheBundleItems::Store store;std::optional<Json> end;for(unsigned i=0;i<1000;++i){
        end=store.Lookup(cache,fixture,active,locale,"cannon",paged->at("NextCursor"),28);if(end)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }assert(end&&end->at("Rows").empty()&&!end->value("HasMore",true));store.Shutdown();}
    HelpyCachedCatalog::Configure();HelpyRuntimePreferences::PassiveRefresh=true;HelpyCachedCatalog::NextPassive={};
    unsigned warmRequests=0;LazyCatalog::RemoteSubmit=[&](const Json& q){assert(q.at("Mode")=="Alphabetical"&&q.at("Limit")==1);++warmRequests;return true;};
    LazyCatalog::For(0).CacheHit=false;LazyCatalog::For(0).Pending=false;HelpyCachedCatalog::Pump();
    assert(warmRequests==1);HelpyCachedCatalog::Pump();assert(warmRequests==1); // Discovery also starts without a cache hit and remains throttled.
    LazyCatalog::ClearRemote();HelpyRuntimePreferences::PassiveRefresh=false;
    LazyCatalog::Windows={};HelpyCachedCatalog::Configure();unsigned restartedQueries=0;LazyCatalog::RemoteSubmit=[&](const Json&){++restartedQueries;return false;};
    assert(LazyCatalog::Search(0,"cannon"));for(unsigned i=0;i<1000&&LazyCatalog::For(0).Pending;++i){LazyCatalog::CheckTimeouts();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(restartedQueries==0&&LazyCatalog::For(0).Rows.size()==1&&LazyCatalog::For(0).Rows[0]["Name"]=="Discovered cannon");LazyCatalog::ClearRemote();
    // More than eight file-worker scan slices: the final empty P: page must
    // fall through to live search, while populated cache results stay local.
    std::vector<std::shared_ptr<const Json>> large;
    for(int i=0;i<17000;++i)large.push_back(std::make_shared<const Json>(Json{{"Name","Stone"+std::to_string(i)},{"Path","/Example/Items/Stone"+std::to_string(i)+".Stone"}}));
    PagedItemCatalog::Save(native,savedScope,large);
    LazyCatalog::Windows={};HelpyCachedCatalog::Configure();std::vector<Json> misses;
    LazyCatalog::RemoteSubmit=[&](const Json& request){misses.push_back(request);return false;};
    assert(LazyCatalog::Search(0,"missing cannon"));
    for(unsigned i=0;i<10000&&LazyCatalog::For(0).Pending;++i){LazyCatalog::CheckTimeouts();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(misses.size()==1&&misses[0]["Mode"]=="Search"&&misses[0]["Query"]=="missing cannon"&&!misses[0].contains("Cursor"));
    LazyCatalog::ClearRemote();fs::remove(PagedItemCatalog::File(native));
    auto disabled=active;disabled.Owners.clear();disabled.DeclaredOwners.clear();assert(CacheBundleStore::Read(cache,fixture,disabled,locale,"items").empty());assert(!lookup(disabled));
    auto relabelled=active;relabelled.Owners["example"]={"renamed-folder"};relabelled.DeclaredOwners=relabelled.Owners;
    HelpyIconPack::Import(renamed,fixture,cache,unavailable,1024*1024,progress,HelpyIconPack::ImportPolicy::Presentation,{}, {},"mods/renamed-folder");
    assert(CacheBundleStore::Read(cache,fixture,relabelled,locale,"items").size()==1);
    // A new cache version replaces removed records and cannot be rolled back by an old pack.
    auto replacement=row;replacement["Path"]="/Example/Items/DA_New.DA_New";replacement["Data"]["Name"]="New tea";
    const auto update=fixture/"v2.rsmk-cache";pack(update,Json::array({replacement}),2,'b');import(update);
    auto current=HelpyPresentationCache::Records(cache,locale,"items");assert(current.size()==1&&current[0]["Path"]==replacement["Path"]);
    import(file);assert(HelpyPresentationCache::Records(cache,locale,"items")[0]["Path"]==replacement["Path"]);
    // Cancellation, malformed pixels and budget failure all keep the last committed index.
    const auto good=CacheBundleIO::Read(index,1024*1024);progress.Cancel=true;
    Reject([&]{HelpyIconPack::Import(update,fixture,cache,unavailable,1024*1024,progress,HelpyIconPack::ImportPolicy::Presentation);});progress.Cancel=false;
    const auto bad=fixture/"bad.rsmk-cache";pack(bad,Json::array({row}),3,'c');{std::fstream out(bad,std::ios::binary|std::ios::in|std::ios::out);out.seekp(-1,std::ios::end);out.put(0);}
    Reject([&]{import(bad);});
    Reject([&]{HelpyIconPack::Import(update,fixture,cache,unavailable,1,progress,HelpyIconPack::ImportPolicy::Presentation);});
    assert(CacheBundleIO::Read(index,1024*1024)==good);
    if(argc==2){const auto full=fixture/"published"/"item-icons";const auto started=std::chrono::steady_clock::now();
        const auto imported=HelpyIconPack::Import(argv[1],fixture,full,unavailable,200*1024*1024,progress,HelpyIconPack::ImportPolicy::Presentation);
        const auto ready=HelpyPresentationCache::Records(full,locale,"items");const auto all=HelpyPresentationCache::Records(full,locale);assert(all.size()==2903&&ready.size()==1545&&imported.Imported>4000);
        const auto again=HelpyIconPack::Import(argv[1],fixture/"renamed again",full,unavailable,200*1024*1024,progress,HelpyIconPack::ImportPolicy::Presentation);
        assert(again.Imported==0&&again.RecordsImported==0&&again.Unchanged==4246);
        std::cout<<"Published cache: "<<all.size()<<" item/journal records, "<<again.Unchanged<<" picture relationships; identical reimport reuses every file. "<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count()<<" ms including both imports\n";
    }
    fs::remove_all(fixture);std::cout<<"Platform isolation, rename reuse, offline catalogue, revisions, integrity and rollback passed\n";
}
