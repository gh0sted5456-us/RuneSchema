#pragma once
#include <unordered_map>

namespace DragonWilds::DialogueLifecycle {
inline bool Matches(const void* expected,const void* current,int expectedSerial,int currentSerial,bool rooted,bool valid) {
    return expected && expected==current && expectedSerial==currentSerial && rooted && valid;
}
template<class Graphs,class Completions,class Session>
void Retire(Graphs& graphs,typename Graphs::iterator found,Completions& completions,Session& session) {
    const auto* expired=found->second.Graph;
    std::erase_if(completions,[&](const auto& entry){return entry.second.Graph==expired;});
    if(session.Graph==expired)session={};
    graphs.erase(found);
}

template<class Graphs,class Completions,class Sessions>
void RetireSessions(Graphs& graphs,typename Graphs::iterator found,Completions& completions,Sessions& sessions) {
    const auto* expired=found->second.Graph;
    std::erase_if(completions,[&](const auto& entry){return entry.second.Graph==expired;});
    std::erase_if(sessions,[&](const auto& entry){return entry.second->Graph==expired;});
    graphs.erase(found);
}
}
