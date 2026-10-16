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
PS::HelpyAuthoring::Session AuthoringHost;
int32_t RS_PLUGIN_CALL HostCall(const char*,const char* service,const char* input,char* output,uint32_t capacity,uint32_t* size){
    if(ThrowHost)throw std::runtime_error("host read failed");
    assert(std::string(service)=="runeschema.tools");
    const auto request=json::parse(input);json reply;
    if(request["Action"]=="AuthoringSnapshot")reply=AuthoringHost.Snapshot(request.value("Revision",uint64_t{}));
    else if(request["Action"]=="AuthoringSubmit")reply=AuthoringHost.Submit(request.at("Request"));
    else if(request["Action"]=="AuthoringActivate")reply=AuthoringHost.Activate();
    else if(request["Action"]=="Read"){
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
    const json nativeDetails={{"Source","/Game/Items/Stone.Stone"},{"Name","Stone"},{"Icon","/Custom/UI/ClonedHelm.ClonedHelm"},{"PowerLevel",4},
        {"Fields",json::array({{{"NativeName","Description"},{"Value","Native description"},{"HasValue",true}},{{"NativeName","BaseDamage"},{"Value",12},{"HasValue",true}}})}};
    const json detailMessage={{"Kind","result"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},{"Payload",{{"_RequestId",detailId},{"ItemDetails",nativeDetails}}}};
    uint32_t responseSize=0;const auto detailWire=detailMessage.dump();assert(ToolsPush(&state,detailWire.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    PS::ItemDetails::Poll();const auto* details=PS::ItemDetails::Find("/Game/Items/Stone.Stone");assert(details&&details->Error.empty());
    assert(PS::ItemPresentation::Description(details->Native)=="Native description");
    assert(PS::ItemPresentation::Facts(details->Native)[0].Value=="12");
    const json staleIcon={{"Path","/Game/Items/Stone.Stone"},{"Icon","/Game/UI/Source.Source"}};
    assert(PS::ItemGridPicker::IconForRow(staleIcon)=="/Custom/UI/ClonedHelm.ClonedHelm");
    PS::ItemGridPicker::ClearSharedCart();assert(PS::ItemGridPicker::AddToCart(staleIcon));
    assert(PS::ItemGridPicker::SharedCart.Entries()[0].Row["Icon"]=="/Custom/UI/ClonedHelm.ClonedHelm");PS::ItemGridPicker::ClearSharedCart();
    PS::ItemDetails::Request("/Game/Items/Stone.Stone");assert(!PS::ItemDetails::Pending); // Cached hover never resubmits.
    PS::ItemGridPicker::PinDetails(staleIcon);PS::ItemGridPicker::DetachedBay=true;
    assert(std::string(PS::ItemGridPicker::Bay.Selected)=="/Game/Items/Stone.Stone"&&PS::ItemGridPicker::Bay.DetailsExpanded);
    assert(PS::ItemPresentation::Description(PS::ItemDetails::Find(PS::ItemGridPicker::Bay.Selected)->Native)=="Native description");
    PS::ItemGridPicker::DetachedBay=false;assert(PS::ItemGridPicker::Bay.DetailsRow==staleIcon);
    HostGeneration=43;SyncHost(&state,true);PS::ItemDetails::Poll();assert(!PS::ItemDetails::Find("/Game/Items/Stone.Stone"));
    // Same cart, two authoritative recipients: one serialized request at a
    // time, immutable target snapshot, and clear only after BOTH confirmations.
    PS::SpawnToolsPanel::PollCartDelivery();state.ToolsWaiting=false;
    const auto batchItem=json{{"Path","/Game/Items/Stone.Stone"},{"Name","Stone"}};
    assert(PS::ItemGridPicker::AddToCart(batchItem));
    PS::SpawnToolsPanel::CartBatch.Begin(PS::ItemGridPicker::SharedCart.Payload(),{{"/Game/Players/A.A","Alice"},{"/Game/Players/B.B","Bob"}});
    const auto batchFirst=++PS::SpawnToolsPanel::CartSequence;assert(PS::ItemGridPicker::SharedCart.BeginDelivery(batchFirst));
    assert(PS::SpawnToolsPanel::SubmitNextCartRecipient(batchFirst));assert(LastSubmission["Player"]=="/Game/Players/A.A");
    const auto grant=json{{"GrantResults",json::array({{{"Item","/Game/Items/Stone.Stone"},{"Count",1},{"State","Confirmed"}}})}};
    state.Completed[batchFirst]=grant;state.ToolsWaiting=false;PS::SpawnToolsPanel::PollCartDelivery();
    assert(PS::ItemGridPicker::SharedCart.Entries().size()==1&&PS::ItemGridPicker::SharedCart.Delivery());
    assert(LastSubmission["Player"]=="/Game/Players/B.B");const auto batchSecond=PS::SpawnToolsPanel::CartBatch.PendingId();assert(batchSecond&&batchSecond!=batchFirst);
    state.Completed[batchSecond]=grant;state.ToolsWaiting=false;PS::SpawnToolsPanel::PollCartDelivery();
    assert(PS::ItemGridPicker::SharedCart.Entries().empty()&&!PS::ItemGridPicker::SharedCart.Delivery()&&!PS::SpawnToolsPanel::CartNeedsReview);
    assert(PS::ItemGridPicker::AddToCart(batchItem));PS::SpawnToolsPanel::CartBatch.Begin(PS::ItemGridPicker::SharedCart.Payload(),{{"/Game/Players/A.A","Alice"},{"/Game/Players/B.B","Bob"}});
    const auto failedFirst=++PS::SpawnToolsPanel::CartSequence;assert(PS::ItemGridPicker::SharedCart.BeginDelivery(failedFirst));assert(PS::SpawnToolsPanel::SubmitNextCartRecipient(failedFirst));
    state.Completed[failedFirst]={{"Status","Request unconfirmed"}};state.ToolsWaiting=false;PS::SpawnToolsPanel::PollCartDelivery();
    assert(LastSubmission["Player"]=="/Game/Players/A.A"&&PS::SpawnToolsPanel::CartNeedsReview&&PS::ItemGridPicker::SharedCart.Entries().size()==1);
    assert(!PS::ItemGridPicker::SharedCart.Delivery());PS::SpawnToolsPanel::PollCartDelivery();assert(LastSubmission["Player"]=="/Game/Players/A.A");
    PS::SpawnToolsPanel::CartNeedsReview=false;PS::ItemGridPicker::ClearSharedCart();
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
    // Stage updates cannot masquerade as arrival or free the UI mutation slot.
    const auto travelProgress=json({{"Kind","result"},{"Available",true},{"Waiting",true},{"Generation",HostGeneration},
        {"Payload",{{"_RequestId",uint64_t(702)},{"_InProgress",true},{"Status","Loading destination"},
            {"MapTravel",{{"Phase","Preparing"},{"ElapsedSeconds",3},{"Committed",false}}}}}}).dump();
    assert(ToolsPush(&state,travelProgress.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    assert(PS::SpawnToolRequests::IsWaiting()&&!PS::SpawnToolRequests::TakeCompleted(702,mapReceipt));
    uint64_t travelRevision=0;json travelSnapshot;
    assert(PS::SpawnToolRequests::RemoteReadQuick(travelRevision,-1,travelSnapshot));
    assert(travelSnapshot["MapTravel"]["Phase"]=="Preparing"&&travelSnapshot.value("Players",json::array())==state.Players);
    const auto arrived=json({{"Kind","result"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},
        {"Payload",{{"_RequestId",uint64_t(702)},{"_InProgress",false},{"Status","Arrived"}}}}).dump();
    assert(ToolsPush(&state,arrived.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    assert(!PS::SpawnToolRequests::IsWaiting()&&PS::SpawnToolRequests::TakeCompleted(702,mapReceipt));
    assert(!PS::SpawnToolRequests::TakeCompleted(702,mapReceipt));
    // Height replies survive the production decoder without altering action receipts.
    const auto heightWire=json({{"Kind","mapground"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},
        {"Payload",{{"RequestId",uint64_t(703)},{"Generation",HostGeneration},{"GroundAvailable",true},{"Location",{198000,91450,1234}}}}}).dump();
    assert(ToolsPush(&state,heightWire.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    travelRevision=0;
    assert(PS::SpawnToolRequests::RemoteReadQuick(travelRevision,-1,travelSnapshot));
    assert(travelSnapshot.at("MapGround").at("Location")[2]==1234&&!PS::SpawnToolRequests::TakeCompleted(703,mapReceipt));
    const auto placement=PS::MapSpawnRequest::Build(projection,projection.ToWorld({0.25,0.75}),{{"Key","tree"},{"Class","/Game/Tree.Tree_C"},{"Type","Resource"}},"player",true,1,0,701);
    assert(PS::SpawnToolRequests::TrySubmit(placement)&&LastSubmission==placement);
    AuthoringHost.Configure(true,true);assert(PS::AuthoringClient::Activate());AuthoringHost.TickActivation();PS::AuthoringClient::Poll(true);
    assert(PS::AuthoringClient::Submit("Catalogue"));auto authoring=AuthoringHost.Take();assert(authoring);
    AuthoringHost.Complete((*authoring)["RequestId"],{{"Large",std::string(300000,'z')}},"Large snapshot");
    PS::AuthoringClient::Poll(true);assert(PS::AuthoringClient::Result("Catalogue")["Large"].get<std::string>().size()==300000);
    PS::AuthoringClient::Clear();
    // Production push/read route retains authoritative display roles, including
    // an empty roster. No role update is a mutation receipt or a client grant.
    const auto roleRow=json{{"Path","/Player/Admin"},{"Guid",std::string(32,'A')},{"Name","Same"},
        {"RoleStatus",PS::PlayerStatus::Describe(1,{1,2,4,8,128},false)}};
    const auto pushRoles=[&](const json& rows){const auto wire=json{{"Kind","catalog"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},
        {"Payload",{{"Players",rows},{"PlayerRoles",PS::PlayerStatus::Snapshot(rows,HostGeneration)}}}}.dump();
        assert(ToolsPush(&state,wire.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);PS::SpawnToolsPanel::RefreshSnapshot(-1);};
    pushRoles(json::array({roleRow}));assert(PS::PlayerBadges::Find(roleRow)->at("Role")=="admin");
    pushRoles(json::array());assert(!PS::PlayerBadges::Find(roleRow));assert(PS::SpawnToolsPanel::UiSnapshot["Players"].empty());
    ThrowHost=true;json ignored;assert(!CallCore(&state,{{"Action","Read"}},ignored));
    PS::ItemIconCache::ClearRemote();ClearRemote();PS::SpawnToolRequests::ClearRemote();
}
