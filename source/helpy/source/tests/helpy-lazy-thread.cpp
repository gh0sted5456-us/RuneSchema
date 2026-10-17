#include "Generator/LazyCatalog.h"
#include <cassert>
#include <thread>
int main(){
    using namespace PS::LazyCatalog;
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
    ApplyWindow({{"Category",0},{"CancelGeneration",oldCancel},{"Rows",json::array({{{"Path","stale"}}})}},100);
    assert(For(0).Pending&&For(0).Rows.empty());
    For(1)=State{};json fallback={{"Definitions",json::array({{{"Key","native"}}})}};Overlay(fallback,1);assert(fallback["Definitions"].size()==1);
    For(0)=State{};For(0).Started=true;For(0).Pending=true;
    json live={{"Items",json::array({{{"Path","/Game/Real.Item"},{"Icon","/Game/Icons/Real.Real"}}})}};
    Overlay(live,0);assert(live["Items"].size()==1&&live["Items"][0]["Icon"]=="/Game/Icons/Real.Real");
    InvalidateSources(true);Overlay(live,0);assert(live["Items"].empty());assert(!Search(0,"removed item")&&For(0).Rows.empty());
    InvalidateSources(false);assert(Search(0,"fresh"));const auto cancelled=For(0).CancelGeneration;assert(Search(0,"new"));
    ApplyWindow({{"Category",0},{"CancelGeneration",cancelled},{"Rows",json::array({{{"Path","stale-source"}}})}},101);assert(For(0).Rows.empty());
    // Source inspection may complete after the UI's first browse. Its cancel
    // fence must survive FirstOpen and world-generation resets on either lane.
    uint64_t fence=0;unsigned accepted=0;
    ConfigureRemote([&](json request){const auto cancel=request.at("CancelGeneration").get<uint64_t>();if(cancel<fence)return false;++accepted;return true;},
        [&](int category,uint64_t cancel){if(category==0)fence=std::max(fence,cancel);});
    InvalidateSources(false);assert(fence>1);
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
    InvalidateSources(false);auto& cached=For(0);cached.Started=true;cached.RuntimeGeneration=13;cached.HasMore=false;cached.Coverage="cached";
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
    InvalidateSources(true);assert(!RestoreCache(0,"query0",13));
    ClearRemote();
}
