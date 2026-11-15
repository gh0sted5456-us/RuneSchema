#pragma once
#include "Core/ContentSource.h"
#include <nlohmann/json.hpp>
#include <mutex>
#include <optional>
#include <string>
#include <atomic>
namespace PS::IconCacheBuild {
using Json=nlohmann::json;
inline std::mutex Mutex;
struct Request {std::string Source,Item;bool Refresh=false;};
inline std::optional<Request> Pending;
inline bool Active=false,CancelRequested=false;
inline std::atomic<bool> PumpAvailable{false};
inline uint64_t Sequence=0;
inline Json Current={{"Available",false},{"Active",false},{"Stage","Idle"}};
inline Json Manifest=Json::object();
inline bool Start(const std::string& source,bool refresh=false,const std::string& item={}){
    if(!PumpAvailable||(!source.empty()&&!ContentSource::Valid(source)))return false;
    if(!item.empty()&&(!refresh||!source.empty()||item.size()>1024||ContentSource::Root(item).empty()||item.find('.')==item.npos))return false;
    if(refresh&&item.empty()&&!source.starts_with("mods/"))return false;
    std::lock_guard lock(Mutex);if(Active||Pending)return false;
    Pending=Request{source,item,refresh};Active=true;CancelRequested=false;Manifest=Json::object();++Sequence;
    Current={{"Available",true},{"Active",true},{"Stage","Queued"},{"Sequence",Sequence},{"Source",source},{"Item",item},{"Refresh",refresh}};return true;
}
inline std::optional<Request> Take(){std::lock_guard lock(Mutex);auto value=std::move(Pending);Pending.reset();return value;}
inline void Cancel(){std::lock_guard lock(Mutex);if(Active)CancelRequested=true;}
inline bool Cancelled(){std::lock_guard lock(Mutex);return CancelRequested;}
inline Json Read(bool manifest=false){std::lock_guard lock(Mutex);auto result=Current;result["Available"]=PumpAvailable.load();if(manifest)result.update(Manifest);return result;}
inline void SetManifest(Json value){std::lock_guard lock(Mutex);Manifest=std::move(value);}
inline bool HasWork(){std::lock_guard lock(Mutex);return Active||Pending.has_value();}
inline void Publish(Json value){std::lock_guard lock(Mutex);Active=value.value("Active",false);if(!Active)Pending.reset();value["Sequence"]=Sequence;Current=std::move(value);}
inline void Stop(const std::string& error){Publish({{"Available",true},{"Active",false},{"Stage","Stopped"},{"Error",error}});}
}
