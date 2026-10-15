#pragma once
#include "Generator/ToolRequest.h"
#include <algorithm>
#include <atomic>
#include <array>
#include <cstdint>
#include <chrono>
#include <functional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <nlohmann/json.hpp>

namespace PS::LazyCatalog {
using json=nlohmann::json;
struct CachedWindow {json Rows=json::array();std::string NextCursor;bool HasMore=true;uint64_t Generation=0;};
struct State {
    json Rows=json::array();
    std::string Query,NextCursor,LastError,Coverage;
    bool HasMore=true,Pending=false,Started=false;
    uint64_t AppliedRevision=0,CancelGeneration=1,ActiveRequestId=0,RuntimeGeneration=0,LastResponseId=0;
    int FillTarget=0,StalledWindows=0;
    std::chrono::steady_clock::time_point RequestedAt{};
    std::unordered_map<std::string,CachedWindow> Cache;
    std::unordered_set<std::string> RandomSeen;
};
inline std::array<State,3> Windows{};
inline bool SourcesRemoved=false;
inline uint64_t FirstOpenGeneration=0,FirstOpenPendingGeneration=0,NextRequestId=(uint64_t{1}<<62);
// The game-thread push callback may only publish this scalar. Catalogue state
// and JSON windows belong exclusively to the UI thread.
inline std::atomic<uint64_t> BackendGeneration{0};
inline constexpr int InitialRows=28;
inline constexpr int MoreRows=24;
inline constexpr int MaxCachedQueries=8;
inline std::function<bool(json)> RemoteSubmit;
inline std::function<void(int,uint64_t)> RemoteCancel;
inline void ConfigureRemote(std::function<bool(json)> submit,std::function<void(int,uint64_t)> cancel){RemoteSubmit=std::move(submit);RemoteCancel=std::move(cancel);}
inline void ClearRemote(){RemoteSubmit={};RemoteCancel={};}

inline State& For(int category){return Windows[static_cast<size_t>(std::clamp(category,0,2))];}
inline const char* KeyFor(int category){return category==0?"Items":"Definitions";}
inline std::string RandomIdentity(const json& row) {
    if(!row.is_object())return {};
    const auto path=row.value("Path",std::string{});if(!path.empty())return "P:"+path;
    const auto persistence=row.value("PersistenceID",std::string{});if(!persistence.empty())return "I:"+persistence;
    const auto internal=row.value("InternalName",std::string{});if(!internal.empty())return "N:"+internal;
    return {};
}
inline void RebuildRandomSeen(State& state) {
    state.RandomSeen.clear();
    if(!state.Rows.is_array())return;
    for(const auto& row:state.Rows){const auto identity=RandomIdentity(row);if(!identity.empty())state.RandomSeen.insert(identity);}
}
inline std::string CacheKey(const State& state,int category){return category==0&&state.Query.empty()?"@random":state.Query;}
inline void SaveCache(int category) {
    auto& state=For(category);if(!state.Started||state.Rows.empty()||!state.RuntimeGeneration)return;
    if(state.Cache.size()>=MaxCachedQueries&&!state.Cache.contains(CacheKey(state,category)))state.Cache.erase(state.Cache.begin());
    state.Cache[CacheKey(state,category)]={state.Rows,state.NextCursor,state.HasMore,state.RuntimeGeneration};
}
inline bool RestoreCache(int category,const std::string& query,uint64_t generation) {
    if(SourcesRemoved)return false;
    auto& state=For(category);const auto key=category==0&&query.empty()?std::string("@random"):query;
    const auto found=state.Cache.find(key);if(found==state.Cache.end()||found->second.Generation!=generation)return false;
    state.Query=query;state.Rows=found->second.Rows;state.NextCursor=found->second.NextCursor;state.HasMore=found->second.HasMore;
    if(category==0&&query.empty())RebuildRandomSeen(state);else state.RandomSeen.clear();
    state.Started=true;state.Pending=false;state.LastError.clear();state.StalledWindows=0;state.RuntimeGeneration=generation;state.FillTarget=static_cast<int>(state.Rows.size());return true;
}
inline bool SubmitRequest(json request) {
    if(RemoteSubmit)return RemoteSubmit(std::move(request));
    request["Action"]="CatalogWindow";return SpawnToolRequests::TrySubmit(std::move(request));
}
inline bool Request(int category,const char* mode,const std::string& query,bool reset,int limit,const std::string& cursor={},int priority=2) {
    auto& state=For(category);if(SourcesRemoved){state.LastError="Mod sources changed. Reload the world after RuneSchema reloads its content to refresh search.";return false;}if(state.Pending)return false;
    const auto generation=SpawnToolRequests::CurrentGeneration();
    const auto requestId=++NextRequestId;
    json request={{"Operation","CatalogWindow"},{"_Source","HelpyStandalone"},{"RequestId",requestId},{"Category",category},{"Mode",mode},
        {"Query",query},{"Limit",limit},{"Reset",reset},{"FieldSet","BasicRow"},{"Priority",priority},
        {"ClientGeneration",generation},{"CancelGeneration",state.CancelGeneration}};
    if(!cursor.empty())request["Cursor"]=cursor;
    if(std::string_view(mode)=="Random") {std::random_device rd;request["Seed"]=(static_cast<uint64_t>(rd())<<32)^rd();}
    if(!SubmitRequest(request)){state.LastError="The runtime did not accept this request. Retry when the world is ready.";return false;}
    state.Started=true;state.Pending=true;state.ActiveRequestId=requestId;state.RuntimeGeneration=generation;
    state.RequestedAt=std::chrono::steady_clock::now();
    if(reset){state.Rows=json::array();state.RandomSeen.clear();state.NextCursor.clear();state.LastError.clear();state.StalledWindows=0;state.HasMore=true;state.Query=query;state.FillTarget=limit;}
    return true;
}
inline bool Search(int category,const std::string& query) {
    auto& state=For(category);const auto generation=SpawnToolRequests::CurrentGeneration();
    SaveCache(category);
    ++state.CancelGeneration;if(RemoteCancel)RemoteCancel(category,state.CancelGeneration);
    state.Pending=false;state.ActiveRequestId=0;
    if(RestoreCache(category,query,generation))return true;
    state.Query=query;return Request(category,"Search",query,true,InitialRows,{},1);
}
inline bool Retry(int category) {
    auto& state=For(category);const auto query=state.Query;
    ++state.CancelGeneration;if(RemoteCancel)RemoteCancel(category,state.CancelGeneration);
    state.Pending=false;state.ActiveRequestId=0;
    return Request(category,category==0&&query.empty()?"Random":"Search",query,true,InitialRows,{},1);
}
inline void CheckTimeouts(std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now()) {
    for(auto& state:Windows)if(state.Pending&&now-state.RequestedAt>std::chrono::seconds(15)) {
        state.Pending=false;state.ActiveRequestId=0;
        ++state.CancelGeneration;
        if(RemoteCancel)RemoteCancel(static_cast<int>(&state-Windows.data()),state.CancelGeneration);
        state.LastError="The item request timed out. Retry to load this list again.";
    }
}
inline bool RequestMore(int category) {
    auto& state=For(category);const char* mode=(category==0&&state.Query.empty())?"Random":"Search";
    if(!state.Started||!state.HasMore||state.Pending||!state.LastError.empty())return false;
    return Request(category,mode,state.Query,false,MoreRows,state.NextCursor,3);
}
// Fill a finite visible page without moving its target on each response.
inline bool FillTo(int category,int count) {
    auto& state=For(category);state.FillTarget=std::max(state.FillTarget,std::clamp(count,0,100000));
    if(static_cast<int>(state.Rows.size())>=state.FillTarget)return true;
    return RequestMore(category);
}
inline bool More(int category){return FillTo(category,static_cast<int>(For(category).Rows.size())+MoreRows);}
inline void FirstOpen(uint64_t generation) {
    if(!generation||FirstOpenGeneration==generation)return;
    FirstOpenGeneration=generation;FirstOpenPendingGeneration=0;
    auto& state=For(0);if(state.RuntimeGeneration!=generation){SaveCache(0);const auto cancel=state.CancelGeneration;state=State{};state.CancelGeneration=cancel;}state.RuntimeGeneration=generation;
    if(RestoreCache(0,{},generation))return;
    if(!Request(0,"Random",{},true,InitialRows,{},2)){FirstOpenPendingGeneration=generation;state.LastError="Waiting for the runtime to accept the initial browse request.";}
}
inline void BackendSignal(uint64_t generation){BackendGeneration.store(generation,std::memory_order_release);}
inline void PumpBackendSignal(){
    const auto generation=BackendGeneration.exchange(0,std::memory_order_acquire);
    if(!generation||FirstOpenPendingGeneration!=generation)return;
    auto& state=For(0);
    if(state.Pending||state.Started){FirstOpenPendingGeneration=0;return;}
    if(Request(0,"Random",{},true,InitialRows,{},2)){FirstOpenPendingGeneration=0;state.LastError.clear();}
}
inline void ApplyWindow(const json& window,uint64_t revision) {
    if(SourcesRemoved)return;
    const int category=window.value("Category",-1);if(category<0||category>2)return;auto& state=For(category);
    const auto cancel=window.value("CancelGeneration",uint64_t{});if(cancel!=state.CancelGeneration)return;
    const auto generation=window.value("RuntimeGeneration",state.RuntimeGeneration);if(state.RuntimeGeneration&&generation!=state.RuntimeGeneration)return;
    const auto requestId=window.value("RequestId",uint64_t{});if(state.ActiveRequestId&&requestId&&requestId!=state.ActiveRequestId)return;
    if(requestId&&requestId==state.LastResponseId)return;
    state.LastResponseId=requestId;
    state.AppliedRevision=revision;state.RuntimeGeneration=generation;
    const bool reset=window.value("Reset",false);const bool randomItems=category==0&&window.value("Mode",std::string{})=="Random";
    if(reset){state.Rows=json::array();state.RandomSeen.clear();state.StalledWindows=0;}
    const auto before=state.Rows.size();const auto previousCursor=state.NextCursor;
    if(window.contains("Rows")&&window["Rows"].is_array())for(const auto& row:window["Rows"]) {
        if(!row.is_object())continue;
        if(randomItems){const auto identity=RandomIdentity(row);if(!identity.empty()&&!state.RandomSeen.insert(identity).second)continue;}
        state.Rows.push_back(row);
    }
    state.NextCursor=window.value("NextCursor",std::string{});state.HasMore=window.value("HasMore",false);state.Pending=false;state.ActiveRequestId=0;state.LastError=window.value("Error",std::string{});
    state.Coverage=window.value("Coverage",std::string{});
    // Empty slices are normal while the host scans non-item UObjects. Only a
    // repeated cursor indicates a stall; an advancing scan must keep filling.
    state.StalledWindows=!reset&&state.HasMore&&state.Rows.size()==before&&state.NextCursor==previousCursor?state.StalledWindows+1:0;
    if(state.StalledWindows>=3&&state.LastError.empty())state.LastError="No additional items returned. Refresh the current list to retry.";
    SaveCache(category);
    if(state.Started&&state.HasMore&&state.LastError.empty()&&static_cast<int>(state.Rows.size())<state.FillTarget)RequestMore(category);
}
inline void Apply(const json& snapshot,uint64_t revision) {
    if(snapshot.contains("_QueryResults")&&snapshot["_QueryResults"].is_array())for(const auto& result:snapshot["_QueryResults"]) {
        if(result.contains("CatalogWindow")&&result["CatalogWindow"].is_object())ApplyWindow(result["CatalogWindow"],revision);
        else for(auto& state:Windows)if(state.Pending){state.Pending=false;state.ActiveRequestId=0;state.LastError=result.value("Status",std::string("The runtime returned an incomplete item response. Retry loading items."));}
    }
    if(snapshot.contains("CatalogWindow")&&snapshot["CatalogWindow"].is_object())ApplyWindow(snapshot["CatalogWindow"],revision);
}
inline void ReleaseFailedWindow(uint64_t revision,const json& snapshot) {
    if(snapshot.contains("_QueryResults")||snapshot.contains("CatalogWindow"))return;
    (void)revision;
}
inline void Overlay(json& snapshot,int category){if(category>=0&&category<=2){auto& state=For(category);const auto* key=KeyFor(category);
    // The original Helpy host can already have a populated live catalogue.
    // Empty asynchronous slices must not replace those real rows with nothing.
    if(SourcesRemoved){snapshot[key]=json::array();return;}
    if(state.Query.empty()&&state.Rows.empty()&&snapshot.contains(key)&&snapshot[key].is_array()&&!snapshot[key].empty())return;
    if(state.Started||!state.Rows.empty())snapshot[key]=state.Rows;
}}
inline void InvalidateSources(bool removed){SourcesRemoved=removed;for(size_t i=0;i<Windows.size();++i){auto& state=Windows[i];const auto cancel=state.CancelGeneration+1;if(RemoteCancel)RemoteCancel(static_cast<int>(i),cancel);state=State{};state.CancelGeneration=cancel;}FirstOpenGeneration=0;}
inline void ClearPending(){for(auto& state:Windows){state.Pending=false;state.ActiveRequestId=0;}}
inline void ResetForGeneration(uint64_t generation){for(auto& state:Windows){if(state.RuntimeGeneration!=generation){const auto cancel=state.CancelGeneration;state=State{};state.CancelGeneration=cancel;state.RuntimeGeneration=generation;}}}
}
