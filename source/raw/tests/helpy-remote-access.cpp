#include "Core/HelpyRemoteAccess.h"
#include "Core/HelpyRemoteResult.h"
#include "Generator/ToolRequest.h"
#include <stdexcept>
#include <iostream>
static void CheckAt(bool value,int line){if(!value)throw std::runtime_error("Remote world access regression at line "+std::to_string(line));}
#define Check(value) CheckAt((value),__LINE__)
int Run(){
    using S=PS::HelpyRemoteAccess::Sessions;using namespace std::chrono;
    S state;const auto now=S::Clock::now();const std::string pin="MyWorld-93728";
    Check(!S::ValidPin("1234")&&S::ValidPin(pin)&&!S::ValidPin(std::string(65,'a')));
    Check(!S::ValidPin("invalid pin")&&!S::ValidPin("invalid\nPIN"));
    Check(state.Authenticate("p1","w1",false,pin,pin,now)==S::Result::Disabled);
    Check(!state.Allowed("p1","w1",true,pin,now));
    Check(state.Authenticate("p1","w1",true,pin,"bad",now)==S::Result::Denied);
    Check(state.Authenticate("p1","w1",true,pin,pin,now)==S::Result::Granted);
    Check(state.Allowed("p1","w1",true,pin,now)&&!state.Allowed("p2","w1",true,pin,now));
    Check(!state.Allowed("p1","w2",true,pin,now));
    Check(state.Authenticate("p1","w2",true,pin,pin,now)==S::Result::Granted);
    Check(!state.Allowed("p1","w2",true,"Rotated-92738",now));
    for(int i=0;i<5;++i)Check(state.Authenticate("p1","w2",true,pin,"wrong",now)==S::Result::Denied);
    Check(state.Authenticate("p1","w2",true,pin,pin,now)==S::Result::RateLimited);
    state.Revoke("p1");Check(state.Authenticate("p1","w2",true,pin,pin,now)==S::Result::RateLimited);
    Check(state.Authenticate("p1","w2",true,pin,pin,now+minutes(1))==S::Result::Granted);
    Check(!state.Allowed("p1","w2",true,pin,now+hours(2)));
    Check(state.Authenticate("p1","w2",true,pin,pin,now+hours(2))==S::Result::Granted);
    state.Revoke("p1");Check(!state.Allowed("p1","w2",true,pin,now+hours(2)));
    state.Reset();
    for(unsigned i=0;i<256;++i)Check(state.Authenticate(std::to_string(i),"w2",true,pin,pin,now)==S::Result::Granted);
    Check(state.Authenticate("full","w2",true,pin,pin,now)==S::Result::Capacity);
    state.Reset();Check(!state.Allowed("0","w2",true,pin,now));
    for(unsigned i=0;i<60;++i)Check(state.Authenticate("bad-"+std::to_string(i),"w2",true,pin,"incorrect",now)==S::Result::Denied);
    Check(state.Authenticate("new-peer","w2",true,pin,pin,now)==S::Result::RateLimited);
    Check(state.Authenticate("new-peer","w2",true,pin,pin,now+minutes(1))==S::Result::Granted);
    PS::HelpyRemoteAccess::ClientAllowed=true;PS::HelpyRemoteAccess::ClientGeneration=12;
    Check(PS::HelpyRemoteAccess::ClientReady(12)&&!PS::HelpyRemoteAccess::ClientReady(13));
    Check(PS::HelpyRemoteAccess::SelfRequest("GiveItems","guid","guid","/Controller"));
    Check(PS::HelpyRemoteAccess::SelfRequest("MapTeleport","/Controller","guid","/Controller"));
    Check(!PS::HelpyRemoteAccess::SelfRequest("GiveItems","another-player","guid","/Controller"));
    Check(!PS::HelpyRemoteAccess::SelfRequest("Spawn","guid","guid","/Controller"));
    Check(!PS::HelpyRemoteAccess::SelfRequest("MapTeleport","","","/Controller"));
    {
        using namespace PS::HelpyRemoteAccess;
        std::map<std::string,std::string> authority{{ItemLane,"0123456789abcdef"}},peer=authority;
        Check(ItemIdsMatch(peer,authority,false));peer["unrelated-extra-lane"]="different";Check(ItemIdsMatch(peer,authority,false));
        peer[ItemLane]="fedcba9876543210";Check(!ItemIdsMatch(peer,authority,false));Check(ItemIdsMatch(peer,authority,true));
        Check(!ItemIdsMatch(peer,{},true));Check(!ItemIdsMatch({},authority,false));
    }
    {
        nlohmann::json result={{"Status","Confirmed ★"},{"GrantResults",nlohmann::json::array()}};
        for(int i=0;i<64;++i)result["GrantResults"].push_back({{"Item","/Game/"+std::string(300,'x')+std::to_string(i)},{"Count",1},{"State","Confirmed"}});
        const auto parts=PS::HelpyRemoteResult::Split(result);Check(parts.size()>1&&parts.size()<=PS::HelpyRemoteResult::MaxParts);
        PS::HelpyRemoteResult::Assembly assembly;std::optional<nlohmann::json> complete;
        for(auto i=parts.rbegin();i!=parts.rend();++i){Check(i->dump().size()<=4096);complete=assembly.Accept(*i);}
        Check(complete&&*complete==result);
        bool rejected=false;try{assembly.Accept({{"Part",0},{"Parts",129},{"Data","bad"}});}catch(...){rejected=true;}Check(rejected);
        Check(!assembly.Accept(parts[0]));Check(!assembly.Accept(parts[0]));
        auto conflict=parts[0];conflict["Data"]="changed";rejected=false;try{assembly.Accept(conflict);}catch(...){rejected=true;}Check(rejected);
        assembly.Reset();Check(!assembly.Accept(parts[0]));
    }
    {
        using Q=PS::SpawnToolRequests;Q::Available=true;unsigned replies=0;
        Check(Q::TrySubmit({{"Action","GiveItems"},{"_RequestId",uint64_t{321}}}));Check(Q::Take().has_value());
        Q::SetNetworkCompletion(321,[&](const auto& receipt){++replies;Check(receipt.at("Status")=="Confirmed");(void)Q::Read();});
        Q::Progress("Accepted, awaiting native result");Check(replies==0&&Q::IsWaiting());
        Q::Publish({{"Status","Confirmed"}});Check(replies==1&&!Q::IsWaiting());
        Q::Publish({{"Status","Unrelated update"}});Check(replies==1);
        Q::SetNetworkCompletion(322,[&](const auto&){++replies;});Q::Clear();
        Check(Q::TrySubmit({{"Action","GiveItems"},{"_RequestId",uint64_t{322}}}));Check(Q::Take().has_value());
        Q::Publish({{"Status","After world reset"}});Check(replies==1);
    }
    return 0;
}
int main(){try{return Run();}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
