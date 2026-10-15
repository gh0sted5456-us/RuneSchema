// Exercise production serialization, push decoding, and query state together.
// The fixture renderer alone cannot catch a lost host/plugin message.
#include "../plugins/helpy/HelpyPlugin.cpp"
#include "Generator/SpawnToolsPanel.h"
#include "Core/MapProjection.h"
#include "Core/MapSpawnRequest.h"
#include <cassert>
namespace {
using json=nlohmann::json;
json LastQuery,LastSubmission,HostSnapshot={{"Players",json::array({{{"Path","/Player/Local"},{"Name","Local player"}}})}};
uint64_t HostGeneration=41,HostRevision=8;
bool ThrowHost=false;
int32_t RS_PLUGIN_CALL HostCall(const char*,const char* service,const char* input,char* output,uint32_t capacity,uint32_t* size){
    if(ThrowHost)throw std::runtime_error("host read failed");
    assert(std::string(service)=="runeschema.tools");
    const auto request=json::parse(input);json reply;
    if(request["Action"]=="Read"){
        reply={{"Available",true},{"Waiting",false},{"Generation",HostGeneration},{"Revision",HostRevision}};
        if(request.value("Revision",uint64_t{})!=HostRevision)reply["Snapshot"]=HostSnapshot;
    }else{
        if(request["Action"]=="QuerySubmit")LastQuery=request.at("Request");
        if(request["Action"]=="Submit")LastSubmission=request.at("Request");
        reply={{"Accepted",true}};
    }
    const auto bytes=reply.dump();*size=static_cast<uint32_t>(bytes.size()+1);
    if(!output||capacity<*size)return RS_PLUGIN_BUFFER_TOO_SMALL;
    std::memcpy(output,bytes.c_str(),*size);return RS_PLUGIN_OK;
}
}
int main(){
    RuneSchemaHostApi host{};host.CallService=HostCall;State state{&host};ConfigureTransport(&state);
    assert(state.ToolsGeneration==41&&state.ToolsAvailable&&state.Players.size()==1); // No bootstrap push required.
    using namespace PS::LazyCatalog;
    ResetForGeneration(41);FirstOpen(41);
    assert(LastQuery["Operation"]=="CatalogWindow"&&LastQuery["Category"]==0&&LastQuery["ClientGeneration"]==41);
    const auto push=[&](json rows,bool more,std::string cursor){
        const auto request=LastQuery;
        json window={{"RequestId",request["RequestId"]},{"CancelGeneration",request["CancelGeneration"]},{"RuntimeGeneration",HostGeneration},
            {"Category",0},{"Mode",request["Mode"]},{"Reset",request["Reset"]},{"Rows",rows},{"HasMore",more},{"NextCursor",cursor},
            {"Dictionary",{{"Icons",json::array({{{"Id",uint64_t(7)},{"Value","/Game/Icons/Log.Log"}}})}}}};
        const auto message=json({{"Kind","query"},{"Generation",HostGeneration},{"Available",true},{"Payload",{{"CatalogWindow",window}}}}).dump();
        uint32_t size=0;assert(ToolsPush(&state,message.c_str(),nullptr,0,&size)==RS_PLUGIN_OK);
        uint64_t revision=0;json snapshot;assert(PS::SpawnToolRequests::RemoteReadQuick(revision,0,snapshot));
        Apply(snapshot,revision);Overlay(snapshot,0);return snapshot;
    };
    // Real host replies can be empty while still scanning toward ItemData.
    for(int i=0;i<8;++i){push(json::array(),true,"R:18:"+std::to_string((i+1)*2048));assert(For(0).Pending&&For(0).LastError.empty());}
    auto snapshot=push(json::array({{{"Path","/Game/Items/Log.Log"},{"Name","Ash logs"},{"Icon",nullptr},{"IconId",uint64_t(7)}}}),false,"done");
    assert(snapshot["Items"].size()==1&&snapshot["Items"][0]["Icon"]=="/Game/Icons/Log.Log");
    assert(snapshot["Players"].size()==1&&!For(0).Pending);
    // Replaying a snapshot must not duplicate a window.
    Apply(snapshot,999);assert(For(0).Rows.size()==1);
    assert(Retry(0));CheckTimeouts(For(0).RequestedAt+std::chrono::seconds(16));assert(!For(0).Pending&&!For(0).LastError.empty());
    assert(Retry(0));snapshot=push(json::array({{{"Path","/Game/Items/Stone.Stone"},{"Name","Stone"}}}),false,"done");
    assert(snapshot["Items"][0]["Name"]=="Stone");
    // A missed world-change push is recovered by the read-only state read.
    HostGeneration=42;++HostRevision;HostSnapshot["Players"]=json::array();SyncHost(&state,true);
    assert(state.ToolsGeneration==42&&state.Players.empty());ResetForGeneration(42);FirstOpen(42);assert(LastQuery["ClientGeneration"]==42);
    PS::SpawnToolsPanel::RefreshSnapshot(-1);
    PS::SpawnToolsPanel::DefinitionPending=777;
    state.Completed[777]={{"SpawnDefinition",{{"Key","npc:test"},{"Name","Test NPC"}}}};
    PS::SpawnToolsPanel::RefreshSnapshot(-1);
    assert(PS::SpawnToolsPanel::InspectedDefinitions.at("npc:test")["Name"]=="Test NPC");
    PS::SpawnToolsPanel::DefinitionPending=778;state.Completed[778]={{"SpawnDefinition",{{"Key",nullptr}}}};
    PS::SpawnToolsPanel::RefreshSnapshot(-1);assert(!PS::SpawnToolsPanel::DefinitionStatus.empty());
    PS::ItemDetails::Request("/Game/Items/Stone.Stone");const auto detailId=PS::ItemDetails::Pending;
    assert(detailId&&PS::ItemDetails::PendingPath=="/Game/Items/Stone.Stone");
    assert(LastSubmission["Action"]=="InspectClone"&&LastSubmission["InspectDetails"]==true&&!LastSubmission.contains("Player"));
    // Read-only item details travel over the same production request/receipt path.
    state.ToolsWaiting=false;
    const json nativeDetails={{"Source","/Game/Items/Stone.Stone"},{"Name","Stone"},{"PowerLevel",4},
        {"Fields",json::array({{{"NativeName","Description"},{"Value","Native description"},{"HasValue",true}},{{"NativeName","BaseDamage"},{"Value",12},{"HasValue",true}}})}};
    const json detailMessage={{"Kind","result"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},{"Payload",{{"_RequestId",detailId},{"ItemDetails",nativeDetails}}}};
    uint32_t responseSize=0;const auto detailWire=detailMessage.dump();assert(ToolsPush(&state,detailWire.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    PS::ItemDetails::Poll();const auto* details=PS::ItemDetails::Find("/Game/Items/Stone.Stone");assert(details&&details->Error.empty());
    assert(PS::ItemPresentation::Description(details->Native)=="Native description");
    assert(PS::ItemPresentation::Facts(details->Native)[0].Value=="12");
    PS::ItemDetails::Request("/Game/Items/Stone.Stone");assert(!PS::ItemDetails::Pending); // Cached hover never resubmits.
    HostGeneration=43;SyncHost(&state,true);PS::ItemDetails::Poll();assert(!PS::ItemDetails::Find("/Game/Items/Stone.Stone"));
    // Map coordinates and artwork metadata survive production JSON transport.
    PS::MapProjection::Projection projection{{198000,91450},{0,1},{1,0},210000,210000,-90000,90000};
    constexpr uint64_t mapId=700;
    assert(PS::SpawnToolRequests::TrySubmit({{"Action","MapSnapshot"},{"_RequestId",mapId}}));
    assert(LastSubmission["Action"]=="MapSnapshot");
    json map={{"BridgeVersion",1},{"Projection",projection.ToJson()},{"Image",{{"File","native-map.rsicon"},{"Status","Ready"}}},
        {"Players",json::array({{{"Name","Local player"},{"Location",{198000,91450,100}}}})}};
    const auto mapWire=json({{"Kind","result"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},{"Payload",{{"_RequestId",mapId},{"MapSnapshot",map}}}}).dump();
    assert(ToolsPush(&state,mapWire.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);json mapReceipt;
    assert(PS::SpawnToolRequests::TakeCompleted(mapId,mapReceipt)&&mapReceipt["MapSnapshot"]==map);
    const auto placement=PS::MapSpawnRequest::Build(projection,projection.ToWorld({0.25,0.75}),{{"Key","tree"},{"Class","/Game/Tree.Tree_C"},{"Type","Resource"}},"player",true,1,0,701);
    assert(PS::SpawnToolRequests::TrySubmit(placement)&&LastSubmission==placement);
    ThrowHost=true;json ignored;assert(!CallCore(&state,{{"Action","Read"}},ignored));
    PS::ItemIconCache::ClearRemote();ClearRemote();PS::SpawnToolRequests::ClearRemote();
}
