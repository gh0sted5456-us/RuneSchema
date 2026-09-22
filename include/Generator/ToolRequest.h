#pragma once
#include <mutex>
#include <optional>
#include <atomic>
#include <cstdint>
#include <map>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>
namespace PS {
template<class Tag> struct ToolRequest {
    inline static std::mutex Mutex;
    inline static std::optional<nlohmann::json> Pending;
    inline static std::atomic<bool> Waiting=false;
    inline static std::atomic<bool> Available=false;
    inline static nlohmann::json Result={{"Status","Load a world before using runtime tools."}};
    static void Submit(nlohmann::json request) {
        std::lock_guard lock(Mutex);
        if(!Available){Result={{"Status","Runtime tool is not ready. Enable its loader and load a world."}};return;}
        if(Pending)return;
        Pending=std::move(request);Waiting=true;Result["Status"]="Queued for the game thread.";
    }
    static std::optional<nlohmann::json> Take() {
        if(!Waiting.load())return {};
        std::lock_guard lock(Mutex);auto value=std::move(Pending);Pending.reset();Waiting=false;return value;
    }
    static void Publish(nlohmann::json result){std::lock_guard lock(Mutex);Result=std::move(result);}
    static nlohmann::json Read(){std::lock_guard lock(Mutex);return Result;}
    static void Clear(){std::lock_guard lock(Mutex);Pending.reset();Waiting=false;Result={{"Status","World changed; refresh tools."}};}
};
struct NpcExportTag;struct SpawnToolTag;struct QuestStatusTag;struct ItemIconTag;
using NpcExportRequests=ToolRequest<NpcExportTag>;
// Spawn tools alone serialize mutation commands and retain one-shot acknowledgements.
// Catalog indexing publishes independent progress and never owns the mutation slot.
template<> struct ToolRequest<SpawnToolTag> {
    inline static std::function<void(std::string)> RemoteSearch;
    inline static std::function<bool(nlohmann::json)> RemoteSubmit;
    inline static std::function<bool(uint64_t,nlohmann::json&)> RemoteCompleted;
    inline static std::function<bool(uint64_t&,int,nlohmann::json&)> RemoteReadQuick;
    inline static std::function<void()> RemoteCancel;
    inline static std::function<bool()> RemoteAvailable;
    inline static std::function<bool()> RemoteWaiting;
    inline static std::function<uint64_t()> RemoteGeneration;
    inline static std::function<void(const char*,const nlohmann::json&,uint64_t)> PushUpdate;
    inline static std::mutex Mutex;
    inline static std::optional<nlohmann::json> Pending;
    inline static std::atomic<bool> Waiting=false,Available=false,CancelRequested=false;
    inline static std::atomic<uint64_t> Generation=1;
    inline static nlohmann::json Result={{"Status","Load a world before using runtime tools."}};
    inline static nlohmann::json QuickCatalogData=nlohmann::json::object();
    inline static uint64_t Revision=1,ActiveRequest=0;
    inline static std::map<uint64_t,nlohmann::json> Completed;
    inline static bool InFlight=false;
    inline static std::optional<std::string> SearchHint;
    static void PrioritizeSearch(std::string query) {
        if(RemoteSearch){RemoteSearch(std::move(query));return;}
        if(query.size()>2048)return;
        std::lock_guard lock(Mutex);SearchHint=std::move(query);
    }
    static std::optional<std::string> TakeSearchHint() {
        std::lock_guard lock(Mutex);auto value=std::move(SearchHint);SearchHint.reset();return value;
    }
    static bool TrySubmit(nlohmann::json request) {
        if(RemoteSubmit)return RemoteSubmit(std::move(request));
        std::lock_guard lock(Mutex);
        if(!Available||Pending||InFlight)return false;
        Pending=std::move(request);Waiting=true;
        Result.erase("GrantResults");Result.erase("SpawnResult");
        Result.erase("CloneResult");Result.erase("CloneSource");Result.erase("CloneAppearance");Result.erase("ItemDetails");
        Result.erase("RecipeExportResult");Result.erase("ItemOverrideExportResult");
        Result["Status"]="Queued for the game thread.";return true;
    }
    static void Submit(nlohmann::json request) {
        if(TrySubmit(std::move(request)))return;
        std::lock_guard lock(Mutex);
        if(!Available&&!Pending&&!InFlight) {
            Result={{"Status","Runtime tool is not ready. Enable its loader and load a world."}};
            ++Revision;
        }
    }
    static std::optional<nlohmann::json> Take() {
        if(!Waiting.load())return {};
        std::lock_guard lock(Mutex);
        if(!Pending||InFlight)return {};
        auto value=std::move(Pending);Pending.reset();InFlight=true;
        ActiveRequest=value->value("_RequestId",uint64_t{});return value;
    }
    static void Publish(nlohmann::json result) {
        nlohmann::json pushed;uint64_t revision=0;
        {
        std::lock_guard lock(Mutex);
        result["_RequestId"]=ActiveRequest;result["_InProgress"]=false;
        if(ActiveRequest) {
            // Compact one-shot receipts survive a second settings request.
            // Do not retain copies of full asset catalogs for every command.
            nlohmann::json receipt={{"Status",result.value("Status",std::string("Completed."))}};
            if(result.contains("GrantResults"))receipt["GrantResults"]=result["GrantResults"];
            if(result.contains("SpawnResult"))receipt["SpawnResult"]=result["SpawnResult"];
            if(result.contains("CloneResult"))receipt["CloneResult"]=result["CloneResult"];
            if(result.contains("CloneSource"))receipt["CloneSource"]=result["CloneSource"];
            if(result.contains("CloneAppearance"))receipt["CloneAppearance"]=result["CloneAppearance"];
            if(result.contains("ItemDetails"))receipt["ItemDetails"]=result["ItemDetails"];
            if(result.contains("RecipeExportResult"))receipt["RecipeExportResult"]=result["RecipeExportResult"];
            if(result.contains("ItemOverrideExportResult"))receipt["ItemOverrideExportResult"]=result["ItemOverrideExportResult"];
            Completed[ActiveRequest]=std::move(receipt);
            while(Completed.size()>16)Completed.erase(Completed.begin());
        }
        Result=std::move(result);InFlight=false;Waiting=false;ActiveRequest=0;revision=++Revision;pushed=Result;
        }
        if(PushUpdate)PushUpdate("result",pushed,revision);
    }
    static bool TakeCompleted(uint64_t request,nlohmann::json& out) {
        if(RemoteCompleted)return RemoteCompleted(request,out);
        std::lock_guard lock(Mutex);auto it=Completed.find(request);
        if(it==Completed.end())return false;
        out=std::move(it->second);Completed.erase(it);return true;
    }
    static void Progress(const std::string& status) {
        nlohmann::json pushed;uint64_t revision=0;{
        std::lock_guard lock(Mutex);Result["Status"]=status;Result["_RequestId"]=ActiveRequest;
        Result["_InProgress"]=true;revision=++Revision;pushed=Result;}
        if(PushUpdate)PushUpdate("result",pushed,revision);
    }
    // Index snapshots never take/release the mutation slot or replace its receipt.
    static void CatalogProgress(const nlohmann::json& snapshot) {
        uint64_t revision=0;{
        std::lock_guard lock(Mutex);
        for(const auto* key:{"Definitions","Items","Players","CatalogReady","_CatalogIndexing","CatalogStatus",
                "RegistryRecords","UnresolvedAssets","MetadataUnavailable","UnclassifiedRecords",
                "CatalogAI","CatalogResources","CatalogCoverage","CookedVisuals","CatalogIssues",
                "IndexStage","IndexDone","IndexTotal","IndexHasTotal","IndexFinished","IndexDetail"})
            if(snapshot.contains(key))QuickCatalogData[key]=snapshot[key];
        revision=++Revision;}
        if(PushUpdate)PushUpdate("catalog",snapshot,revision);
    }
    static nlohmann::json Read(){std::lock_guard lock(Mutex);return Result;}
    static bool ReadIfChanged(uint64_t& revision,nlohmann::json& out) {
        std::lock_guard lock(Mutex);if(revision==Revision)return false;
        out=Result;revision=Revision;return true;
    }
    // Only F2 opts into live index snapshots. The settings panel's Read() and
    // explicit saved Catalog result remain a point-in-time snapshot.
    static bool ReadQuickIfChanged(uint64_t& revision,int category,nlohmann::json& out) {
        if(RemoteReadQuick)return RemoteReadQuick(revision,category,out);
        std::lock_guard lock(Mutex);if(revision==Revision)return false;
        out=Result;
        const auto copy=[&](const char* key){if(QuickCatalogData.contains(key))out[key]=QuickCatalogData[key];};
        if(category==0){copy("Items");copy("CookedVisuals");copy("CatalogIssues");}
        else if(category==1||category==2) {
            if(QuickCatalogData.contains("Definitions")&&QuickCatalogData["Definitions"].is_array()) {
                out["Definitions"]=nlohmann::json::array();
                for(const auto& row:QuickCatalogData["Definitions"]) {
                    const auto type=row.value("Type",std::string{});
                    if((category==1&&(type=="AI"||type=="NPC"))||(category==2&&type=="Resource"))out["Definitions"].push_back(row);
                }
            }
        } else for(auto it=QuickCatalogData.begin();it!=QuickCatalogData.end();++it)out[it.key()]=it.value();
        copy("Players");
        for(const auto* key:{"CatalogReady","_CatalogIndexing","CatalogStatus","RegistryRecords","UnresolvedAssets",
                "MetadataUnavailable","UnclassifiedRecords","CatalogAI","CatalogResources","CatalogCoverage",
                "IndexStage","IndexDone","IndexTotal","IndexHasTotal","IndexFinished","IndexDetail"})copy(key);
        out["_Category"]=category;
        revision=Revision;return true;
    }
    static bool ReadQuickIfChanged(uint64_t& revision,nlohmann::json& out) {return ReadQuickIfChanged(revision,-1,out);}
    static void Clear() {
        nlohmann::json pushed;uint64_t revision=0;{
        std::lock_guard lock(Mutex);Pending.reset();SearchHint.reset();Waiting=false;InFlight=false;ActiveRequest=0;
        CancelRequested=true;Completed.clear();QuickCatalogData=nlohmann::json::object();++Generation;
        Result={{"Status","World changed; refresh tools."}};revision=++Revision;pushed=Result;}
        if(PushUpdate)PushUpdate("clear",pushed,revision);
    }
    static bool IsAvailable(){return RemoteAvailable?RemoteAvailable():Available.load();}
    static bool IsWaiting(){return RemoteWaiting?RemoteWaiting():Waiting.load();}
    static uint64_t CurrentGeneration(){return RemoteGeneration?RemoteGeneration():Generation.load();}
    static void Cancel(){if(RemoteCancel)RemoteCancel();else CancelRequested=true;}
    static void ClearRemote() {
        RemoteSearch={};RemoteSubmit={};RemoteCompleted={};RemoteReadQuick={};RemoteCancel={};
        RemoteAvailable={};RemoteWaiting={};RemoteGeneration={};
    }
};
using SpawnToolRequests=ToolRequest<SpawnToolTag>;
using QuestStatusRequests=ToolRequest<QuestStatusTag>;
using ItemIconRequests=ToolRequest<ItemIconTag>;
}
