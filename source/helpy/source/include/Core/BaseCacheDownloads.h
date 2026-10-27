#pragma once
#include <nlohmann/json.hpp>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
namespace PS::BaseCacheDownloads {
inline bool Runtime(const std::string& value){return value=="Steam"||value=="Steam / Epic"||value=="Steam/Epic"||value=="Epic Games"||value=="Game Pass"||value=="Dedicated server";}
// Candidate selection is not content certification. Client imports still verify
// native-content SHA-256; dedicated servers use the existing display-only policy.
inline std::vector<nlohmann::json> Candidates(const nlohmann::json& packs,const std::map<std::string,std::string>& active,
    const std::set<std::string>& nativePlugins,const std::string& runtime,const std::string& version,const std::string& locale,bool bundle){
    std::vector<std::pair<int,nlohmann::json>> ranked;
    if(!Runtime(runtime))return {};
    const std::set<std::string> knownNative{"game","engine","agility","dowdunreach","fishing","futuremajorversion","scornedwilderness","umbralsands"};
    for(const auto& pack:packs){
        if(!pack.is_object()||pack.value("GameVersion",std::string{}).empty()||!Runtime(pack.value("Runtime",std::string{})))continue;
        const auto sources=pack.find("Sources");if(sources==pack.end()||!sources->is_object()||sources->empty()||sources->size()>512||!sources->contains("game"))continue;
        bool native=true;
        for(const auto& [group,key]:sources->items())if((!knownNative.contains(group)&&!nativePlugins.contains(group))||!key.is_string()
            ||key.get_ref<const std::string&>().size()!=64||key.get_ref<const std::string&>().find_first_not_of("0123456789abcdef")!=std::string::npos){native=false;break;}
        if(!native)continue;
        // Inactive official plugin groups in a published native pack are skipped
        // by import. They must not prevent the base-game part from downloading.
        int score=pack.value("GameVersion",std::string{})==version?0:4;
        score+=pack.value("Runtime",std::string{})==runtime?0:1;
        if(bundle&&pack.value("Locale",std::string{})!=locale)score+=2;
        ranked.emplace_back(score,pack);
    }
    std::stable_sort(ranked.begin(),ranked.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    std::vector<nlohmann::json> result;for(auto& [score,pack]:ranked)result.push_back(std::move(pack));return result;
}
}
