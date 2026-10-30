#pragma once
#include "Runtime/PluginRuntimeServices.h"
#include "Generator/LazyCatalog.h"
#include "Core/CacheBundleItems.h"
#include "Runtime/HostServices.h"
#include "Core/BoundedLru.h"
#include <optional>
#include <deque>

// UI requests wait only for this file worker. The native catalogue refreshes
// independently and cannot hold up cached presentation or page navigation.
namespace PS::HelpyCachedCatalog {
inline CacheBundleItems::Store Files;
inline std::optional<nlohmann::json> Pending;
inline std::deque<uint64_t> PassiveRequests;
inline std::chrono::steady_clock::time_point NextPassive{};
struct LiveCheck {nlohmann::json Request;std::string Key,Cursor="F:0";uint64_t Id=0;unsigned Slices=0,Found=0;bool Waiting=false;std::chrono::steady_clock::time_point Next{},Deadline{},ReplyDeadline{};};
inline std::optional<LiveCheck> Check;
inline BoundedLru<std::string,std::chrono::steady_clock::time_point> Checked{64};
inline BoundedLru<std::string,std::string> Resume{64};
inline std::string CheckStatus;
inline void StopCheck(const std::string& message){CheckStatus=message;LazyCatalog::For(0).CheckingChanges=false;Check.reset();}
inline void ResetChecks(){Check.reset();Checked.Clear();Resume.Clear();CheckStatus.clear();}
inline bool QueueCheck(nlohmann::json request,bool force=false){
    const auto query=request.value("Query",std::string{});if(query.empty()||!LazyCatalog::RemoteSubmit||(PluginRuntimeServices::ExternalDesktop&&!PluginRuntimeServices::GameProcessId.load()))return false;
    const ItemCatalogFilter::Query filter(query);if(!force&&filter.Words.empty()&&filter.Family.empty())return false;
    const auto now=std::chrono::steady_clock::now();
    const auto key=nlohmann::json::array({HelpySourceCache::Current.CatalogKey,HelpySourceCache::Current.Versions,HelpySourceCache::Current.Authoritative,HelpySourceCache::Current.Provisional,request.value("ClientGeneration",uint64_t{}),PluginRuntimeServices::GameProcessId.load(),query}).dump();
    if(!force)if(const auto* previous=Checked.Find(key);previous&&now-*previous<std::chrono::seconds(60))return false;
    Checked.Put(key,now);Check=LiveCheck{std::move(request),key};Check->Deadline=now+std::chrono::seconds(10);
    if(const auto* cursor=Resume.Find(key);cursor&&cursor->starts_with("F:"))Check->Cursor=*cursor;
    LazyCatalog::For(0).CheckingChanges=true;CheckStatus="Checking mounted items in bounded slices; cached results remain available.";return true;
}
inline bool RefreshSource(const std::string& provider){
    if(!provider.starts_with("mods/"))return false;
    const auto& state=LazyCatalog::For(0);
    return QueueCheck({{"Operation","CatalogWindow"},{"Category",0},{"Mode","Search"},{"Query",ItemSource::Query(ItemSource::Kind::Modded,"All","",provider)},{"ClientGeneration",state.RuntimeGeneration},{"CancelGeneration",state.CancelGeneration}},true);
}
inline bool ReceiveCheck(const nlohmann::json& window){
    if(!Check||!Check->Id||window.value("RequestId",uint64_t{})!=Check->Id)return false;
    auto& job=*Check;job.Waiting=false;job.Next=std::chrono::steady_clock::now()+std::chrono::milliseconds(200);
    auto& state=LazyCatalog::For(0);bool changed=false;
    if(window.value("RuntimeGeneration",uint64_t{})==state.RuntimeGeneration&&window.contains("Rows")&&window["Rows"].is_array()){
        const ItemCatalogFilter::Query match(state.Query);
        for(const auto& row:window["Rows"]){if(!row.is_object())continue;++job.Found;if(!match(row))continue;const auto path=row.value("Path",std::string{});if(path.empty())continue;
            auto found=std::find_if(state.Rows.begin(),state.Rows.end(),[&](const auto& current){return current.value("Path",std::string{})==path;});
            if(found==state.Rows.end()){if(state.Rows.size()<4096){state.Rows.push_back(row);changed=true;}}
            else if(row.value("Loaded",true)){auto merged=*found;merged.update(row);if(merged!=*found){*found=std::move(merged);changed=true;}}
        }
    }
    if(changed){state.RowsRevision=++LazyCatalog::RowsClock;LazyCatalog::SaveCache(0);}
    if(!window.value("Error",std::string{}).empty()){StopCheck("Live check unavailable; existing cached results retained.");return true;}
    job.Cursor=window.value("NextCursor",std::string{});
    if(!window.value("HasMore",false)||!job.Cursor.starts_with("F:")){Resume.Put(job.Key,{});Files.Invalidate();StopCheck("Mounted item check complete; new discoveries queued for cache.");}
    return true;
}
inline void PumpCheck(){
    if(!Check)return;const auto now=std::chrono::steady_clock::now();auto& job=*Check;
    if(job.Request.value("ClientGeneration",uint64_t{})!=LazyCatalog::For(0).RuntimeGeneration||job.Request.value("CancelGeneration",uint64_t{})!=LazyCatalog::For(0).CancelGeneration){StopCheck("Live session or search changed; existing cache retained.");return;}
    if(now>=job.Deadline||job.Slices>=32){Resume.Put(job.Key,job.Cursor);Files.Invalidate();StopCheck("Bounded check finished; discoveries retained. Refresh again to continue.");return;}
    if(job.Waiting){if(now>=job.ReplyDeadline)StopCheck("Live check timed out; cached results remain available.");return;}
    if(now<job.Next)return;
    auto request=job.Request;job.Id=++LazyCatalog::NextRequestId;request["RequestId"]=job.Id;request["Cursor"]=job.Cursor;request["Mode"]="Search";request["Limit"]=64;request["Priority"]=2;request["FieldSet"]="BasicRow";request["Reset"]=false;
    PassiveRequests.push_back(job.Id);while(PassiveRequests.size()>64)PassiveRequests.pop_front();job.Waiting=true;job.ReplyDeadline=now+std::chrono::seconds(3);++job.Slices;
    if(!LazyCatalog::RemoteSubmit||!LazyCatalog::RemoteSubmit(std::move(request)))StopCheck("Live check is not ready; existing cached results retained.");
}
inline bool Submit(const nlohmann::json& request){
    const bool offline=PluginRuntimeServices::ExternalDesktop&&!PluginRuntimeServices::GameProcessId.load();
    if(request.value("Cursor",std::string{}).starts_with("S:")||request.value("Category",-1)!=0||request.value("Mode",std::string{})=="Random"||(!offline&&!HelpyRuntimePreferences::CacheFirst.load()&&HelpySourceCache::Ready()))return false;
    Pending=request;return true;
}
inline void Pump(){
    using namespace LazyCatalog;
    auto& state=For(0);
    if(Pending){
        const auto request=*Pending;
        if(!state.Pending||state.ActiveRequestId!=request.value("RequestId",uint64_t{})){Pending.reset();return;}
        auto page=Files.Lookup(HelpySourceCache::Directory(false),HostServices::ModDirectory(),HelpySourceCache::Current,CacheBundleStore::Locale(),request.value("Query",std::string{}),request.value("Cursor",std::string{}),request.value("Limit",size_t(28)));
        // Exhausting a saved catalogue with no matches is a live-search miss,
        // including when filtering required several local continuation pages.
        if(page&&page->at("Rows").empty()&&!page->value("HasMore",true)&&!request.value("Query",std::string{}).empty()
            &&(state.Refreshing?state.RefreshRows:state.Rows).empty())page.reset();
        if(page){
            Pending.reset();(*page)["Category"]=0;(*page)["Mode"]=request.at("Mode");(*page)["RequestId"]=request.at("RequestId");(*page)["CancelGeneration"]=request.at("CancelGeneration");(*page)["RuntimeGeneration"]=request.at("ClientGeneration");
            (*page)["Reset"]=request.value("Reset",false)||page->value("Reset",false);(*page)["PopulationStage"]="cached-items";(*page)["CheckingChanges"]=HelpyRuntimePreferences::PassiveRefresh.load()&&(!PluginRuntimeServices::ExternalDesktop||PluginRuntimeServices::GameProcessId.load()!=0);
            ApplyWindow(*page,state.AppliedRevision+1);
            if(request.value("Cursor",std::string{}).empty()&&!page->at("Rows").empty())QueueCheck(request);
        }else if(!Files.Awaiting()){
            Pending.reset();auto native=request;
            if(!request.value("Query",std::string{}).empty()){native["Mode"]="Search";native.erase("Cursor");}
            bool accepted=false;if(RemoteSubmit)accepted=RemoteSubmit(native);else{native["Action"]="CatalogWindow";accepted=SpawnToolRequests::TrySubmit(std::move(native));}
            if(!accepted){state.Pending=false;state.ActiveRequestId=0;state.LastError="No cached items are available yet. Download a cache or retry when the game is ready.";}
        }
    }
    PumpCheck();
    const auto now=std::chrono::steady_clock::now();
    if(now<NextPassive||!state.Started||state.Pending||!RemoteSubmit||!HelpyRuntimePreferences::PassiveRefresh.load()||!HelpySourceCache::Ready()||(PluginRuntimeServices::ExternalDesktop&&!PluginRuntimeServices::GameProcessId.load()))return;
    NextPassive=now+std::chrono::seconds(15);
    const auto id=++NextRequestId;PassiveRequests.push_back(id);if(PassiveRequests.size()>32)PassiveRequests.pop_front();
    const nlohmann::json request={{"Operation","CatalogWindow"},{"_Source","HelpyStandalone"},{"RequestId",id},{"Category",0},{"Mode","Alphabetical"},{"Query",""},{"Limit",1},{"Reset",true},{"FieldSet","BasicRow"},{"Priority",3},{"ClientGeneration",state.RuntimeGeneration},{"CancelGeneration",state.CancelGeneration},{"BrowsePolicy",{{"loadOrder","cacheFirst"},{"passiveRefresh",true}}}};
    RemoteSubmit(request);
}
inline void Configure(){
    LazyCatalog::LocalSubmit=Submit;LazyCatalog::LocalPoll=Pump;
    LazyCatalog::IgnoreResponse=[](const auto& result){if(ReceiveCheck(result))return true;const auto id=result.value("RequestId",uint64_t{});return std::find(PassiveRequests.begin(),PassiveRequests.end(),id)!=PassiveRequests.end();};
    LazyCatalog::LocalShutdown=[]{Pending.reset();Files.Shutdown();PassiveRequests.clear();NextPassive={};ResetChecks();};
}
}
