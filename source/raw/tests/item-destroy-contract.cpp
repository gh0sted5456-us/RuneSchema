#include "Core/ItemDestroy.h"
#include <iostream>
#include <stdexcept>
namespace D=PS::ItemDestroy;namespace S=PS::ItemSnapshot;using Json=nlohmann::json;
static void Check(bool ok){if(!ok)throw std::runtime_error("item destroy contract regression");}
template<class F>static bool Refused(F f){try{f();return false;}catch(const std::exception&){return true;}}
int main(){try{
    const Json body{{"Player",std::string(32,'A')},{"Scope","Inventory"},{"Slot",3},{"Instance",std::string(32,'B')},{"Item","/Game/Fixture/Ore.Ore"},{"Count",5},{"Request","12345678-1234-4123-8123-123456789abc"}};
    const auto request=D::Parse(body);
    const auto snapshot=[&]{
        auto s=S::Build(request.Exact.Item,"/Script/Dominion.Item",request.Exact.Instance,{
          {"Count","Item","IntProperty","int32","int32",std::string(S::TextCodec),"5",false},
          {"Guid","Item","StructProperty","FGuid","/Script/CoreUObject.Guid",std::string(S::TextCodec),request.Exact.Instance,false},
          {"Durability","Item","IntProperty","int32","int32",std::string(S::TextCodec),"0",false}});
        s["Ok"]=true;s["Observed"]={{"Player",request.Exact.Player},{"Scope","Inventory"},{"Slot",3},{"InMemory",true}};return s;};
    for(const auto& patch:std::vector<Json>{{{"Scope","Loadout"}},{{"Scope","World"}},{{"Count",0}},{{"Count",5.0}},{{"Count",UINT64_MAX}},{{"Request","not-a-uuid"}},{{"Request","00000000-0000-0000-0000-000000000000"}},{{"Unexpected",true}}}){
        auto bad=body;bad.update(patch);Check(Refused([&]{D::Parse(bad);}));}
    auto personal=body;personal["Scope"]="Personal";Check(D::Parse(personal).Exact.Where==S::Scope::Personal);
    int reads=0,invokes=0,checks=0;D::Requests cache;
    auto prepare=[&]{++reads;return snapshot();};auto invoke=[&]{++invokes;};auto verify=[&]{++checks;return D::State::Confirmed;};
    Check(Refused([&]{cache.Run("Fixture",false,request,D::MinCapacity,prepare,invoke,verify);}));Check(reads==0&&invokes==0);
    try{cache.Run("Fixture",true,request,0,prepare,invoke,verify);Check(false);}catch(const D::BufferRequired& b){Check(b.Size==D::MinCapacity);}Check(reads==0&&invokes==0);
    auto first=cache.Run("Fixture",true,request,D::MinCapacity,prepare,invoke,verify);Check(first["State"]=="Confirmed"&&first["Snapshot"]==snapshot()&&invokes==1&&checks==1);
    auto replay=cache.Run("Fixture",true,request,D::MinCapacity,[]{throw std::runtime_error("item is already absent");return Json();},invoke,verify);Check(replay==first&&invokes==1&&reads==1);
    auto changed=request;changed.Count=4;Check(Refused([&]{cache.Run("Fixture",true,changed,D::MinCapacity,prepare,invoke,verify);}));Check(invokes==1);
    D::Requests failed;auto unknown=failed.Run("Fixture",true,request,D::MinCapacity,prepare,[&]{++invokes;throw std::runtime_error("after native call");},verify);
    Check(unknown["State"]=="Unknown");const auto attempted=invokes;
    Check(failed.Run("Fixture",true,request,D::MinCapacity,prepare,invoke,verify)==unknown&&invokes==attempted);
    D::Requests partial;auto shortStack=partial.Run("Fixture",true,request,D::MinCapacity,[&]{auto s=snapshot();s["Fields"][0]["Value"]="4";return s;},invoke,verify);Check(shortStack["State"]=="Refused"&&invokes==attempted);
    Check(partial.Run("Fixture",true,request,D::MinCapacity,prepare,invoke,verify)==shortStack&&invokes==attempted);
    D::Requests oversized;size_t needed=0;
    auto large=[&]{auto s=snapshot();s["Opaque"]=std::string(D::MinCapacity,'x');return s;};
    try{oversized.Run("Fixture",true,request,D::MinCapacity,large,invoke,verify);Check(false);}catch(const D::BufferRequired& b){needed=b.Size;Check(needed>D::MinCapacity);}Check(invokes==attempted);
    Check(oversized.Run("Fixture",true,request,needed,large,invoke,verify)["State"]=="Confirmed"&&invokes==attempted+1);
    D::Requests retained;Check(retained.Run("Fixture",true,request,D::MinCapacity,prepare,invoke,[]{return D::State::Retained;})["State"]=="Retained");
    D::Requests observations;Check(observations.Run("Fixture",true,request,D::MinCapacity,[&]{auto s=snapshot();s["Observed"]["Player"]=std::string(32,'C');return s;},invoke,verify)["State"]=="Refused");
    std::cout<<"Item destroy contract passed: whole stack, capability, bounded reply, replay, conflict, retained and unknown.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
