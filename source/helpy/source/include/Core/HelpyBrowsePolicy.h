#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
namespace PS::HelpyBrowsePolicy {
struct Policy {
    bool CacheFirst=true,PassiveRefresh=true;
    bool UseBundle(bool nativeReady,bool nativeEmpty)const{return CacheFirst||!nativeReady||nativeEmpty;}
    bool operator==(const Policy&)const=default;
};
inline Policy Read(const nlohmann::json& document){
    if(!document.is_object()||(document.contains("passiveRefresh")&&!document["passiveRefresh"].is_boolean()))
        throw std::runtime_error("[RS-CFG-001] Cache preferences must be an object with a boolean passiveRefresh.");
    if(document.contains("loadOrder")&&!document["loadOrder"].is_string())
        throw std::runtime_error("[RS-CACHE-013] Content loadOrder must be cacheFirst or gameFirst.");
    const auto mode=document.value("loadOrder",std::string("cacheFirst"));
    if(mode!="cacheFirst"&&mode!="gameFirst")throw std::runtime_error("[RS-CACHE-013] Unsupported content load order; use cacheFirst or gameFirst.");
    return {mode=="cacheFirst",document.value("passiveRefresh",true)};
}
}
