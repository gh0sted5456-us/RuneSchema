#include "Core/PluginServerRequest.h"
#include <iostream>
#include <stdexcept>
namespace P=PS::PluginServerRequest;using Json=nlohmann::json;
static void Check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
template<class F>static bool Refused(F f){try{f();return false;}catch(const std::exception&){return true;}}
int main(){try{
    const Json input{{"Action","VaultWithdraw"},{"Request","12345678-1234-4123-8123-123456789abc"},{"Body",{{"Transfer","signed-ticket"}}}};
    const auto request=P::Parse(input);
    Check(request.Action=="VaultWithdraw"&&request.Body["Transfer"]=="signed-ticket","valid request");
    Check(P::Parse({{"Action","Ping"},{"Request","12345678-1234-4123-8123-123456789abc"}}).Body==Json::object(),"body optional");
    for(const auto& patch:std::vector<Json>{{{"Action",""}},{{"Action","has space"}},{{"Action",std::string(65,'a')}},{{"Request","12345678-1234-4123-8123-123456789ABC"}},
        {{"Request","00000000-0000-0000-0000-000000000000"}},{{"Body","text"}},{{"Body",{{"x",std::string(3000,'y')}}}},{{"From",{{"Character","forged"}}}}}){
        auto bad=input;bad.update(patch);Check(Refused([&]{P::Parse(bad);}),"bad request refused");}
    // The sender comes from RuneSchema; a Body field named From is just data.
    auto spoof=input;spoof["Body"]["From"]={{"Character","OTHER"}};
    const auto call=P::ServerCall("GrandExchange",P::Parse(spoof),"5A3F0C21B6D84E7F9A1C2B3D4E5F6071",true);
    Check(call["From"]["Character"]=="5A3F0C21B6D84E7F9A1C2B3D4E5F6071"&&call["From"]["Remote"]==true&&call["Plugin"]=="GrandExchange","server sees the real sender");
    Check(P::ServerService("GrandExchange")=="GrandExchange.server"&&P::ResultService("GrandExchange")=="GrandExchange.server-result","service names");
    const auto ok=P::Result("VaultWithdraw",request.Id,true,{{"State","Confirmed"}});Check(ok["Ok"]==true&&ok["Result"]["State"]=="Confirmed","ok result");
    const auto big=P::Result("VaultWithdraw",request.Id,true,{{"x",std::string(4000,'z')}});Check(big["Ok"]==false&&big.dump().size()<P::MaxResultBytes,"oversized result refused");
    const auto no=P::Result("VaultWithdraw",request.Id,false,Json("GrandExchange is not installed on this server."));Check(no["Ok"]==false&&no["Error"].get<std::string>().find("not installed")!=std::string::npos,"error result");
    P::Pending pending;const auto t0=P::Pending::Clock::now();
    Check(pending.Add(7,{"GrandExchange","VaultWithdraw",request.Id,t0})&&!pending.Add(7,{"GrandExchange","x",request.Id,t0}),"one entry per revision");
    Check(!pending.Take(7,"OtherMod",request.Id,t0)&&!pending.Take(7,"GrandExchange","12345678-1234-4123-8123-000000000000",t0),"wrong plugin or request refused");
    Check(pending.Take(7,"GrandExchange",request.Id,t0)->Action=="VaultWithdraw"&&!pending.Take(7,"GrandExchange",request.Id,t0),"taken once");
    Check(pending.Add(8,{"GrandExchange","a",request.Id,t0})&&!pending.Take(8,"GrandExchange",request.Id,t0+P::Expiry+std::chrono::seconds(1)),"expired answers dropped");
    for(int64_t i=1;i<=64;++i)Check(pending.Add(100+i,{"GrandExchange","a",request.Id,t0}),"fill");
    Check(!pending.Add(999,{"GrandExchange","a",request.Id,t0})&&pending.Size()==64&&pending.Full(t0),"at most 64 outstanding");
    Check(!pending.Full(t0+P::Expiry+std::chrono::seconds(1))&&pending.Size()==0,"expired requests free their places");
    // The sender's own delivery state travels with the request; absent means an older sender.
    Check(!P::ParseDeliveries(nullptr).Known,"absent delivery state is unknown");
    {const Json opened=P::DeliveriesJson({true,true,""});const auto open=P::ParseDeliveries(&opened);Check(open.Known&&open.Open,"open round-trips");
        const Json paused=P::DeliveriesJson({true,false,"othermod"});const auto closed=P::ParseDeliveries(&paused);Check(closed.Known&&!closed.Open&&closed.By=="othermod","pause round-trips");
        const Json blank=P::DeliveriesJson({true,false,""});Check(blank["By"]=="unknown","an unnamed block reads as unknown");}
    for(const auto& bad:std::vector<Json>{Json("open"),Json::object(),{{"Open","yes"}},{{"Open",true},{"By","x"}},{{"Open",false}},{{"Open",false},{"By","has space"}},
        {{"Open",false},{"By",std::string(41,'a')}},{{"Open",false},{"By","helpy"},{"Extra",1}}})
        Check(Refused([&]{P::ParseDeliveries(&bad);}),"malformed delivery state refused");
    {const auto wire=P::Wire(request,{true,false,"helpy"});Check(wire.size()==3&&wire["Deliveries"]["By"]=="helpy"&&wire["Request"]==request.Id,"wire carries the sender's state");}
    std::cout<<"Plugin server request contract passed: envelope, real sender, delivery state, results, pending answers.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
