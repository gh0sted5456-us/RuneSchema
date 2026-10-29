#include "Core/BuildingNetworkGuard.h"
#include <iostream>
namespace B=PS::BuildingNetworkGuard;
void Need(bool value){if(!value)throw std::runtime_error("building identity check failed");}
template<class F>void Reject(F run){try{run();}catch(const std::exception&){return;}throw std::runtime_error("invalid building identity accepted");}
int main(){
    std::vector<std::string> rows(907);
    for(std::size_t i=0;i<rows.size();++i)rows[i]="building-"+std::to_string(i);
    const auto hash=PS::NetworkIdentityPlan::Fingerprint(std::span(rows).first(847));
    B::Registry client{847,907,hash,PS::NetworkIdentityPlan::Fingerprint(rows)};
    auto authority=client;
    Need(!B::Rejection(client,authority,874));
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
    Need(B::Rejection({},client,874)!=nullptr);
    authority=client;std::swap(rows[871],rows[874]);
    authority.Fingerprint=PS::NetworkIdentityPlan::Fingerprint(rows);
    Need(B::Rejection(client,authority,874)!=nullptr); // Equal counts are insufficient.
    authority=client;authority.NativeFingerprint="ffffffffffffffff";
    Need(B::Rejection(client,authority,10)!=nullptr);
    Need(B::Rejection(client,client,907)!=nullptr);
    for(const auto& bad:{nlohmann::json{{"version",1}},nlohmann::json("invalid"),nlohmann::json::array({1}),nlohmann::json::array(),nlohmann::json(nullptr)})Reject([&]{B::Decode(bad);});
    auto bad=B::Encode(client);bad["count"]=-1;Reject([&]{B::Decode(bad);});
    bad=B::Encode(client);bad["nativeCount"]=908;Reject([&]{B::Decode(bad);});
    bad=B::Encode(client);bad["fingerprint"]="";Reject([&]{B::Decode(bad);});
    bad=B::Encode(client);bad["version"]=2;Reject([&]{B::Decode(bad);});
    Need(!B::Decode(nlohmann::json::object()).Valid());
    std::cout<<"building-network-guard: PASS\n";
}
