#include "Generator/HelpyQueryService.h"
#include "Generator/ToolRequest.h"
#include <cassert>
#include <stdexcept>
using nlohmann::json;
int main(){
    using namespace PS;
    namespace queries=HelpyQueryService;
    queries::Clear();SpawnToolRequests::Available=false;SpawnToolRequests::Generation=91;
    json reply;unsigned publications=0,calls=0;
    SpawnToolRequests::PushUpdate=[&](const char* kind,const json& value,uint64_t){assert(std::string(kind)=="query");reply=value;++publications;};
    const auto request=[](uint64_t id,int category=0){return json{{"Operation","CatalogWindow"},{"Category",category},{"RequestId",id},{"CancelGeneration",1},{"Reset",true},{"Limit",24}};};
    const std::function<json(const json&)> read=[&](const json& value){++calls;return json{{"CatalogWindow",{{"RequestId",value["RequestId"]},{"Rows",json::array({{{"Path","/Game/Items/Iron.Iron"},{"Name","Iron Bar"}}})},{"HasMore",false}}}};};
    assert(queries::Submit(request(1))&&queries::Submit(request(2,1)));
    assert(!queries::PumpOne(true,read)&&queries::PendingCount()==2&&calls==0); // mutations keep priority
    assert(queries::PumpOne(false,read)&&queries::PendingCount()==1&&calls==1&&publications==1);
    assert(reply["CatalogWindow"]["Rows"][0]["Name"]=="Iron Bar"); // no world, player, icon or renderer exists in this test
    assert(queries::PumpOne(false,read)&&queries::PendingCount()==0&&calls==2);
    assert(!queries::PumpOne(false,read));
    assert(queries::Submit(request(3)));
    assert(queries::PumpOne(false,[](const json&)->json{throw std::runtime_error("ItemData not ready");}));
    assert(reply["CatalogWindow"]["RequestId"]==3&&reply["CatalogWindow"]["RuntimeGeneration"]==91&&reply["CatalogWindow"]["Error"]=="ItemData not ready");
    assert(queries::Submit(request(4,2)));
    assert(queries::PumpOne(false,[](const json&)->json{throw 42;}));
    assert(reply["CatalogWindow"]["RequestId"]==4&&!reply["CatalogWindow"]["Error"].get<std::string>().empty());
    assert(queries::Submit(request(5)));
    const auto beforeCancel=publications;
    assert(queries::PumpOne(false,[](const json&)->json{queries::Cancel(0,2);return {{"CatalogWindow",{{"RequestId",5}}}};}));
    assert(publications==beforeCancel); // cancellation during work cannot publish stale rows
    queries::Clear();SpawnToolRequests::PushUpdate={};
}
