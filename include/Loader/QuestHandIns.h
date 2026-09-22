#pragma once
#include "QuestDefinition.h"
#include <limits>
#include <map>

namespace DragonWilds::Quests {
struct HandInPlan {
    std::map<std::string,int> Amounts;
    std::vector<size_t> Objectives;
};

// Select independently eligible objectives; other areas and unfinished kills
// must not prevent committing this area's item objectives.
template<class Counter,class InArea,class Inventory>
HandInPlan SelectHandIns(const std::vector<Definition>& objectives,
    Counter counter,InArea inArea,Inventory inventory) {
    HandInPlan result;
    for(bool optional:{false,true})for(size_t i=0;i<objectives.size();++i) {
        const auto& objective=objectives[i];
        if(objective.Optional!=optional || objective.Kill || objective.Acquire)continue;
        const auto completed=counter(i);
        if(completed<0 || completed>objective.Required.Count)
            throw std::runtime_error("Quest hand-in counter is outside its requirement");
        if(completed==objective.Required.Count || !inArea(objective))continue;
        const auto existing=result.Amounts.find(objective.Required.Item);
        const int reserved=existing==result.Amounts.end()?0:existing->second;
        if(objective.Required.Count>std::numeric_limits<int>::max()-reserved)
            throw std::runtime_error("Quest hand-in item total exceeds supported bounds");
        const int requested=reserved+objective.Required.Count;
        if(inventory(objective)<requested)continue;
        result.Amounts[objective.Required.Item]=requested;
        result.Objectives.push_back(i);
    }
    return result;
}
}
