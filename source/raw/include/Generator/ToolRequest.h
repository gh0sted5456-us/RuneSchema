#pragma once
#include <mutex>
#include <optional>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string_view>
#include <vector>
#include <deque>
#include <string>
#include <functional>
#include <cmath>
#include <chrono>
#include <nlohmann/json.hpp>
namespace PS {
struct NpcExportTag;struct SpawnToolTag;struct QuestStatusTag;struct ItemIconTag;
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
template<> struct ToolRequest<ItemIconTag> {
    static bool TrySubmit(nlohmann::json request);
    static void Submit(nlohmann::json request);
    static bool HasPending();
    static void SetAvailable(bool available);
    static std::optional<nlohmann::json> Take();
    static void Publish(nlohmann::json result);
    static nlohmann::json Read();
    static void Clear();
};
using NpcExportRequests=ToolRequest<NpcExportTag>;
// Spawn tools alone serialize mutation commands and retain one-shot acknowledgements.
// Catalog indexing publishes independent progress and never owns the mutation slot.
template<> struct ToolRequest<SpawnToolTag> {
    inline static std::function<void(std::string)> RemoteSearch;
    inline static std::function<bool(nlohmann::json)> RemoteSubmit;
    inline static std::function<bool(nlohmann::json)> RemoteMapGroundSubmit;
    inline static std::function<bool(uint64_t,nlohmann::json&)> RemoteCompleted;
    inline static std::function<bool(uint64_t&,int,nlohmann::json&)> RemoteReadQuick;
    inline static std::function<void()> RemoteCancel;
    inline static std::function<bool()> RemoteAvailable;
    inline static std::function<bool()> RemoteWaiting;
    inline static std::function<uint64_t()> RemoteGeneration;
    inline static std::function<void(const char*,const nlohmann::json&,uint64_t)> PushUpdate;
    inline static std::mutex Mutex;
    inline static std::optional<nlohmann::json> Pending;
    inline static std::optional<nlohmann::json> PendingMapGround;
    inline static std::atomic_bool PendingPlayerRoles=false;
    inline static std::chrono::steady_clock::time_point NextPlayerRoles{};
    static void RequestPlayerRoles(){
        std::lock_guard lock(Mutex);const auto now=std::chrono::steady_clock::now();
        if(Available&&now>=NextPlayerRoles){PendingPlayerRoles=true;NextPlayerRoles=now+std::chrono::seconds(5);}
    }
    static bool TakePlayerRoles(){
        if(!PendingPlayerRoles.load(std::memory_order_relaxed))return false;
        std::lock_guard lock(Mutex);
        return PendingPlayerRoles.exchange(false,std::memory_order_relaxed);
    }
    static std::atomic<bool> Waiting,Available,CancelRequested;
    static std::atomic<uint64_t> Generation;
    // Live catalog pieces (Definitions, Items, ...). Each value is immutable and shared: the game thread
    // builds a new one outside the lock, and readers copy pointers under it and build JSON after release.
    // Helpy's reads therefore never hold the mutex the game thread needs while catalogs are deep-copied.
    using SharedJson=std::shared_ptr<const nlohmann::json>;
    using CatalogMap=std::map<std::string,SharedJson,std::less<>>;
    static CatalogMap Share(nlohmann::json value) {
        CatalogMap fields;
        for(auto it=value.begin();it!=value.end();++it)
            fields.emplace(it.key(),std::make_shared<const nlohmann::json>(std::move(it.value())));
        return fields;
    }
    static nlohmann::json Expand(const CatalogMap& fields) {
        auto value=nlohmann::json::object();
        for(const auto& [key,field]:fields)value[key]=*field;
        return value;
    }
    inline static CatalogMap Result=Share({{"Status","Load a world before using runtime tools."}});
    inline static CatalogMap QuickCatalogData;
    static CatalogMap CatalogShared(){std::lock_guard lock(Mutex);return QuickCatalogData;}
    inline static uint64_t Revision=1,ActiveRequest=0;
    inline static std::map<uint64_t,nlohmann::json> Completed;
    inline static bool InFlight=false;
    inline static uint64_t NetworkCompletionId=0;
    inline static std::function<void(const nlohmann::json&)> NetworkCompletion;
    static void SetNetworkCompletion(uint64_t id,std::function<void(const nlohmann::json&)> callback){
        std::lock_guard lock(Mutex);NetworkCompletionId=id;NetworkCompletion=std::move(callback);
    }
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
        CatalogMap retired;
        const auto status=std::make_shared<const nlohmann::json>("Queued for the game thread.");
        std::lock_guard lock(Mutex);
        if(!Available||Pending||InFlight)return false;
        Pending=std::move(request);Waiting=true;
        for(const auto* key:{"GrantResults","SpawnResult","CloneResult","CloneSource","CloneAppearance","ItemDetails",
                "RecipeExportResult","ItemOverrideExportResult","KitMetadata","NativeContracts","DeveloperContracts",
                "CartValidation","ServerControl","RecipeWorkspaceCatalog","RecipeWorkspaceSource","JournalCatalog",
                "InventorySnapshot","InventoryAction","EquipmentAction","StorageResult","SpawnDefinition",
                "MapSnapshot","MapBuildings","MapPoint","MapTravel"})
            if(auto field=Result.extract(key);!field.empty())retired.insert(std::move(field));
        Result["Status"]=status;return true;
    }
    // Read-only, latest-point-wins mailbox; never owns the mutation slot.
    static bool SubmitMapGround(nlohmann::json request) {
        if(RemoteMapGroundSubmit)return RemoteMapGroundSubmit(std::move(request));
        try{
            if(!Available||!request.is_object()||!request.value("RequestId",uint64_t{})
                ||request.value("Generation",uint64_t{})!=Generation.load())return false;
            const auto& point=request.at("TargetLocation");
            for(const auto* key:{"X","Y"})if(!point.at(key).is_number()
                ||!std::isfinite(point.at(key).get<double>())||std::abs(point.at(key).get<double>())>10000000)return false;
            std::lock_guard lock(Mutex);PendingMapGround=std::move(request);return true;
        }catch(...){return false;}
    }
    static std::optional<nlohmann::json> TakeMapGround(){std::lock_guard lock(Mutex);auto request=std::move(PendingMapGround);PendingMapGround.reset();return request;}
    static void Submit(nlohmann::json request) {
        if(TrySubmit(std::move(request)))return;
        auto replacement=Share({{"Status","Runtime tool is not ready. Enable its loader and load a world."}});
        std::lock_guard lock(Mutex);
        if(!Available&&!Pending&&!InFlight) {
            Result.swap(replacement);
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
        auto fields=Share(std::move(result));
        const auto status=fields.find("Status");
        nlohmann::json receipt={{"Status",status==fields.end()?nlohmann::json("Completed."):*status->second}};
        for(const auto* key:{"GrantResults","KitMetadata","NativeContracts","ServerControl","SpawnResult","CloneResult",
                "CloneSource","CloneAppearance","ItemDetails","MapSnapshot","MapBuildings","MapPoint","RecipeExportResult",
                "RecipeWorkspaceCatalog","JournalCatalog","InventorySnapshot","EquipmentAction","InventoryAction",
                "StorageResult","RecipeWorkspaceSource","SpawnDefinition","ItemOverrideExportResult","SkillEdit"})
            if(const auto it=fields.find(key);it!=fields.end())receipt[key]=*it->second;
        CatalogMap pushedFields;uint64_t revision=0;
        std::map<uint64_t,nlohmann::json>::node_type oldReceipt;
        std::function<void(const nlohmann::json&)> completion;
        {
            std::lock_guard lock(Mutex);
            fields["_RequestId"]=std::make_shared<const nlohmann::json>(ActiveRequest);
            fields["_InProgress"]=std::make_shared<const nlohmann::json>(false);
            if(ActiveRequest&&ActiveRequest==NetworkCompletionId){completion=std::move(NetworkCompletion);NetworkCompletionId=0;}
            if(ActiveRequest){
                Completed[ActiveRequest]=std::move(receipt);
                if(Completed.size()>16)oldReceipt=Completed.extract(Completed.begin());
            }
            Result.swap(fields);InFlight=false;Waiting=false;ActiveRequest=0;revision=++Revision;
            if(completion||PushUpdate)pushedFields=Result;
        }
        if(!completion&&!PushUpdate)return;
        if(pushedFields.contains("PlayersDelta"))pushedFields.erase("Players");
        const auto pushed=Expand(pushedFields);
        if(completion)try{completion(pushed);}catch(...){/* Native result remains recorded; never repeat a mutation after a transport error. */}
        if(PushUpdate)PushUpdate("result",pushed,revision);
    }
    static bool TakeCompleted(uint64_t request,nlohmann::json& out) {
        if(RemoteCompleted)return RemoteCompleted(request,out);
        std::lock_guard lock(Mutex);auto it=Completed.find(request);
        if(it==Completed.end())return false;
        out=std::move(it->second);Completed.erase(it);return true;
    }
    static void Progress(const std::string& status,const nlohmann::json& details=nlohmann::json::object()) {
        CatalogMap incoming;
        if(details.is_object())for(auto it=details.begin();it!=details.end();++it)
            if(!it.key().starts_with("_")&&it.key()!="Status")incoming.emplace(it.key(),std::make_shared<const nlohmann::json>(it.value()));
        incoming["Status"]=std::make_shared<const nlohmann::json>(status);
        incoming["_InProgress"]=std::make_shared<const nlohmann::json>(true);
        CatalogMap retired;uint64_t revision=0;{
            std::lock_guard lock(Mutex);
            incoming["_RequestId"]=std::make_shared<const nlohmann::json>(ActiveRequest);
            for(const auto& [key,value]:incoming){
                if(auto field=Result.extract(key);!field.empty())retired.insert(std::move(field));
                Result.emplace(key,value);
            }
            revision=++Revision;
        }
        if(PushUpdate)PushUpdate("result",Expand(incoming),revision);
    }
    // Index snapshots never take/release the mutation slot or replace its receipt.
    static void CatalogProgress(const nlohmann::json& snapshot) {
        // Copy the (possibly large) catalog pieces before taking the lock.
        std::vector<std::pair<const char*,SharedJson>> incoming;
        for(const auto* key:{"Definitions","Items","Players","PlayerRoles","ServerMods","RemoteAccess","CatalogReady","_CatalogIndexing","CatalogStatus",
                "RegistryRecords","UnresolvedAssets","MetadataUnavailable","UnclassifiedRecords",
                "CatalogAI","CatalogResources","CatalogCoverage","CookedVisuals","CatalogIssues",
                "IndexStage","IndexDone","IndexTotal","IndexHasTotal","IndexFinished","IndexDetail"})
            if(snapshot.contains(key))incoming.emplace_back(key,std::make_shared<const nlohmann::json>(snapshot[key]));
        uint64_t revision=0;{
        std::lock_guard lock(Mutex);
        for(auto& [key,value]:incoming)QuickCatalogData[key].swap(value);
        revision=++Revision;}
        if(PushUpdate)PushUpdate("catalog",snapshot,revision);
    }
    static nlohmann::json Read(){
        CatalogMap fields;{std::lock_guard lock(Mutex);fields=Result;}
        return Expand(fields);
    }
    static bool ReadIfChanged(uint64_t& revision,nlohmann::json& out) {
        CatalogMap fields;uint64_t current;
        {std::lock_guard lock(Mutex);if(revision==Revision)return false;fields=Result;current=Revision;}
        out=Expand(fields);revision=current;return true;
    }
    // Only F2 opts into live index snapshots. The settings panel's Read() and
    // explicit saved Catalog result remain a point-in-time snapshot.
    static bool ReadQuickIfChanged(uint64_t& revision,int category,nlohmann::json& out) {
        if(RemoteReadQuick)return RemoteReadQuick(revision,category,out);
        CatalogMap catalog,fields;uint64_t current=0;
        {std::lock_guard lock(Mutex);if(revision==Revision)return false;current=Revision;catalog=QuickCatalogData;if(category!=-2)fields=Result;}
        out=CatalogSnapshot(Expand(fields),catalog,category);
        revision=current;return true;
    }
    static bool ReadQuickIfChanged(uint64_t& revision,nlohmann::json& out) {return ReadQuickIfChanged(revision,-1,out);}
    // Builds a category view from shared catalog pieces. Call without holding Mutex.
    //  -2: server mods and remote access only. -1: everything. 0: items. 1: AI/NPC definitions. 2: resources.
    static nlohmann::json CatalogSnapshot(nlohmann::json out,const CatalogMap& catalog,int category){
        const auto find=[&](std::string_view key)->const nlohmann::json*{const auto it=catalog.find(key);return it==catalog.end()||!it->second?nullptr:it->second.get();};
        const auto copy=[&](const char* key){if(const auto* value=find(key))out[key]=*value;};
        if(category==-2){copy("ServerMods");copy("RemoteAccess");return out;}
        if(category==0){copy("Items");copy("CookedVisuals");copy("CatalogIssues");}
        else if(category==1||category==2) {
            if(const auto* definitions=find("Definitions");definitions&&definitions->is_array()) {
                out["Definitions"]=nlohmann::json::array();
                for(const auto& row:*definitions) {
                    const auto type=row.value("Type",std::string{});
                    if((category==1&&(type=="AI"||type=="NPC"))||(category==2&&type=="Resource"))out["Definitions"].push_back(row);
                }
            }
        } else for(const auto& [key,value]:catalog)if(value)out[key]=*value;
        copy("Players");copy("PlayerRoles");copy("ServerMods");copy("RemoteAccess");
        for(const auto* key:{"CatalogReady","_CatalogIndexing","CatalogStatus","RegistryRecords","UnresolvedAssets",
                "MetadataUnavailable","UnclassifiedRecords","CatalogAI","CatalogResources","CatalogCoverage",
                "IndexStage","IndexDone","IndexTotal","IndexHasTotal","IndexFinished","IndexDetail"})copy(key);
        out["_Category"]=category;
        return out;
    }
    static void Clear() {
        const nlohmann::json pushed={{"Status","World changed; refresh tools."}};
        auto replacement=Share(pushed);
        CatalogMap catalog;
        decltype(Completed) receipts;
        uint64_t revision=0;{
        std::lock_guard lock(Mutex);Pending.reset();PendingMapGround.reset();SearchHint.reset();Waiting=false;InFlight=false;ActiveRequest=0;
        NetworkCompletion={};NetworkCompletionId=0;
        CancelRequested=true;Completed.swap(receipts);QuickCatalogData.swap(catalog);PendingPlayerRoles=false;NextPlayerRoles={};++Generation;
        Result.swap(replacement);revision=++Revision;}
        if(PushUpdate)PushUpdate("clear",pushed,revision);
    }
    static bool IsAvailable(){return RemoteAvailable?RemoteAvailable():Available.load();}
    static bool IsWaiting(){return RemoteWaiting?RemoteWaiting():Waiting.load();}
    static uint64_t CurrentGeneration(){return RemoteGeneration?RemoteGeneration():Generation.load();}
    static void Cancel(){if(RemoteCancel)RemoteCancel();else CancelRequested=true;}
    static void ClearRemote() {
        RemoteSearch={};RemoteSubmit={};RemoteMapGroundSubmit={};RemoteCompleted={};RemoteReadQuick={};RemoteCancel={};
        RemoteAvailable={};RemoteWaiting={};RemoteGeneration={};
    }
};
using SpawnToolRequests=ToolRequest<SpawnToolTag>;
using QuestStatusRequests=ToolRequest<QuestStatusTag>;
using ItemIconRequests=ToolRequest<ItemIconTag>;
}
