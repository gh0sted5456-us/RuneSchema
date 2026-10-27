#pragma once
#include "Generator/LazyCatalog.h"
#include "Core/CacheBundleItems.h"
#include "Runtime/HostServices.h"
#include <optional>
#include <deque>

// UI requests wait only for this file worker. The native catalogue refreshes
// independently and cannot hold up cached presentation or page navigation.
namespace PS::HelpyCachedCatalog {
inline CacheBundleItems::Store Files;
inline std::optional<nlohmann::json> Pending;
inline std::deque<uint64_t> PassiveRequests;
inline std::chrono::steady_clock::time_point NextPassive{};
inline bool Submit(const nlohmann::json& request){
    if(request.value("Cursor",std::string{}).starts_with("S:")||request.value("Category",-1)!=0||request.value("Mode",std::string{})=="Random"||(!HelpyRuntimePreferences::CacheFirst.load()&&HelpySourceCache::Ready()))return false;
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
            (*page)["Reset"]=request.value("Reset",false)||page->value("Reset",false);(*page)["PopulationStage"]="cached-items";(*page)["CheckingChanges"]=HelpyRuntimePreferences::PassiveRefresh.load();
            ApplyWindow(*page,state.AppliedRevision+1);
        }else if(!Files.Awaiting()){
            Pending.reset();auto native=request;
            if(!request.value("Query",std::string{}).empty()){native["Mode"]="Search";native.erase("Cursor");}
            bool accepted=false;if(RemoteSubmit)accepted=RemoteSubmit(native);else{native["Action"]="CatalogWindow";accepted=SpawnToolRequests::TrySubmit(std::move(native));}
            if(!accepted){state.Pending=false;state.ActiveRequestId=0;state.LastError="No cached items are available yet. Download a cache or retry when the game is ready.";}
        }
    }
    const auto now=std::chrono::steady_clock::now();
    if(now<NextPassive||!state.Started||state.Pending||!RemoteSubmit||!HelpyRuntimePreferences::PassiveRefresh.load()||!HelpySourceCache::Ready())return;
    NextPassive=now+std::chrono::seconds(15);
    const auto id=++NextRequestId;PassiveRequests.push_back(id);if(PassiveRequests.size()>32)PassiveRequests.pop_front();
    const nlohmann::json request={{"Operation","CatalogWindow"},{"_Source","HelpyStandalone"},{"RequestId",id},{"Category",0},{"Mode","Alphabetical"},{"Query",""},{"Limit",1},{"Reset",true},{"FieldSet","BasicRow"},{"Priority",3},{"ClientGeneration",state.RuntimeGeneration},{"CancelGeneration",state.CancelGeneration},{"BrowsePolicy",{{"loadOrder","cacheFirst"},{"passiveRefresh",true}}}};
    RemoteSubmit(request);
}
inline void Configure(){
    LazyCatalog::LocalSubmit=Submit;LazyCatalog::LocalPoll=Pump;
    LazyCatalog::IgnoreResponse=[](const auto& result){const auto id=result.value("RequestId",uint64_t{});return std::find(PassiveRequests.begin(),PassiveRequests.end(),id)!=PassiveRequests.end();};
    LazyCatalog::LocalShutdown=[]{Pending.reset();Files.Shutdown();PassiveRequests.clear();NextPassive={};};
}
}
