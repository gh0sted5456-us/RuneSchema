#pragma once
#include "Generator/ToolRequest.h"
#include "Core/ItemPresentation.h"
#include <chrono>
#include <map>
namespace PS::ItemDetails {
using Json=nlohmann::json;
using Clock=std::chrono::steady_clock;
struct Entry {Json Native=Json::object();std::string Error;};
inline std::map<std::string,Entry> Cache;
inline uint64_t Generation=0,Pending=0,Sequence=0x7100000000000000ull;
inline std::string PendingPath;
inline Clock::time_point RequestedAt{},NextRequest{};
inline void Poll(Clock::time_point now=Clock::now()) {
    const auto generation=SpawnToolRequests::CurrentGeneration();
    if(Generation!=generation){Generation=generation;Cache.clear();Pending=0;PendingPath.clear();NextRequest={};}
    if(!Pending)return;Json receipt;
    if(SpawnToolRequests::TakeCompleted(Pending,receipt)){
        Entry entry;
        if(receipt.contains("ItemDetails")&&receipt["ItemDetails"].is_object()&&ItemPresentation::Text(receipt["ItemDetails"],"Source")==PendingPath){
            const auto& native=receipt["ItemDetails"];
            for(const auto* key:{"Source","Name","Icon","PowerLevel"})if(native.contains(key)&&!native[key].is_structured())entry.Native[key]=native[key];
            entry.Native["Description"]=ItemPresentation::Description(native);
            entry.Native["Fields"]=Json::array();
            if(native.contains("Fields")&&native["Fields"].is_array())for(const auto& field:native["Fields"]){
                if(!field.is_object()||!field.contains("Value")||field["Value"].is_structured()||field["Value"].dump().size()>4096)continue;
                auto compact=Json::object();for(const auto* key:{"Name","NativeName","HasValue","Value"})if(field.contains(key))compact[key]=field[key];
                entry.Native["Fields"].push_back(std::move(compact));if(entry.Native["Fields"].size()==128)break;
            }
        }
        else entry.Error=ItemPresentation::Text(receipt,"Status").empty()?"Native details are unavailable for this item.":ItemPresentation::Text(receipt,"Status");
        if(Cache.size()>=48&&!Cache.contains(PendingPath))Cache.erase(Cache.begin());
        Cache.insert_or_assign(PendingPath,std::move(entry));Pending=0;PendingPath.clear();
    }else if(now-RequestedAt>std::chrono::seconds(15)){
        Cache.insert_or_assign(PendingPath,Entry{Json::object(),"Native details timed out. Right-click to reload details."});Pending=0;PendingPath.clear();
    }
}
inline const Entry* Find(const std::string& path){const auto found=Cache.find(path);return found==Cache.end()?nullptr:&found->second;}
inline void Request(const std::string& path,Clock::time_point now=Clock::now()) {
    Poll(now);
    if(path.empty()||path.size()>1024||path.front()!='/'||Cache.contains(path)||Pending||now<NextRequest||!SpawnToolRequests::IsAvailable()||SpawnToolRequests::IsWaiting())return;
    NextRequest=now+std::chrono::milliseconds(700);
    const auto request=++Sequence;
    // This host action only inspects metadata on the game thread; no player,
    // cloning, inventory grant, or authoring operation is requested.
    if(SpawnToolRequests::TrySubmit({{"Action","InspectClone"},{"InspectDetails",true},{"Source",path},{"_Source","HelpyHover"},{"_RequestId",request}})){
        Pending=request;PendingPath=path;RequestedAt=now;
    }
}
inline void Reload(const std::string& path){Cache.erase(path);NextRequest={};Request(path);}
}
