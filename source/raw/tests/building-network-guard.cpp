#include "Core/BuildingNetworkGuard.h"
#include <iostream>
namespace B=PS::BuildingNetworkGuard;
void Need(bool value){if(!value)throw std::runtime_error("building identity check failed");}
template<class F>void Reject(F run){try{run();}catch(const std::exception&){return;}throw std::runtime_error("invalid building identity accepted");}
int main(){
    using namespace std::chrono_literals;
    const auto start=B::HandshakeRetry::Clock::time_point{}+1s;
    B::HandshakeRetry retry;Need(!retry.Due(start));
    retry.Begin();Need(retry.Due(start));retry.Attempt(start);
    Need(!retry.Due(start+1999ms)&&retry.Due(start+2s));
    Need(!retry.Complete(1,true)); // No owned RPC component was available to send.
    retry.Attempt(start+2s);retry.SentReady=false;retry.Revision=2;
    Need(!retry.Complete(2,true)); // The client's building list was not ready.
    retry.Attempt(start+4s);retry.SentReady=true;retry.Revision=3;
    Need(!retry.Complete(3,false)&&!retry.Complete(2,true)); // Empty server list and stale replies cannot finish the handshake.
    Need(retry.Complete(3,true)&&!retry.Due(start+1h)&&!retry.Complete(3,true));
    retry.Begin();Need(!retry.SentReady&&retry.Revision==0&&retry.Attempts==0&&retry.Due(start));
    for(unsigned i=0;i<B::HandshakeRetry::Limit;++i){const auto now=start+2s*i;Need(retry.Due(now));retry.Attempt(now);Need(!retry.Due(now+1s));}
    Need(retry.Attempts==16&&!retry.Due(start+1h));
    retry.SentReady=true;retry.Revision=99;Need(retry.Complete(99,true)); // A late matching reply still resolves it.
    retry.Begin();Need(retry.Due(start));retry={};Need(!retry.Due(start)&&!retry.Pending);
    Need(B::PlacementIndexContract("IntProperty",0,4,136)); // Observed Game Pass placement RPC.
    Need(B::PlacementIndexContract("UInt16Property",0,2,134));
    Need(!B::PlacementIndexContract("IntProperty",0,2,136));
    Need(!B::PlacementIndexContract("IntProperty",0,4,2));
    Need(!B::PlacementIndexContract("IntProperty",4,4,136));
    Need(!B::PlacementIndexContract("UInt16Property",0,2,1));
    Need(!B::PlacementIndexContract("FloatProperty",0,4,136));
    Need(!B::PlacementIndexContract("ObjectProperty",0,8,136));
    Need(!B::PlacementIndexContract("",-1,0,136));
    std::vector<std::string> rows(907);
    for(std::size_t i=0;i<rows.size();++i)rows[i]="building-"+std::to_string(i);
    const auto hash=PS::NetworkIdentityPlan::Fingerprint(std::span(rows).first(847));
    B::Registry client{847,907,hash,PS::NetworkIdentityPlan::Fingerprint(rows)};
    auto authority=client;
    Need(!B::Rejection(client,authority,874));
    // Matching authority prefix permits client-only buildings after the server range.
    auto extended=client;auto extension=rows;extension.push_back("client-only");
    extended.Count=extension.size();extended.Fingerprint=PS::NetworkIdentityPlan::Fingerprint(extension);
    extended.SharedCount=client.Count;extended.SharedFingerprint=client.Fingerprint;
    Need(!B::Rejection(extended,client,874)&&!B::Rejection(client,extended,874));
    Need(B::Rejection(extended,client,907)!=nullptr);
    Need(B::Encode(B::Decode(nlohmann::json::parse(B::Encode(extended).dump())))==B::Encode(extended));
    auto corrupt=extended;corrupt.SharedFingerprint="ffffffffffffffff";
    Need(B::Rejection(corrupt,client,874)!=nullptr);
    corrupt=extended;corrupt.SharedCount=extended.Count;Need(!corrupt.Valid());
    Need(B::Encode(B::Decode(B::Encode(client)))==B::Encode(client));
    // Missing 18 definitions shifted the same numeric trebuchet ID onto a Totem.
    auto oldRows=rows;oldRows.erase(oldRows.begin()+850,oldRows.begin()+868);
    authority={847,889,hash,PS::NetworkIdentityPlan::Fingerprint(oldRows)};
    Need(B::Rejection(client,authority,874)!=nullptr);
    Need(B::Rejection(authority,client,874)!=nullptr);
    Need(B::Rejection(client,authority,906)!=nullptr);
    Need(!B::Rejection(client,authority,846));
    Need(B::Rejection(client,authority,847)!=nullptr);
    Need(B::Rejection(client,{},874)!=nullptr);
    Need(B::Rejection(client,{},663)!=nullptr); // Observed native placement waits too; no unsafe bypass.
    Need(B::Rejection({},client,874)!=nullptr);
    authority=client;std::swap(rows[871],rows[874]);
    authority.Fingerprint=PS::NetworkIdentityPlan::Fingerprint(rows);
    Need(B::Rejection(client,authority,874)!=nullptr); // Equal counts are insufficient.
    authority=client;authority.NativeFingerprint="ffffffffffffffff";
    Need(B::Rejection(client,authority,10)!=nullptr);
    Need(B::Rejection(client,client,907)!=nullptr);
    Need(B::Rejection(client,client,static_cast<std::size_t>(-1))!=nullptr);
    for(const auto& bad:{nlohmann::json{{"version",1}},nlohmann::json("invalid"),nlohmann::json::array({1}),nlohmann::json::array(),nlohmann::json(nullptr)})Reject([&]{B::Decode(bad);});
    auto bad=B::Encode(client);bad["count"]=-1;Reject([&]{B::Decode(bad);});
    bad=B::Encode(client);bad["nativeCount"]=908;Reject([&]{B::Decode(bad);});
    bad=B::Encode(client);bad["fingerprint"]="";Reject([&]{B::Decode(bad);});
    bad=B::Encode(extended);bad["sharedCount"]=-1;Reject([&]{B::Decode(bad);});
    bad=B::Encode(extended);bad.erase("sharedFingerprint");Reject([&]{B::Decode(bad);});
    bad=B::Encode(client);bad["version"]=2;Reject([&]{B::Decode(bad);});
    Need(!B::Decode(nlohmann::json::object()).Valid());
    std::cout<<"building-network-guard: PASS\n";
}
