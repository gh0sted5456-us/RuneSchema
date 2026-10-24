#pragma once
#include "Core/CookedPakRegistryManifest.h"
#include "Core/NetworkIdentityPlan.h"
namespace PS::CookedEnumContract {
using Symbols=std::vector<std::pair<std::string,int64_t>>;
inline std::string Fingerprint(const std::string& owner,const std::string& path,const Symbols& symbols){
    if(!CookedPakRegistryManifest::Detail::OwnedObjectPath(path,owner)||symbols.empty()||symbols.size()>256)
        throw std::runtime_error("Cooked enum requires an owned object path and 1..256 cooked symbols");
    std::map<std::string,int64_t> names;std::set<int64_t> values;
    for(const auto& [name,value]:symbols){
        if(name.empty()||name.size()>256||std::any_of(name.begin(),name.end(),[](unsigned char c){return c<33||c>126;})
            ||value<0||value>255||!names.emplace(CookedPakRegistryManifest::Detail::Fold(name),value).second||!values.insert(value).second)
            throw std::runtime_error("Cooked enum symbols have invalid, duplicate or non-byte identities");
    }
    std::vector<std::string> rows{CookedPakRegistryManifest::Detail::Fold(owner),CookedPakRegistryManifest::Detail::Fold(path)};
    for(const auto& [name,value]:names)rows.push_back(name+"="+std::to_string(value));
    return NetworkIdentityPlan::Fingerprint(rows);
}
inline void RequireLegacyAppendSafe(const std::string& enumPath){
    if(CookedPakRegistryManifest::Detail::Fold(enumPath)=="/script/dominion.eskill")
        throw std::runtime_error("ESkill extension is disabled: compiled MAX, switch, array, UI and network consumers are not verified; do not borrow a native skill slot");
}
}
