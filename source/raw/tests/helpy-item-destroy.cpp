#include "Core/HelpyItemDestroy.h"
#include <iostream>
#include <set>
#include <stdexcept>
// Helpy's Destroy button: request building, confirmation, authority and the bounded result.
namespace D=PS::HelpyItemDestroy;using Json=nlohmann::json;
static void Check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
template<class F>static bool Refused(F f){try{f();return false;}catch(const std::exception&){return true;}}
int main(){try{
    // Request ids: lower-case v4 UUIDs that item.destroy accepts, different every time.
    std::set<std::string> ids;
    for(int i=0;i<64;++i){const auto id=D::NewRequestId();Check(id.size()==36&&id[14]=='4'&&std::string("89ab").find(id[19])!=std::string::npos,"v4 UUID");ids.insert(id);}
    Check(ids.size()==64,"fresh ids");
    // Instance: the panel's four numbers become the 32 hex digits the snapshot service compares.
    Check(D::InstanceText(Json::array({1,0xABCDEF01u,2,0xFFFFFFFFu}))=="00000001ABCDEF0100000002FFFFFFFF","instance text");
    for(const auto& bad:std::vector<Json>{Json::array({0,0,0,0}),Json::array({1,2,3}),Json::array({-1,0,0,1}),Json::array({1,2,3,4294967296ull}),Json("A1"),Json()})
        Check(Refused([&]{D::InstanceText(bad);}),"bad instance refused");
    const std::string guid="5A3F0C21B6D84E7F9A1C2B3D4E5F6071",path="/Game/Player/BP_PlayerController_C_0";
    const Json row{{"Slot",12},{"Instance",Json::array({7,8,9,10})},{"Path","/Game/Items/Ore/Copper.Copper"},{"Count",25},{"Name","Copper ore"}};
    const auto id=D::NewRequestId();
    const auto request=D::Build(row,path,"{5a3f0c21-b6d8-4e7f-9a1c-2b3d4e5f6071}",false,42,id);
    Check(request["Action"]=="DestroyItem"&&request["Confirmed"]==true&&request["PlayerGuid"]==guid&&request["Player"]==path&&request["Generation"]==42,"request envelope");
    Check(request["Destroy"]["Scope"]=="Inventory"&&request["Destroy"]["Slot"]==12&&request["Destroy"]["Count"]==25&&request["Destroy"]["Request"]==id
        &&request["Destroy"]["Instance"]=="0000000700000008000000090000000A","destroy body");
    Check(D::Build(row,path,guid,true,42,id)["Destroy"]["Scope"]=="Personal","personal storage tab");
    // Build refuses what the game would refuse.
    for(const auto& patch:std::vector<Json>{{{"Equipped",true}},{{"Slot",-1}},{{"Count",0}},{{"Path","Game/x"}},{{"Instance",Json::array({0,0,0,0})}}}){
        auto bad=row;bad.update(patch);Check(Refused([&]{D::Build(bad,path,guid,false,1,id);}),"bad row refused");}
    Check(Refused([&]{D::Build(row,path,"",false,1,id);})&&Refused([&]{D::Build(row,path,guid,false,1,"not-a-uuid");}),"identity required");
    // The game reads only a confirmed request for the selected character.
    const auto body=D::Body(request);Check(body.Exact.Player==guid&&body.Count==25&&body.Id==id,"body");
    {auto unconfirmed=request;unconfirmed["Confirmed"]=false;Check(Refused([&]{D::Body(unconfirmed);}),"confirmation required");
        unconfirmed.erase("Confirmed");Check(Refused([&]{D::Body(unconfirmed);}),"confirmation required when absent");}
    {auto other=request;other["PlayerGuid"]="ABCDEFABCDEFABCDEFABCDEFABCDEF01";Check(Refused([&]{D::Body(other);}),"target must be the selected character");}
    {auto loadout=request;loadout["Destroy"]["Scope"]="Loadout";Check(Refused([&]{D::Body(loadout);}),"equipped items refused");}
    {auto wrong=request;wrong["Action"]="GiveItems";Check(Refused([&]{D::Body(wrong);}),"only DestroyItem");}
    // Authority. This game (single player, host, server): own character gated, others are the host's call.
    Check(D::Allow({false,false,false,false,true})==true,"local own character is gated");
    Check(D::Allow({false,false,false,false,false})==false,"host acting on another player is not gated");
    // Through a server: saved access or allowlist, own character only, always gated.
    Check(Refused([&]{D::Allow({true,false,false,false,true});}),"no access refused");
    Check(Refused([&]{D::Allow({true,false,false,true,true});}),"a world PIN alone is refused");
    Check(D::Allow({true,true,false,false,true})&&D::Allow({true,false,true,false,true})&&D::Allow({true,true,false,true,true}),"saved access or allowlist, own character");
    Check(Refused([&]{D::Allow({true,true,false,false,false});})&&Refused([&]{D::Allow({true,false,true,false,false});}),"never another character through a server");
    try{D::Allow({true,false,false,true,true});}catch(const std::exception& error){Check(std::string(error.what()).find("PIN")!=std::string::npos,"the PIN refusal explains itself");}
    // What Helpy shows: the outcome only.
    const Json reply{{"Schema","runeschema.item-destroy/1"},{"State","Confirmed"},{"Ok",true},{"Request",id},{"Snapshot",{{"Fields",Json::array()},{"Digest","sha256:00"}}},{"Observed",{{"InMemory",true}}}};
    const auto shown=D::Bounded(reply,body);
    Check(!shown.contains("Snapshot")&&!shown.contains("Observed")&&shown["State"]=="Confirmed"&&shown["Ok"]==true&&shown["Count"]==25&&shown["Slot"]==12&&shown["Item"]=="/Game/Items/Ore/Copper.Copper","bounded result");
    Check(shown.dump().size()<1024,"small enough for the server result envelope");
    const auto refused=D::Bounded({{"State","Refused"},{"Ok",false},{"Phase","ExactItem"},{"Error",std::string(500,'x')}},body);
    Check(refused["Error"].get<std::string>().size()==300&&refused["Phase"]=="ExactItem","refusal bounded");
    Check(D::Status(shown).starts_with("Destroyed")&&D::Status(refused).starts_with("Not destroyed")&&D::Status({{"State","Retained"}}).find("kept")!=std::string::npos,"status lines");
    Check(D::Status({{"State","Unknown"}}).find("will not retry")!=std::string::npos&&D::Status(Json::object()).find("unknown")!=std::string::npos,"unknown never invites a retry");
    Check(D::Status(shown).find("save")==std::string::npos,"Confirmed is not described as saved");
    std::cout<<"Helpy item destroy passed: ids, instance, request, confirmation, authority, bounded result.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
