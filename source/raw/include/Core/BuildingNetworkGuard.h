#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include "Core/NetworkIdentityPlan.h"
#include "nlohmann/json.hpp"

namespace PS::BuildingNetworkGuard {
struct HandshakeRetry {
    using Clock=std::chrono::steady_clock;
    static constexpr unsigned Limit=16;
    bool Pending=false,SentReady=false;
    unsigned Attempts=0;
    int64_t Revision=0;
    Clock::time_point Next{};
    void Begin(){*this={};Pending=true;}
    bool Due(Clock::time_point now)const{return Pending&&Attempts<Limit&&now>=Next;}
    void Attempt(Clock::time_point now){Attempts=std::min(Attempts+1,Limit);Next=now+std::chrono::seconds(2);SentReady=false;Revision=0;}
    bool Complete(int64_t revision,bool peerReady){if(!Pending||!SentReady||!Revision||revision!=Revision||!peerReady)return false;Pending=false;return true;}
};
inline bool PlacementIndexContract(std::string_view type,int offset,int bytes,int parameters){
    return offset==0&&parameters>=bytes
        &&((type=="UInt16Property"&&bytes==2)||(type=="IntProperty"&&bytes==4));
}
struct Registry {
    std::size_t NativeCount=0,Count=0;
    std::string NativeFingerprint,Fingerprint;
    std::size_t SharedCount=0;
    std::string SharedFingerprint;
    bool Valid() const {
        const auto hash=[](const std::string& value){return value.size()==16&&std::ranges::all_of(value,[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});};
        return NativeCount>0&&NativeCount<=Count&&Count<=NetworkIdentityPlan::Capacity
            &&hash(NativeFingerprint)&&hash(Fingerprint)
            &&((SharedCount==0&&SharedFingerprint.empty())
                ||(SharedCount>=NativeCount&&SharedCount<=Count&&hash(SharedFingerprint)
                    &&(SharedCount!=Count||SharedFingerprint==Fingerprint)));
    }
};
inline nlohmann::json Encode(const Registry& registry) {
    if(!registry.Valid())return nlohmann::json::object();
    nlohmann::json result={{"version",1},{"nativeCount",registry.NativeCount},{"count",registry.Count},
        {"nativeFingerprint",registry.NativeFingerprint},{"fingerprint",registry.Fingerprint}};
    if(registry.SharedCount){result["sharedCount"]=registry.SharedCount;result["sharedFingerprint"]=registry.SharedFingerprint;}
    return result;
}
inline Registry Decode(const nlohmann::json& value) {
    if(value.is_object()&&value.empty())return {};
    if(!value.is_object()||(value.size()!=5&&value.size()!=7)||value.value("version",0)!=1
        ||!value.at("nativeCount").is_number_integer()||!value.at("count").is_number_integer())
        throw std::runtime_error("Building network identity contract differs");
    Registry result{value.at("nativeCount").get<std::size_t>(),value.at("count").get<std::size_t>(),
        value.at("nativeFingerprint").get<std::string>(),value.at("fingerprint").get<std::string>()};
    if(value.size()==7){
        if(!value.contains("sharedCount")||!value.at("sharedCount").is_number_unsigned()
            ||!value.contains("sharedFingerprint")||!value.at("sharedFingerprint").is_string())throw std::runtime_error("Invalid shared building prefix");
        result.SharedCount=value.at("sharedCount").get<std::size_t>();result.SharedFingerprint=value.at("sharedFingerprint").get<std::string>();
        if(!result.SharedCount)throw std::runtime_error("Empty shared building prefix");
    }
    if(!result.Valid())throw std::runtime_error("Building network identity is invalid");
    return result;
}
inline const char* Rejection(const Registry& local,const Registry& peer,std::size_t index) {
    if(!local.Valid()||!peer.Valid())return "building_ids_unverified";
    if(index>=local.Count||index>=peer.Count)return "building_id_unavailable";
    if(index<local.NativeCount&&index<peer.NativeCount
        &&local.NativeCount==peer.NativeCount&&local.NativeFingerprint==peer.NativeFingerprint)return nullptr;
    if(local.Count==peer.Count&&local.NativeCount==peer.NativeCount
        &&local.NativeFingerprint==peer.NativeFingerprint&&local.Fingerprint==peer.Fingerprint)return nullptr;
    if(local.NativeCount==peer.NativeCount&&local.NativeFingerprint==peer.NativeFingerprint
        &&((local.Count>peer.Count&&local.SharedCount==peer.Count&&local.SharedFingerprint==peer.Fingerprint)
            ||(peer.Count>local.Count&&peer.SharedCount==local.Count&&peer.SharedFingerprint==local.Fingerprint)))return nullptr;
    return "building_ids_different";
}
}
