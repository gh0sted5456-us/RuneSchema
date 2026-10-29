#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include "Core/NetworkIdentityPlan.h"
#include "nlohmann/json.hpp"

namespace PS::BuildingNetworkGuard {
struct Registry {
    std::size_t NativeCount=0,Count=0;
    std::string NativeFingerprint,Fingerprint;
    bool Valid() const {
        const auto hash=[](const std::string& value){return value.size()==16&&std::ranges::all_of(value,[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});};
        return NativeCount>0&&NativeCount<=Count&&Count<=NetworkIdentityPlan::Capacity
            &&hash(NativeFingerprint)&&hash(Fingerprint);
    }
};
inline nlohmann::json Encode(const Registry& registry) {
    if(!registry.Valid())return nlohmann::json::object();
    return {{"version",1},{"nativeCount",registry.NativeCount},{"count",registry.Count},
        {"nativeFingerprint",registry.NativeFingerprint},{"fingerprint",registry.Fingerprint}};
}
inline Registry Decode(const nlohmann::json& value) {
    if(value.is_object()&&value.empty())return {};
    if(!value.is_object()||value.size()!=5||value.value("version",0)!=1
        ||!value.at("nativeCount").is_number_integer()||!value.at("count").is_number_integer())
        throw std::runtime_error("Building network identity contract differs");
    Registry result{value.at("nativeCount").get<std::size_t>(),value.at("count").get<std::size_t>(),
        value.at("nativeFingerprint").get<std::string>(),value.at("fingerprint").get<std::string>()};
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
    return "building_ids_different";
}
}
