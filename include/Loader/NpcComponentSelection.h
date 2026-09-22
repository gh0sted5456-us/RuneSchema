#pragma once
#include <vector>
#include <stdexcept>
namespace DragonWilds::NpcComponents {
template<class T> struct Candidate {T* Object;bool Owned,Live,Preferred;};
template<class T> T* Select(const std::vector<Candidate<T>>& candidates) {
    T* preferred=nullptr;T* unique=nullptr;size_t count=0;
    for(const auto& candidate:candidates) {
        if(!candidate.Object || !candidate.Owned || !candidate.Live)continue;
        unique=candidate.Object;++count;
        if(candidate.Preferred) {
            if(preferred)throw std::runtime_error("Multiple owned resource visual components have the preferred name");
            preferred=candidate.Object;
        }
    }
    if(preferred)return preferred;
    if(count>1)throw std::runtime_error("Resource NPC static mesh components are ambiguous");
    return unique;
}
}
