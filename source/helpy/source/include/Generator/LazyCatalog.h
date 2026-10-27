#pragma once
#include "Generator/ToolRequest.h"
#include "Generator/HelpyRuntimePreferences.h"
#include "Core/ItemSource.h"
#include "Core/ItemCategory.h"
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
struct CachedWindow {json Rows=json::array();std::string NextCursor;bool HasMore=true;uint64_t Generation=0,LastUsed=0;std::string Coverage;bool CachePreview=false,Stale=false;};
struct State {
    json Rows=json::array();
    json RefreshRows=json::array();
    size_t RefreshRequired=0;
    std::string Query,NextCursor,LastError,Coverage;
    std::string PopulationStage{"checking-cache"};
    uint64_t PopulationDone=0,PopulationTotal=0;
    bool CacheHit=false,CheckingChanges=false,Refreshing=false,CachePreview=false;
    bool HasMore=true,Pending=false,Started=false,PreparingAlphabetical=false;
    uint64_t AppliedRevision=0,CancelGeneration=1,ActiveRequestId=0,RuntimeGeneration=0,LastResponseId=0;
    uint64_t CacheClock=0,RowsRevision=0;
    int FillTarget=0,StalledWindows=0;
    std::chrono::steady_clock::time_point RequestedAt{};
    std::chrono::steady_clock::time_point ResumeAt{};
    std::unordered_map<std::string,CachedWindow> Cache;
    std::unordered_set<std::string> RandomSeen;
};
inline std::array<State,3> Windows{};
inline uint64_t RowsClock=0;
// UI-thread-only native icon confirmations. They never introduce items or
// survive a world/source change. Keep query caches from restoring old artwork.
inline std::unordered_map<std::string,std::string> NativeIcons;
inline void RecordNativeIcon(const std::string& path,const json& icon){
    if(path.empty()||path.size()>1024||path.front()!='/'||!icon.is_string()||icon.get_ref<const std::string&>().size()>2048)return;
    if(NativeIcons.size()>=512&&!NativeIcons.contains(path))NativeIcons.erase(NativeIcons.begin());
    NativeIcons[path]=icon.get<std::string>();
    const auto update=[&](json& rows){if(rows.is_array())for(auto& row:rows)if(row.is_object()&&row.value("Path",std::string{})==path)row["Icon"]=icon;};
    update(Windows[0].Rows);update(Windows[0].RefreshRows);for(auto& [query,cache]:Windows[0].Cache)update(cache.Rows);
}
inline uint64_t FirstOpenGeneration=0,FirstOpenPendingGeneration=0,NextRequestId=(uint64_t{1}<<62);
// The game-thread push callback may only publish this scalar. Catalogue state
// and JSON windows belong exclusively to the UI thread.
inline std::atomic<uint64_t> BackendGeneration{0};
inline constexpr int InitialRows=28;
inline constexpr int MoreRows=24;
inline constexpr int MaxCachedQueries=16;
inline constexpr size_t MaxCachedRows=8192,MaxRowsPerQuery=2048;
inline std::function<bool(json)> RemoteSubmit;
inline std::function<bool(const json&)> LocalSubmit,IgnoreResponse;
inline std::function<void()> LocalPoll,LocalShutdown;
inline std::function<void(int,uint64_t)> RemoteCancel;
inline void ConfigureRemote(std::function<bool(json)> submit,std::function<void(int,uint64_t)> cancel){RemoteSubmit=std::move(submit);RemoteCancel=std::move(cancel);}
inline void ClearRemote(){if(LocalShutdown)LocalShutdown();LocalSubmit={};LocalPoll={};LocalShutdown={};IgnoreResponse={};RemoteSubmit={};RemoteCancel={};}

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
inline const char* BrowseMode(int category){return category==0?"Alphabetical":"Search";}
inline std::string CacheKey(const State& state,int category){return std::to_string(HelpyRuntimePreferences::CacheFirst.load())+std::to_string(HelpyRuntimePreferences::PassiveRefresh.load())+":"+(category==0&&state.Query.empty()?"@alphabetical":state.Query);}
inline void SaveCache(int category) {
    auto& state=For(category);if(state.Refreshing||!state.Started||!state.RuntimeGeneration||!state.LastError.empty()
        ||(state.Rows.empty()&&(state.HasMore||state.Pending))||state.Rows.size()>MaxRowsPerQuery)return;
    const auto key=CacheKey(state,category);
    // Evict only the least recently used query, with both row and query caps.
    const auto retainedRows=[&]{size_t count=state.Rows.size();for(const auto& [name,window]:state.Cache)if(name!=key)count+=window.Rows.size();return count;};
    while((state.Cache.size()>=MaxCachedQueries&&!state.Cache.contains(key))||retainedRows()>MaxCachedRows){
        auto oldest=state.Cache.end();for(auto it=state.Cache.begin();it!=state.Cache.end();++it)if(it->first!=key&&(oldest==state.Cache.end()||it->second.LastUsed<oldest->second.LastUsed))oldest=it;
        if(oldest==state.Cache.end())break;state.Cache.erase(oldest);
    }
    state.Cache[key]={state.Rows,state.NextCursor,state.HasMore,state.RuntimeGeneration,++state.CacheClock,state.Coverage,state.CachePreview,state.CheckingChanges};
}
inline bool RestoreCache(int category,const std::string& query,uint64_t generation) {
    auto& state=For(category);const auto key=std::to_string(HelpyRuntimePreferences::CacheFirst.load())+std::to_string(HelpyRuntimePreferences::PassiveRefresh.load())+":"+(category==0&&query.empty()?std::string("@alphabetical"):query);
    const auto found=state.Cache.find(key);if(found==state.Cache.end()||found->second.Generation!=generation)return false;
    found->second.LastUsed=++state.CacheClock;
    state.Query=query;state.Rows=found->second.Rows;state.RowsRevision=++RowsClock;state.NextCursor=found->second.NextCursor;state.HasMore=found->second.HasMore;
    state.Coverage=found->second.Coverage;
    state.CachePreview=found->second.CachePreview;
    state.CheckingChanges=found->second.Stale;
    state.PreparingAlphabetical=false;state.Refreshing=false;state.RefreshRows=json::array();state.RefreshRequired=0;
    state.PopulationStage="cached-items";state.CacheHit=true;
    if(category==0&&query.empty())RebuildRandomSeen(state);else state.RandomSeen.clear();
    state.Started=true;state.Pending=false;state.LastError.clear();state.StalledWindows=0;state.RuntimeGeneration=generation;state.FillTarget=static_cast<int>(state.Rows.size());return true;
}
inline bool SubmitRequest(json request) {
    if(request.value("Category",-1)==0&&request.value("Cursor",std::string{}).starts_with("S:"))request["Mode"]="Search";
    if(LocalSubmit&&LocalSubmit(request))return true;
    if(RemoteSubmit)return RemoteSubmit(std::move(request));
    request["Action"]="CatalogWindow";return SpawnToolRequests::TrySubmit(std::move(request));
}
inline bool Request(int category,const char* mode,const std::string& query,bool reset,int limit,const std::string& cursor={},int priority=2) {
    auto& state=For(category);if(state.Pending)return false;
    const auto generation=SpawnToolRequests::CurrentGeneration();
    const auto requestId=++NextRequestId;
    json request={{"Operation","CatalogWindow"},{"_Source","HelpyStandalone"},{"RequestId",requestId},{"Category",category},{"Mode",mode},
        {"Query",query},{"Limit",limit},{"Reset",reset},{"FieldSet","BasicRow"},{"Priority",priority},
        {"ClientGeneration",generation},{"CancelGeneration",state.CancelGeneration}};
    request["BrowsePolicy"]={{"loadOrder",HelpyRuntimePreferences::CacheFirst.load()?"cacheFirst":"gameFirst"},{"passiveRefresh",HelpyRuntimePreferences::PassiveRefresh.load()}};
    if(!cursor.empty())request["Cursor"]=cursor;
    if(std::string_view(mode)=="Random") {std::random_device rd;request["Seed"]=(static_cast<uint64_t>(rd())<<32)^rd();}
    if(!SubmitRequest(request)){state.LastError="The runtime did not accept this request. Retry when the world is ready.";return false;}
    state.Started=true;state.Pending=true;state.ActiveRequestId=requestId;state.RuntimeGeneration=generation;
    state.RequestedAt=std::chrono::steady_clock::now();
    if(reset){state.Refreshing=state.Query==query&&!state.Rows.empty();state.RefreshRows=json::array();state.RefreshRequired=state.Refreshing?state.Rows.size():0;state.RandomSeen.clear();if(!state.Refreshing){state.Rows=json::array();state.RowsRevision=++RowsClock;}state.NextCursor.clear();state.LastError.clear();state.ResumeAt={};state.StalledWindows=0;state.HasMore=true;state.Query=query;state.FillTarget=state.Refreshing?std::max({state.FillTarget,limit,static_cast<int>(state.Rows.size())}):limit;state.PreparingAlphabetical=std::string_view(mode)=="Alphabetical";state.PopulationStage="checking-cache";state.PopulationDone=state.PopulationTotal=0;state.CacheHit=false;}
    return true;
}
inline bool Search(int category,const std::string& query) {
    auto& state=For(category);const auto generation=SpawnToolRequests::CurrentGeneration();
    SaveCache(category);
    ++state.CancelGeneration;if(RemoteCancel)RemoteCancel(category,state.CancelGeneration);
    state.Pending=false;state.ActiveRequestId=0;
    if(RestoreCache(category,query,generation)){
        if(state.CheckingChanges)Request(category,BrowseMode(category),query,true,InitialRows,{},3);
        return true; // Last-good results remain browsable even if refresh cannot submit.
    }
    return Request(category,BrowseMode(category),query,true,InitialRows,{},1);
}
inline bool Retry(int category) {
    auto& state=For(category);const auto query=state.Query;
    ++state.CancelGeneration;if(RemoteCancel)RemoteCancel(category,state.CancelGeneration);
    state.Pending=false;state.ActiveRequestId=0;
    return Request(category,BrowseMode(category),query,true,InitialRows,{},1);
}
inline void CancelLoading(int category){
    auto& state=For(category);++state.CancelGeneration;if(RemoteCancel)RemoteCancel(category,state.CancelGeneration);
    state.Pending=false;state.ActiveRequestId=0;state.Refreshing=false;state.RefreshRows=json::array();state.RefreshRequired=0;state.PreparingAlphabetical=false;
    state.LastError="Loading cancelled. Search again or retry this list.";
}
inline void CheckTimeouts(std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now()) {
    if(LocalPoll)LocalPoll();
    for(auto& state:Windows)if(state.Pending&&now-state.RequestedAt>std::chrono::seconds(15)) {
        state.Pending=false;state.ActiveRequestId=0;
        ++state.CancelGeneration;
        if(RemoteCancel)RemoteCancel(static_cast<int>(&state-Windows.data()),state.CancelGeneration);
        state.LastError="The item request timed out. Retry to load this list again.";
    }
}
inline bool RequestMore(int category) {
    auto& state=For(category);const char* mode=BrowseMode(category);
    if(!state.Started||!state.HasMore||state.Pending||!state.LastError.empty())return false;
    if(std::chrono::steady_clock::now()<state.ResumeAt)return false;
    return Request(category,mode,state.Query,false,MoreRows,state.NextCursor,3);
}
// Fill a finite visible page without moving its target on each response.
inline bool FillTo(int category,int count) {
    auto& state=For(category);state.FillTarget=std::max(state.FillTarget,std::clamp(count,0,100000));
    if(state.Refreshing)return RequestMore(category);
    if(static_cast<int>(state.Rows.size())>=state.FillTarget)return true;
    return RequestMore(category);
}
inline bool More(int category){return FillTo(category,static_cast<int>(For(category).Rows.size())+MoreRows);}
inline void FirstOpen(uint64_t generation) {
    if((!generation&&!LocalSubmit)||(FirstOpenGeneration==generation&&For(0).Started))return;
    FirstOpenGeneration=generation;FirstOpenPendingGeneration=0;
    auto& state=For(0);if(state.RuntimeGeneration!=generation){SaveCache(0);const auto cancel=state.CancelGeneration;state=State{};state.CancelGeneration=cancel;}state.RuntimeGeneration=generation;
    if(RestoreCache(0,{},generation))return;
    if(!Request(0,BrowseMode(0),{},true,InitialRows,{},2)){FirstOpenPendingGeneration=generation;state.LastError="Waiting for the runtime to accept the initial browse request.";}
}
inline void BackendSignal(uint64_t generation){BackendGeneration.store(generation,std::memory_order_release);}
inline void PumpBackendSignal(){
    const auto generation=BackendGeneration.exchange(0,std::memory_order_acquire);
    if(!generation||FirstOpenPendingGeneration!=generation)return;
    auto& state=For(0);
    if(state.Pending||state.Started){FirstOpenPendingGeneration=0;return;}
    if(Request(0,BrowseMode(0),{},true,InitialRows,{},2)){FirstOpenPendingGeneration=0;state.LastError.clear();}
}
inline void ApplyWindow(const json& window,uint64_t revision) {
    if(IgnoreResponse&&IgnoreResponse(window))return;
    const int category=window.value("Category",-1);if(category<0||category>2)return;auto& state=For(category);
    const auto cancel=window.value("CancelGeneration",uint64_t{});if(cancel!=state.CancelGeneration)return;
    const auto generation=window.value("RuntimeGeneration",state.RuntimeGeneration);if(state.RuntimeGeneration&&generation!=state.RuntimeGeneration)return;
    const auto requestId=window.value("RequestId",uint64_t{});if(state.ActiveRequestId&&requestId&&requestId!=state.ActiveRequestId)return;
    if(requestId&&requestId==state.LastResponseId)return;
    state.LastResponseId=requestId;
    state.AppliedRevision=revision;state.RuntimeGeneration=generation;
    const bool reset=window.value("Reset",false);const bool randomItems=category==0&&window.value("Mode",std::string{})=="Random";
    if(!state.Refreshing)state.RowsRevision=++RowsClock;
    state.PreparingAlphabetical=category==0&&window.contains("AlphabeticalReady")&&!window.value("AlphabeticalReady",false);
    state.PopulationStage=window.value("PopulationStage",state.PreparingAlphabetical?std::string("discovering-items"):std::string("items-ready"));
    state.PopulationDone=window.value("PopulationDone",window.value("IndexedItems",uint64_t{}));state.PopulationTotal=window.value("PopulationTotal",uint64_t{});
    state.CacheHit=window.value("CacheHit",false);state.CheckingChanges=window.value("CheckingChanges",false);
    state.CachePreview=window.value("CachePreview",false);
    // Revalidation restarts at page one. Stage its small response slices until
    // they cover every previously visible row, rather than briefly shrinking
    // a multi-page list to the initial 28 results and jumping the current page.
    if(reset){if(state.Refreshing)state.RefreshRows=json::array();else state.Rows=json::array();state.RandomSeen.clear();state.StalledWindows=0;}
    auto& destination=state.Refreshing?state.RefreshRows:state.Rows;
    const auto before=destination.size();const auto previousCursor=state.NextCursor;
    if(window.contains("Rows")&&window["Rows"].is_array())for(const auto& row:window["Rows"]) {
        if(!row.is_object())continue;
        if(randomItems){const auto identity=RandomIdentity(row);if(!identity.empty()&&!state.RandomSeen.insert(identity).second)continue;}
        auto current=row;if(category==0){ItemCategory::Repair(current);const auto icon=NativeIcons.find(row.value("Path",std::string{}));if(icon!=NativeIcons.end())current["Icon"]=icon->second;}
        destination.push_back(std::move(current));
    }
    const auto received=destination.size();
    if(state.Refreshing&&window.value("Error",std::string{}).empty()
        &&(state.RefreshRows.size()>=state.RefreshRequired||!window.value("HasMore",false))){
        if(state.Rows!=state.RefreshRows){state.Rows=std::move(state.RefreshRows);state.RowsRevision=++RowsClock;}
        state.RefreshRows=json::array();state.RefreshRequired=0;state.Refreshing=false;
    }
    state.NextCursor=window.value("NextCursor",std::string{});state.HasMore=window.value("HasMore",false);state.Pending=false;state.ActiveRequestId=0;state.LastError=window.value("Error",std::string{});
    const bool cacheWait=state.PreparingAlphabetical||state.PopulationStage=="checking-cache"||state.PopulationStage=="reading-catalogue";
    state.ResumeAt=cacheWait||(state.Rows.empty()&&state.NextCursor.starts_with("W:"))?std::chrono::steady_clock::now()+std::chrono::milliseconds(75):std::chrono::steady_clock::time_point{};
    state.Coverage=window.value("Coverage",std::string{});
    // Empty slices are normal while the host scans non-item UObjects. Only a
    // repeated cursor indicates a stall; an advancing scan must keep filling.
    state.StalledWindows=!cacheWait&&!reset&&state.HasMore&&received==before&&state.NextCursor==previousCursor?state.StalledWindows+1:0;
    if(state.StalledWindows>=3&&state.LastError.empty())state.LastError="No additional items returned. Refresh the current list to retry.";
    SaveCache(category);
    if(state.Started&&state.HasMore&&state.LastError.empty()&&(state.Refreshing||static_cast<int>(state.Rows.size())<state.FillTarget))RequestMore(category);
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
    if(state.Query.empty()&&state.Rows.empty()&&snapshot.contains(key)&&snapshot[key].is_array()&&!snapshot[key].empty()){snapshot.erase("_CatalogRowsRevision");return;}
    if(state.Started||!state.Rows.empty()){snapshot[key]=state.Rows;snapshot["_CatalogRowsRevision"]=state.RowsRevision;}
    else snapshot.erase("_CatalogRowsRevision");
}}
inline void InvalidateSources(bool removed){
    // A disk replacement/removal is not proof that an already mounted asset
    // vanished. Never latch the browser off or discard its last-good rows.
    (void)removed;NativeIcons.clear();
    for(size_t i=0;i<Windows.size();++i){auto& state=Windows[i];
        SaveCache(static_cast<int>(i));for(auto& [query,cache]:state.Cache)cache.Stale=true;
        ++state.CancelGeneration;if(RemoteCancel)RemoteCancel(static_cast<int>(i),state.CancelGeneration);
        state.Pending=false;state.ActiveRequestId=0;state.CheckingChanges=true;
        state.RefreshRows=json::array();state.RefreshRequired=0;state.Refreshing=false;
        if(state.Started||!state.Rows.empty())Request(static_cast<int>(i),BrowseMode(static_cast<int>(i)),state.Query,true,InitialRows,{},3);
    }
}
inline void ClearPending(){for(auto& state:Windows){state.Pending=false;state.ActiveRequestId=0;}}
inline void RefreshSourceMetadata(const HelpyCacheSources::Plan& plan,const std::set<std::string>& nativePlugins){
    auto& state=For(0);bool changed=false;
    const auto update=[&](json& rows){if(rows.is_array())for(auto& row:rows){
        if(!row.is_object()||row.value("RuntimeClone",false))continue;
        const auto path=row.find("Path");if(path==row.end()||!path->is_string())continue;
        const auto info=ItemSource::Reattribute(row,plan,nativePlugins);
        const auto fields=ItemSource::Fields(info);
        for(const auto& [key,value]:fields.items())if(!row.contains(key)||row[key]!=value){row[key]=value;changed=true;}
    }};
    update(state.Rows);update(state.RefreshRows);for(auto& [key,window]:state.Cache)update(window.Rows);
    if(!changed)return;
    // Metadata-only source discovery updates the existing browse in place;
    // only source-restricted result sets need another membership query.
    std::erase_if(state.Cache,[](const auto& pair){return pair.first.starts_with("__helpysource:");});
    ++state.AppliedRevision;
    if(state.Query.starts_with("__helpysource:"))Retry(0);
}
inline void ResetForGeneration(uint64_t generation){for(auto& state:Windows){if(state.RuntimeGeneration!=generation){NativeIcons.clear();const auto cancel=state.CancelGeneration;state=State{};state.CancelGeneration=cancel;state.RuntimeGeneration=generation;state.RowsRevision=++RowsClock;}}}
}
