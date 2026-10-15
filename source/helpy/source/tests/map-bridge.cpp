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
}
