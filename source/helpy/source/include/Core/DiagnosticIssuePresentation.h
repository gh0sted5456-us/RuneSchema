#pragma once

#include "Core/DiagnosticIssueFilters.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PS::DiagnosticIssuePresentation {
using Group=DiagnosticCenterModel::Group;
using Store=DiagnosticCenterModel::Store;

inline std::unordered_map<std::uint64_t,std::string> LatestOperationKeys(const Store& store){
    std::unordered_map<std::uint64_t,std::string> keys;
    keys.reserve(store.Groups().size());
    for(const auto& event:store.Events()){
        if(!event.OperationId.empty()&&!event.ModId.empty())
            keys[event.GroupId]=event.ModId+"\x1f"+event.OperationId;
        else keys.erase(event.GroupId);
    }
    return keys;
}
struct Row {
    const Group* Issue=nullptr;
    std::size_t Related=0;
    bool Child=false;
};
inline std::vector<Row> Rows(const Store& store,const std::vector<const Group*>& sorted,
                             bool cluster,const std::unordered_set<std::uint64_t>& expanded){
    std::vector<Row> rows;
    rows.reserve(sorted.size());
    if(!cluster){for(const auto* group:sorted)rows.push_back({group,0,false});return rows;}
    const auto keys=LatestOperationKeys(store);
    std::unordered_map<std::string,std::vector<const Group*>> families;
    for(const auto* group:sorted){
        if(const auto it=keys.find(group->Id);it!=keys.end())
            families[it->second].push_back(group);
    }
    std::unordered_set<std::uint64_t> emitted;
    for(const auto* group:sorted){
        if(!emitted.insert(group->Id).second)continue;
        const auto key=keys.find(group->Id);
        if(key==keys.end()){rows.push_back({group,0,false});continue;}
        const auto family=families.find(key->second);
        if(family==families.end()||family->second.size()<2){rows.push_back({group,0,false});continue;}
        rows.push_back({group,family->second.size()-1,false});
        for(std::size_t i=1;i<family->second.size();++i){
            const auto* child=family->second[i];emitted.insert(child->Id);
            if(expanded.contains(group->Id))rows.push_back({child,0,true});
        }
    }
    return rows;
}
inline std::vector<std::uint64_t> Related(const Store& store,std::uint64_t selected,std::size_t limit=16){
    const auto keys=LatestOperationKeys(store);
    const auto own=keys.find(selected);
    if(own==keys.end())return {};
    std::vector<std::uint64_t> result;
    for(const auto& group:store.Groups()){
        if(group.Id==selected)continue;
        const auto candidate=keys.find(group.Id);
        if(candidate!=keys.end()&&candidate->second==own->second){
            result.push_back(group.Id);
            if(result.size()>=limit)break;
        }
    }
    return result;
}
}
