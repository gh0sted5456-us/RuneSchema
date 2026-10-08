#pragma once
#include <stdexcept>
#include <string_view>
namespace DragonWilds::Quests {
inline bool RootedQuestLeaseMatches(bool sameObject,bool samePath,bool rooted,bool valid,int savedSerial,int currentSerial) {
    return sameObject && samePath && rooted && valid && savedSerial>=0 && currentSerial>=0
        && (savedSerial==currentSerial || savedSerial==0);
}
inline bool RestoreUnregisteredIdentity(std::string_view actual,std::string_view expected,bool registered) {
    if(expected.empty())throw std::runtime_error("Expected quest identity is empty");
    if(actual==expected)return false;
    if(actual.empty() && !registered)return true;
    throw std::runtime_error("Quest identity changed; refusing to replace an established identity");
}
}
