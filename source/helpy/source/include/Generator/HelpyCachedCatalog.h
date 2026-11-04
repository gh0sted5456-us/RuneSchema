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
struct LiveCheck {nlohmann::json Request;std::string Key,Cursor="F:0";uint64_t Id=0;unsigned Slices=0,Found=0;bool Waiting=false,Interactive=false;std::chrono::steady_clock::time_point Next{},Deadline{},ReplyDeadline{};};
inline std::optional<LiveCheck> Check;
inline BoundedLru<std::string,std::chrono::steady_clock::time_point> Checked{64};
inline BoundedLru<std::string,std::string> Resume{64};
inline std::string CheckStatus;
inline bool Connected(){return LazyCatalog::RemoteSubmit&&(!PluginRuntimeServices::ExternalDesktop||PluginRuntimeServices::GameProcessId.load());}
inline std::string WarmKey,PresentedCacheKey,PreparedCacheKey;inline bool Warming=false;
inline std::chrono::steady_clock::time_point NextWarm{};
inline void WarmStartup(uint64_t cacheRevision=0){
    HelpySourceCache::PollSaved();const auto& snapshot=HelpySourceCache::Current;if(snapshot.Key.empty()||Pending)return;
    const auto key=snapshot.Key+"|"+snapshot.CatalogKey+"|"+std::to_string(HelpySourceCache::Revision)+"|"+std::to_string(cacheRevision);
    if(WarmKey!=key){WarmKey=key;Warming=true;}
    const auto now=std::chrono::steady_clock::now();if(!Warming&&now<NextWarm)return;
    const auto page=Files.Lookup(HelpySourceCache::Directory(false),HostServices::ModDirectory(),snapshot,CacheBundleStore::Locale(),{}, {},1,true);
    const auto prepared=Files.ContentKey();if(!prepared.empty()&&prepared!=PreparedCacheKey){PreparedCacheKey=prepared;LazyCatalog::For(0).Cache.clear();}
    if(page||!Files.Awaiting()){Warming=false;NextWarm=now+std::chrono::seconds(2);}
}
inline bool RefreshAvailable(){return (HelpyRuntimePreferences::CacheFirst.load()||!Connected())&&LazyCatalog::For(0).Started&&!LazyCatalog::For(0).Pending&&!Files.ContentKey().empty()&&Files.ContentKey()!=PresentedCacheKey;}
inline bool RefreshCached(){return RefreshAvailable()&&LazyCatalog::Retry(0);}
inline void StopCheck(const std::string& message){
    auto& state=LazyCatalog::For(0);CheckStatus=message;
    if(Check&&Check->Request.value("ClientGeneration",uint64_t{})==state.RuntimeGeneration&&Check->Request.value("CancelGeneration",uint64_t{})==state.CancelGeneration){
        state.CheckingChanges=false;if(Check->Interactive){state.Pending=false;state.ActiveRequestId=0;state.PopulationStage="items-ready";}
    }
    LazyCatalog::SaveCache(0);Check.reset();
}
inline void ResetChecks(){Check.reset();Checked.Clear();Resume.Clear();CheckStatus.clear();}
inline bool QueueCheck(nlohmann::json request,bool force=false){
    const auto query=request.value("Query",std::string{});if(query.empty()||!Connected())return false;
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
inline bool RefreshSearch(const std::string& query){
    if(!Connected()||query.empty())return false;
    const ItemCatalogFilter::Query filter(query);if(filter.Words.empty()&&filter.Family.empty())return false;
    auto& state=LazyCatalog::For(0);
    if(state.Query!=query)LazyCatalog::Search(0,query);
    ++state.CancelGeneration;LazyCatalog::QueueCancel(0,state.CancelGeneration);Pending.reset();
    if(!QueueCheck({{"Query",query},{"ClientGeneration",state.RuntimeGeneration},{"CancelGeneration",state.CancelGeneration}},true)){state.Pending=false;state.ActiveRequestId=0;state.CheckingChanges=false;return false;}
    Check->Interactive=true;state.Started=true;state.Pending=true;state.ActiveRequestId=0;state.RequestedAt=std::chrono::steady_clock::now();
    state.Refreshing=false;state.RefreshRows=nlohmann::json::array();state.RefreshRequired=0;state.PreparingAlphabetical=false;state.LastError.clear();
    state.PopulationStage="searching-live-items";state.PopulationDone=state.PopulationTotal=0;
    if(state.Rows.empty())state.HasMore=false;
    return true;
}
inline bool ReceiveCheck(const nlohmann::json& window){
    if(!Check||!Check->Id||window.value("RequestId",uint64_t{})!=Check->Id)return false;
    auto& job=*Check;job.Waiting=false;job.Next=std::chrono::steady_clock::now()+std::chrono::milliseconds(200);
    auto& state=LazyCatalog::For(0);bool changed=false;
    if(job.Request.value("ClientGeneration",uint64_t{})!=state.RuntimeGeneration||job.Request.value("CancelGeneration",uint64_t{})!=state.CancelGeneration){StopCheck("Live session or search changed; existing cache retained.");return true;}
    if(job.Interactive)state.PopulationDone=window.value("PopulationDone",state.PopulationDone);
    if(window.value("RuntimeGeneration",uint64_t{})==state.RuntimeGeneration&&window.contains("Rows")&&window["Rows"].is_array()){
        const ItemCatalogFilter::Query match(state.Query);
        for(const auto& row:window["Rows"]){if(!row.is_object())continue;++job.Found;if(!match(row))continue;const auto path=row.value("Path",std::string{});if(path.empty())continue;
            if(!job.Interactive)continue;
            const auto key=HelpyCacheSources::Lower(path);
            auto found=std::find_if(state.Rows.begin(),state.Rows.end(),[&](const auto& current){return HelpyCacheSources::Lower(current.value("Path",std::string{}))==key;});
            if(found==state.Rows.end()){if(state.Rows.size()<4096){state.Rows.push_back(row);changed=true;}}
            else if(row.value("Loaded",true)){auto merged=ItemName::Merge(*found,row);if(merged!=*found){*found=std::move(merged);changed=true;}}
        }
    }
    if(changed){state.RowsRevision=++LazyCatalog::RowsClock;LazyCatalog::SaveCache(0);}
    if(!window.value("Error",std::string{}).empty()){StopCheck("Live check unavailable; existing cached results retained.");return true;}
    job.Cursor=window.value("NextCursor",std::string{});
    if(!window.value("HasMore",false)||!job.Cursor.starts_with("F:")){Resume.Put(job.Key,{});StopCheck("Mounted item check complete; new discoveries queued for cache.");}
    return true;
}
inline void PumpCheck(){
    if(!Check)return;const auto now=std::chrono::steady_clock::now();auto& job=*Check;
    if(job.Request.value("ClientGeneration",uint64_t{})!=LazyCatalog::For(0).RuntimeGeneration||job.Request.value("CancelGeneration",uint64_t{})!=LazyCatalog::For(0).CancelGeneration){StopCheck("Live session or search changed; existing cache retained.");return;}
    if(now>=job.Deadline||job.Slices>=32){Resume.Put(job.Key,job.Cursor);StopCheck("Bounded check finished; discoveries retained. Refresh again to continue.");return;}
    if(job.Waiting){if(now>=job.ReplyDeadline)StopCheck("Live check timed out; cached results remain available.");return;}
    if(now<job.Next)return;
    auto request=job.Request;job.Id=++LazyCatalog::NextRequestId;request["Operation"]="CatalogWindow";request["Category"]=0;request["RequestId"]=job.Id;request["Cursor"]=job.Cursor;request["Mode"]="Alphabetical";request["Limit"]=64;request["Priority"]=job.Interactive?1:2;request["FieldSet"]="BasicRow";request["Reset"]=false;
    PassiveRequests.push_back(job.Id);while(PassiveRequests.size()>64)PassiveRequests.pop_front();job.Waiting=true;job.ReplyDeadline=now+std::chrono::seconds(3);++job.Slices;
    if(!LazyCatalog::RemoteSubmit||!LazyCatalog::RemoteSubmit(std::move(request)))StopCheck("Live check is not ready; existing cached results retained.");
}
inline bool Submit(const nlohmann::json& request){
    const bool offline=PluginRuntimeServices::ExternalDesktop&&!PluginRuntimeServices::GameProcessId.load();
    const auto cursor=request.value("Cursor",std::string{});
    if(cursor.starts_with("S:")||cursor.starts_with("F:")||request.value("Category",-1)!=0||request.value("Mode",std::string{})=="Random"||(!offline&&!HelpyRuntimePreferences::CacheFirst.load()&&HelpySourceCache::Ready()))return false;
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
        if(!page&&!Files.Awaiting()&&PluginRuntimeServices::ExternalDesktop&&!PluginRuntimeServices::GameProcessId.load())
            page=nlohmann::json{{"Rows",nlohmann::json::array()},{"HasMore",false},{"AlphabeticalReady",true},{"CacheHit",true},{"CachePreview",true},{"Coverage","Offline cache only. Connect the assigned game to discover uncached items; existing cached references remain browsable."}};
        if(page){
            Pending.reset();(*page)["Category"]=0;(*page)["Mode"]=request.at("Mode");(*page)["RequestId"]=request.at("RequestId");(*page)["CancelGeneration"]=request.at("CancelGeneration");(*page)["RuntimeGeneration"]=request.at("ClientGeneration");
            (*page)["Reset"]=request.value("Reset",false)||page->value("Reset",false);(*page)["PopulationStage"]="cached-items";(*page)["CheckingChanges"]=false;
            ApplyWindow(*page,state.AppliedRevision+1);
            PresentedCacheKey=Files.ContentKey();
        }else if(!Files.Awaiting()){
            Pending.reset();state.RequestedAt=std::chrono::steady_clock::now();auto native=request;
            if(!request.value("Query",std::string{}).empty()){native["Mode"]="Alphabetical";native["Cursor"]="F:0";}
            bool accepted=false;if(RemoteSubmit)accepted=RemoteSubmit(native);else{native["Action"]="CatalogWindow";accepted=SpawnToolRequests::TrySubmit(std::move(native));}
            if(!accepted){state.Pending=false;state.ActiveRequestId=0;state.LastError="No cached items are available yet. Download a cache or retry when the game is ready.";}
        }
    }
    PumpCheck();
    const auto now=std::chrono::steady_clock::now();
    if(NextPassive==std::chrono::steady_clock::time_point{})NextPassive=now+std::chrono::seconds(60);
    if(now<NextPassive||!state.Started||state.Pending||!RemoteSubmit||!HelpyRuntimePreferences::PassiveRefresh.load()||!HelpySourceCache::Ready()||(PluginRuntimeServices::ExternalDesktop&&!PluginRuntimeServices::GameProcessId.load()))return;
    NextPassive=now+std::chrono::seconds(60);
    if(Check)return;
    if(!state.Query.empty()&&QueueCheck({{"Query",state.Query},{"ClientGeneration",state.RuntimeGeneration},{"CancelGeneration",state.CancelGeneration}}))return;
    const auto id=++NextRequestId;PassiveRequests.push_back(id);if(PassiveRequests.size()>32)PassiveRequests.pop_front();
    const nlohmann::json request={{"Operation","CatalogWindow"},{"_Source","HelpyStandalone"},{"RequestId",id},{"Category",0},{"Mode","Alphabetical"},{"Query",""},{"Limit",1},{"Reset",true},{"FieldSet","BasicRow"},{"Priority",3},{"ClientGeneration",state.RuntimeGeneration},{"CancelGeneration",state.CancelGeneration},{"BrowsePolicy",{{"loadOrder","cacheFirst"},{"passiveRefresh",true}}}};
    RemoteSubmit(request);
}
inline void Configure(){
    LazyCatalog::LocalSubmit=Submit;LazyCatalog::LocalPoll=Pump;
    LazyCatalog::LocalWaiting=[](uint64_t id){return Pending&&Pending->value("RequestId",uint64_t{})==id;};
    LazyCatalog::IgnoreResponse=[](const auto& result){if(ReceiveCheck(result))return true;const auto id=result.value("RequestId",uint64_t{});return std::find(PassiveRequests.begin(),PassiveRequests.end(),id)!=PassiveRequests.end();};
    LazyCatalog::LocalShutdown=[]{Pending.reset();Files.Shutdown();WarmKey.clear();PresentedCacheKey.clear();PreparedCacheKey.clear();NextWarm={};Warming=false;PassiveRequests.clear();NextPassive={};ResetChecks();};
}
}
