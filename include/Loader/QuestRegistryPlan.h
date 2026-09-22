#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>

namespace DragonWilds::QuestRegistry {
struct Plan { uint16_t Id; bool Append; };
inline Plan NetworkPlan(uintptr_t quest,std::span<const uintptr_t> entries,std::optional<uint16_t> reverse) {
    if(!quest || entries.size()>65535)throw std::runtime_error("Invalid quest network registry");
    std::optional<size_t> existing;
    for(size_t i=0;i<entries.size();++i)if(entries[i]==quest) {
        if(existing)throw std::runtime_error("Quest has duplicate network entries");
        existing=i;
    }
    if(reverse) {
        if(!existing || *reverse!=*existing)throw std::runtime_error("Quest network reverse mapping is inconsistent");
        return {*reverse,false};
    }
    if(existing)throw std::runtime_error("Quest network reverse mapping is missing");
    if(entries.size()>=65535)throw std::runtime_error("Quest network registry is full");
    return {static_cast<uint16_t>(entries.size()),true};
}
}
