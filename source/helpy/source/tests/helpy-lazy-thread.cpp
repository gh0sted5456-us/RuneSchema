#include "Generator/LazyCatalog.h"
#include "Core/CatalogWindowProgress.h"
#include <cassert>
#include <thread>
int main(){
    using namespace PS::LazyCatalog;
    const auto resetFixture=[] {for(auto& state:Windows){const auto cancel=state.CancelGeneration;state=State{};state.CancelGeneration=cancel;}};
    PS::SpawnToolRequests::Generation=10;
    unsigned submissions=0;
    ConfigureRemote([&](json){++submissions;return true;},[](int,uint64_t){});
    FirstOpenPendingGeneration=10;For(0)=State{};
    std::thread producer([]{for(unsigned i=0;i<20000;++i)BackendSignal(10);});
    // Producer does not read or mutate any UI-owned JSON or state.
    producer.join();assert(submissions==0);assert(!For(0).Pending);
    PumpBackendSignal();assert(submissions==1);assert(For(0).Pending);
    BackendSignal(10);PumpBackendSignal();assert(submissions==1);
    ResetForGeneration(11);assert(For(0).RuntimeGeneration==11 && For(0).Rows.empty());
    PS::SpawnToolRequests::Generation=11;For(0)=State{};submissions=0;
    assert(Request(0,"Search","test",true,20));
    const auto respond=[&](int start,int count,bool reset,bool more){
        auto& state=For(0);json rows=json::array();for(int i=start;i<start+count;++i)rows.push_back({{"Path","/Game/Item"+std::to_string(i)}});
        ApplyWindow({{"Category",0},{"Mode","Search"},{"CancelGeneration",state.CancelGeneration},{"RuntimeGeneration",11},
            {"RequestId",state.ActiveRequestId},{"Reset",reset},{"Rows",rows},{"HasMore",more}},submissions);
    };
    respond(0,20,true,true);assert(!For(0).Pending&&submissions==1);
    FillTo(0,60);assert(For(0).Pending&&For(0).FillTarget==60&&submissions==2);
    respond(20,20,false,true);assert(For(0).Pending&&For(0).FillTarget==60&&submissions==3);
    respond(40,20,false,true);assert(!For(0).Pending&&For(0).Rows.size()==60&&submissions==3);
    FillTo(0,60);assert(submissions==3); // No moving target or background full-catalog drain.
    FillTo(0,80);assert(submissions==4);
    respond(0,0,false,true);respond(0,0,false,true);respond(0,0,false,true);
    assert(!For(0).Pending&&!For(0).LastError.empty()&&For(0).StalledWindows==3);
    const auto stopped=submissions;FillTo(0,80);assert(submissions==stopped);
    // A sparse native object roster can produce many empty slices before an
    // ItemData object. Advancing cursors are progress, not a stalled query.
    assert(Retry(0));
    for(int i=1;i<=12;++i){
        auto& state=For(0);
        ApplyWindow({{"Category",0},{"Mode","Random"},{"RequestId",state.ActiveRequestId},{"CancelGeneration",state.CancelGeneration},
            {"RuntimeGeneration",11},{"Reset",i==1},{"Rows",json::array()},{"NextCursor","R:123:"+std::to_string(i*2048)},{"HasMore",true}},submissions);
        assert(state.Pending&&state.LastError.empty()&&state.StalledWindows==0);
    }
    respond(0,28,false,false);assert(For(0).Rows.size()==28&&!For(0).Pending);
    assert(Retry(0));const auto oldCancel=For(0).CancelGeneration;
    CheckTimeouts(For(0).RequestedAt+std::chrono::seconds(16));
    assert(!For(0).Pending&&!For(0).LastError.empty()&&For(0).CancelGeneration>oldCancel);
    // A late response after timeout must not erase a newer request's data.
    assert(Retry(0));
    const auto lastGoodBeforeLate=For(0).Rows;
    ApplyWindow({{"Category",0},{"CancelGeneration",oldCancel},{"Rows",json::array({{{"Path","stale"}}})}},100);
    assert(For(0).Pending&&For(0).Rows==lastGoodBeforeLate);
    For(1)=State{};json fallback={{"Definitions",json::array({{{"Key","native"}}})}};Overlay(fallback,1);assert(fallback["Definitions"].size()==1);
    For(0)=State{};For(0).Started=true;For(0).Pending=true;
    json live={{"Items",json::array({{{"Path","/Game/Real.Item"},{"Icon","/Game/Icons/Real.Real"}}})}};
    Overlay(live,0);assert(live["Items"].size()==1&&live["Items"][0]["Icon"]=="/Game/Icons/Real.Real");
    InvalidateSources(true);Overlay(live,0);assert(live["Items"].size()==1);assert(Search(0,"removed item"));
    InvalidateSources(false);assert(Search(0,"fresh"));const auto cancelled=For(0).CancelGeneration;assert(Search(0,"new"));
    ApplyWindow({{"Category",0},{"CancelGeneration",cancelled},{"Rows",json::array({{{"Path","stale-source"}}})}},101);assert(For(0).Rows.empty());
    // Source inspection may complete after the UI's first browse. Its cancel
    // fence must survive FirstOpen and world-generation resets on either lane.
    uint64_t fence=0;unsigned accepted=0;
    ConfigureRemote([&](json request){const auto cancel=request.at("CancelGeneration").get<uint64_t>();if(cancel<fence)return false;++accepted;return true;},
        [&](int category,uint64_t cancel){if(category==0)fence=std::max(fence,cancel);});
    InvalidateSources(false);assert(fence>1);accepted=0;
    PS::SpawnToolRequests::Generation=12;FirstOpen(12);
    assert(accepted==1&&For(0).Pending&&For(0).CancelGeneration>=fence);
    ResetForGeneration(13);PS::SpawnToolRequests::Generation=13;FirstOpen(13);
    assert(accepted==2&&For(0).CancelGeneration>=fence);
    // A native clone/hover icon confirmation updates all cached searches and
    // survives stale in-flight basic rows without creating new catalogue items.
    For(0)=State{};auto& cloneWindow=For(0);cloneWindow.CancelGeneration=fence;cloneWindow.RuntimeGeneration=13;cloneWindow.Started=true;
    const json stale={{"Path","/Game/RuneSchema/MyClones/Helm.Helm"},{"Icon","/Game/UI/Source.Source"}};
    cloneWindow.Rows=json::array({stale});cloneWindow.Cache["helm"]={cloneWindow.Rows,"",false,13};
    RecordNativeIcon(stale.at("Path"),"/Mod/UI/Clone.Clone");assert(cloneWindow.Rows[0]["Icon"]=="/Mod/UI/Clone.Clone");
    assert(cloneWindow.Cache.at("helm").Rows[0]["Icon"]=="/Mod/UI/Clone.Clone");
    assert(Request(0,"Search","helm",true,1));
    ApplyWindow({{"Category",0},{"CancelGeneration",cloneWindow.CancelGeneration},{"RuntimeGeneration",13},{"RequestId",cloneWindow.ActiveRequestId},
        {"Reset",true},{"Rows",json::array({stale})},{"HasMore",false}},102);
    assert(cloneWindow.Rows.size()==1&&cloneWindow.Rows[0]["Icon"]=="/Mod/UI/Clone.Clone");
    RecordNativeIcon(stale.at("Path"),"");assert(cloneWindow.Rows[0]["Icon"]=="");
    InvalidateSources(false);assert(NativeIcons.empty());
    InvalidateSources(false);resetFixture();auto& cached=For(0);cached.Started=true;cached.RuntimeGeneration=13;cached.HasMore=false;cached.Coverage="cached";
    for(int i=0;i<MaxCachedQueries;++i){cached.Query="query"+std::to_string(i);cached.Rows=json::array({{{"Path","/Game/I"+std::to_string(i)}}});SaveCache(0);}
    assert(RestoreCache(0,"query0",13));cached.Query="new";cached.Rows=json::array({{{"Path","/Game/New"}}});SaveCache(0);
    assert(cached.Cache.size()==MaxCachedQueries&&cached.Cache.contains("query0")&&!cached.Cache.contains("query1"));
    assert(RestoreCache(0,"query0",13)&&cached.Coverage=="cached");
    const auto cachedSubmissions=accepted;assert(Search(0,"query0")&&accepted==cachedSubmissions&&!cached.Pending);
    cached.Query="empty";cached.Rows=json::array();cached.HasMore=false;SaveCache(0);
    assert(RestoreCache(0,"empty",13)&&cached.Rows.empty()&&!cached.HasMore);
    assert(!RestoreCache(0,"query0",14));
    cached.Query.clear();cached.Rows=json::array({{{"Path","/FutureExpansion/Items/X.X"},{"SourceKind","unknown"}}});cached.HasMore=false;SaveCache(0);
    const auto metadataRequests=accepted;RefreshSourceMetadata({},{"futureexpansion"});
    assert(accepted==metadataRequests&&cached.Rows[0]["SourceKind"]=="expansion"&&cached.Cache.at("@alphabetical").Rows[0]["SourceKind"]=="expansion");
    InvalidateSources(true);assert(RestoreCache(0,"query0",13)&&cached.CheckingChanges);
    assert(Search(0,"query0")&&cached.Pending&&cached.Rows.size()==1); // Stale search remains visible while refreshing.
    InvalidateSources(false);resetFixture();assert(Request(0,"Alphabetical","",true,28));
    auto& waiting=For(0);const auto beforeWait=accepted;
    ApplyWindow({{"Category",0},{"CancelGeneration",waiting.CancelGeneration},{"RuntimeGeneration",13},{"RequestId",waiting.ActiveRequestId},
        {"Rows",json::array()},{"NextCursor","W:1"},{"HasMore",true},{"AlphabeticalReady",false}},103);
    assert(waiting.PreparingAlphabetical&&!waiting.Pending&&accepted==beforeWait);
    assert(!RequestMore(0)); // Do not hammer the host while its file worker verifies mounts.
    waiting.ResumeAt={};assert(RequestMore(0)&&accepted==beforeWait+1);
    for(unsigned i=0;i<5;++i){auto& pending=For(0);ApplyWindow({{"Category",0},{"CancelGeneration",pending.CancelGeneration},{"RuntimeGeneration",13},{"RequestId",pending.ActiveRequestId},
        {"Rows",json::array()},{"NextCursor",pending.NextCursor},{"HasMore",true},{"AlphabeticalReady",false},{"PopulationStage","reading-catalogue"}},104+i);
        assert(pending.LastError.empty()&&pending.StalledWindows==0&&pending.PopulationStage=="reading-catalogue");pending.ResumeAt={};assert(RequestMore(0));}
    // Same-query reload keeps usable rows through empty cache-wait slices,
    // then replaces (not appends to) them when fresh rows arrive.
    InvalidateSources(false);resetFixture();auto& stable=For(0);stable.RuntimeGeneration=13;stable.Started=true;stable.Query="keep";
    stable.Rows=json::array({{{"Path","/Game/Old"}}});stable.HasMore=false;
    assert(Retry(0)&&stable.Refreshing&&stable.Rows.size()==1);
    ApplyWindow({{"Category",0},{"CancelGeneration",stable.CancelGeneration},{"RuntimeGeneration",13},{"RequestId",stable.ActiveRequestId},
        {"Reset",true},{"Rows",json::array()},{"NextCursor","W:cache"},{"HasMore",true},{"PopulationStage","reading-catalogue"}},120);
    assert(stable.Refreshing&&stable.Rows[0]["Path"]=="/Game/Old");stable.ResumeAt={};assert(FillTo(0,1));
    ApplyWindow({{"Category",0},{"CancelGeneration",stable.CancelGeneration},{"RuntimeGeneration",13},{"RequestId",stable.ActiveRequestId},
        {"Rows",json::array({{{"Path","/Game/New"}}})},{"HasMore",false}},121);
    assert(!stable.Refreshing&&stable.Rows.size()==1&&stable.Rows[0]["Path"]=="/Game/New");
    assert(Search(0,"different")&&!stable.Refreshing&&stable.Rows.empty()); // No stale rows across queries.
    InvalidateSources(false);resetFixture();auto& paged=For(0);paged.RuntimeGeneration=13;paged.Started=true;paged.Query="red dye";paged.HasMore=false;paged.FillTarget=120;
    for(int i=0;i<120;++i)paged.Rows.push_back({{"Path","/Mod/Old"+std::to_string(i)}});
    assert(Retry(0)&&paged.Refreshing&&paged.FillTarget>=120&&paged.RefreshRequired==120);
    for(int start=0;start<120;start+=24){
        json replacement=json::array();for(int i=start;i<start+24;++i)replacement.push_back({{"Path","/Mod/New"+std::to_string(i)}});
        ApplyWindow({{"Category",0},{"CancelGeneration",paged.CancelGeneration},{"RuntimeGeneration",13},{"RequestId",paged.ActiveRequestId},
            {"Reset",start==0},{"Rows",replacement},{"NextCursor","P:refresh:"+std::to_string(start+24)},{"HasMore",true},{"AlphabeticalReady",true}},200+start);
        assert(paged.Rows.size()==120); // A page-three view never collapses to page one.
        if(start<96)assert(paged.Refreshing&&paged.Rows[80]["Path"]=="/Mod/Old80");
    }
    assert(!paged.Refreshing&&paged.RefreshRows.empty()&&paged.Rows[80]["Path"]=="/Mod/New80");
    assert(Retry(0));
    ApplyWindow({{"Category",0},{"CancelGeneration",paged.CancelGeneration},{"RuntimeGeneration",13},{"RequestId",paged.ActiveRequestId},
        {"Reset",true},{"Rows",json::array({{{"Path","/Mod/OnlyRemaining"}}})},{"HasMore",false},{"AlphabeticalReady",true}},340);
    assert(!paged.Refreshing&&paged.Rows.size()==1&&paged.Rows[0]["Path"]=="/Mod/OnlyRemaining"); // Real deletions still apply.
    assert(Search(0,"another query")&&paged.RefreshRows.empty()&&paged.Rows.empty());
    // Exercise the real native wrapper's progress forwarding: asynchronous
    // cache waits legitimately retain the same cursor for multiple replies.
    assert(Search(0,"Health"));
    for(int i=0;i<8;++i){auto& search=For(0);
        json response={{"Category",0},{"CancelGeneration",search.CancelGeneration},{"RuntimeGeneration",13},{"RequestId",search.ActiveRequestId},
            {"Rows",json::array()},{"NextCursor",""},{"HasMore",true},{"AlphabeticalReady",false}};
        PS::CatalogWindowProgress::Forward(response,{{"PopulationStage","filtering-cache"},{"PopulationDone",4096},{"PopulationTotal",38984},{"CacheHit",true},{"CheckingChanges",true}});
        ApplyWindow(response,130+i);
        assert(search.LastError.empty()&&search.StalledWindows==0&&!search.Pending&&search.PreparingAlphabetical);
        assert(search.PopulationStage=="filtering-cache"&&search.PopulationDone==4096&&search.PopulationTotal==38984&&search.CacheHit&&search.CheckingChanges);
        search.ResumeAt={};assert(RequestMore(0));
    }
    auto& health=For(0);
    ApplyWindow({{"Category",0},{"CancelGeneration",health.CancelGeneration},{"RuntimeGeneration",13},{"RequestId",health.ActiveRequestId},
        {"Rows",json::array({{{"Path","/Game/TinctureHealth"},{"Name","Health Tincture"}}})},{"HasMore",false},{"AlphabeticalReady",true},{"PopulationStage","cached-items"}},140);
    assert(health.Rows.size()==1&&!health.Pending&&!health.PreparingAlphabetical&&health.LastError.empty());
    const auto beforeCachedHealth=accepted;assert(Search(0,"Health")&&accepted==beforeCachedHealth&&!health.Pending);
    assert(Search(0,"uncached"));const auto cancelId=health.CancelGeneration,requestId=health.ActiveRequestId;
    CancelLoading(0);assert(!health.Pending&&!health.PreparingAlphabetical&&!health.LastError.empty());
    ApplyWindow({{"Category",0},{"CancelGeneration",cancelId},{"RuntimeGeneration",13},{"RequestId",requestId},
        {"Rows",json::array({{{"Path","/Game/Late"}}})},{"HasMore",false}},141);
    assert(health.Rows.empty()&&!health.LastError.empty());
    assert(Search(0,"preview"));
    ApplyWindow({{"Category",0},{"CancelGeneration",health.CancelGeneration},{"RuntimeGeneration",13},{"RequestId",health.ActiveRequestId},
        {"Rows",json::array({{{"Path","/Mod/Cached"},{"Name","Cached"},{"SourceName","cached-mod"}}})},{"HasMore",false},{"AlphabeticalReady",true},{"CachePreview",true}},142);
    assert(health.CachePreview&&health.Rows.size()==1);
    assert(Search(0,"preview")&&health.CachePreview&&health.Rows[0]["SourceName"]=="cached-mod");
    InvalidateSources(false);
    assert(health.CachePreview&&health.Refreshing&&health.Pending&&health.Query=="preview"&&health.Rows.size()==1);
    assert(Retry(0)&&health.Refreshing&&health.Rows.size()==1);
    ApplyWindow({{"Category",0},{"CancelGeneration",health.CancelGeneration},{"RuntimeGeneration",13},{"RequestId",health.ActiveRequestId},
        {"Reset",true},{"Rows",json::array()},{"HasMore",true},{"AlphabeticalReady",false},{"PopulationStage","discovering-items"}},143);
    assert(health.Rows.size()==1&&health.Rows[0]["SourceName"]=="cached-mod"&&health.Refreshing);
    const auto oldSourceFence=health.CancelGeneration,oldSourceRequest=health.ActiveRequestId;
    health.CachePreview=false;InvalidateSources(true); // Updater removes/replaces a file, not the mounted object.
    assert(health.Rows.size()==1&&health.Query=="preview"&&health.Pending&&health.Refreshing);
    json visible;Overlay(visible,0);assert(visible["Items"]==health.Rows);
    ApplyWindow({{"Category",0},{"CancelGeneration",oldSourceFence},{"RuntimeGeneration",13},{"RequestId",oldSourceRequest},
        {"Rows",json::array()},{"HasMore",false}},144);assert(health.Rows.size()==1&&health.Pending);
    ApplyWindow({{"Category",0},{"CancelGeneration",health.CancelGeneration},{"RuntimeGeneration",13},{"RequestId",health.ActiveRequestId},
        {"Reset",true},{"Rows",json::array({{{"Path","/Mod/Updated"}}})},{"HasMore",false},{"AlphabeticalReady",true}},145);
    assert(health.Rows.size()==1&&health.Rows[0]["Path"]=="/Mod/Updated"&&!health.Refreshing);
    assert(Search(0,"new source search")); // No permanent source-removal latch.
    ConfigureRemote([](json){return true;},[](int,uint64_t){});
    auto& actors=For(1);actors=State{};actors.RuntimeGeneration=13;
    assert(Search(1,"old actors"));
    ApplyWindow({{"Category",1},{"CancelGeneration",actors.CancelGeneration},{"RuntimeGeneration",13},{"RequestId",actors.ActiveRequestId},
        {"Rows",json::array({{{"Key","A"},{"Type","NPC"}}})},{"HasMore",false}},500);
    json actorSnapshot;Overlay(actorSnapshot,1);const auto oldRowsRevision=actorSnapshot.at("_CatalogRowsRevision");
    assert(Search(1,"empty actors"));Overlay(actorSnapshot,1);
    assert(actorSnapshot.at("Definitions").empty()&&actorSnapshot.at("_CatalogRowsRevision")!=oldRowsRevision);
    const auto emptyRowsRevision=actorSnapshot.at("_CatalogRowsRevision");
    ApplyWindow({{"Category",1},{"CancelGeneration",actors.CancelGeneration},{"RuntimeGeneration",13},{"RequestId",actors.ActiveRequestId},
        {"Rows",json::array({{{"Key","B"},{"Type","NPC"}}})},{"HasMore",false}},500);
    Overlay(actorSnapshot,1);assert(actorSnapshot.at("_CatalogRowsRevision")!=emptyRowsRevision);
    assert(Search(1,"old actors"));Overlay(actorSnapshot,1);
    assert(actorSnapshot.at("Definitions")[0]["Key"]=="A"&&actorSnapshot.at("_CatalogRowsRevision")!=oldRowsRevision);
    ClearRemote();
}
