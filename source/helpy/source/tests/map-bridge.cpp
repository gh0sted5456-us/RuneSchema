// Exercise the actual core-side request mailbox without an Unreal world.
#include "Generator/ToolRequest.h"
#include <cassert>
int main(){
    using Requests=PS::SpawnToolRequests;using Json=nlohmann::json;
    Requests::Available=true;uint64_t id=800;
    for(const char* key:{"MapSnapshot","MapBuildings","MapPoint"}){
        assert(Requests::TrySubmit({{"Action","MapProbe"},{"_RequestId",++id}}));
        const auto request=Requests::Take();assert(request&&request->at("_RequestId")==id);
        Requests::Publish({{"Status","Read only"},{key,{{"X",198000},{"Y",91450}}}});
        Json receipt;assert(Requests::TakeCompleted(id,receipt)&&receipt.contains(key));assert(!Requests::TakeCompleted(id,receipt));
        assert(Requests::TrySubmit({{"Action","MapTeleport"},{"_RequestId",++id}}));assert(!Requests::Read().contains(key));
        assert(Requests::Take());Requests::Publish({{"Status","Synthetic action"}});
    }
    Json pushed;
    Requests::PushUpdate=[&](const char*,const Json& update,uint64_t){pushed=update;};
    assert(Requests::TrySubmit({{"Action","MapTeleport"},{"_RequestId",++id}}));assert(Requests::Take());
    Requests::Progress("Loading destination",{{"MapTravel",{{"Phase","Preparing"},{"Committed",false}}},{"_RequestId",123},{"_InProgress",false}});
    Json receipt;
    assert(Requests::IsWaiting()&&!Requests::TakeCompleted(id,receipt));
    assert(!Requests::TrySubmit({{"Action","MapTeleport"},{"_RequestId",id+1}}));
    assert(pushed.at("_RequestId")==id&&pushed.at("_InProgress")==true);
    assert(pushed.at("MapTravel").at("Phase")=="Preparing");
    Requests::Publish({{"Status","Arrived"}});
    assert(!Requests::IsWaiting()&&Requests::TakeCompleted(id,receipt));
    assert(!Requests::TakeCompleted(id,receipt));
    assert(Requests::TrySubmit({{"Action","MapTeleport"},{"_RequestId",++id}}));
    assert(!Requests::Read().contains("MapTravel"));
    assert(Requests::Take());Requests::Publish({{"Status","Arrived again"}});
    Requests::PushUpdate={};
    Json hover={{"RequestId",uint64_t(900)},{"Generation",Requests::CurrentGeneration()},{"TargetLocation",{{"X",198000},{"Y",91450}}}};
    // Read-only queries coalesce without blocking a queued/in-flight journey.
    assert(Requests::SubmitMapGround(hover)&&!Requests::IsWaiting());
    hover["RequestId"]=uint64_t(901);assert(Requests::SubmitMapGround(hover));
    assert(Requests::TrySubmit({{"Action","MapTeleport"},{"_RequestId",++id}}));assert(Requests::Take());
    const auto latest=Requests::TakeMapGround();assert(latest&&latest->at("RequestId")==901&&!Requests::TakeMapGround());
    assert(Requests::IsWaiting());Requests::Publish({{"Status","Arrived"}});
    // Status polling is bounded/coalesced and cannot claim/release the mutation slot.
    assert(Requests::TrySubmit({{"Action","GiveItems"},{"_RequestId",++id}}));assert(Requests::Take());
    Requests::RequestPlayerRoles();Requests::RequestPlayerRoles();assert(Requests::TakePlayerRoles());assert(!Requests::TakePlayerRoles());
    assert(Requests::IsWaiting()&&!Requests::TakeCompleted(id,receipt));Requests::Publish({{"Status","Confirmed"}});
    assert(Requests::TakeCompleted(id,receipt));Requests::NextPlayerRoles={};Requests::RequestPlayerRoles();Requests::Clear();assert(!Requests::TakePlayerRoles());
    hover["Generation"]=Requests::CurrentGeneration()+1;assert(!Requests::SubmitMapGround(hover));
    hover["Generation"]=Requests::CurrentGeneration();hover["TargetLocation"]["X"]=1e12;assert(!Requests::SubmitMapGround(hover));
    hover["TargetLocation"]["X"]=198000;assert(Requests::SubmitMapGround(hover));Requests::Clear();assert(!Requests::TakeMapGround());
}
