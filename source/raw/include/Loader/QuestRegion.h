#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace DragonWilds::Quests {
inline std::string RegionName(std::string value) {
    constexpr const char* prefix="EQuestRegion::";
    if(value.starts_with(prefix))value.erase(0,std::char_traits<char>::length(prefix));
    if(value.empty()||value.size()>64||value=="None"||value=="MAX"
        ||value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")!=value.npos)
        throw std::runtime_error("Quest Region requires a native EQuestRegion name, not None or a numeric ID");
    return value;
}
inline uint8_t ResolveRegion(const std::string& requested,const std::vector<std::pair<std::string,int64_t>>& values) {
    const auto name=RegionName(requested);int result=-1;
    if(values.empty()||values.size()>256)throw std::runtime_error("Quest region enum size changed");
    for(const auto& [native,value]:values){
        auto leaf=native;const auto separator=leaf.rfind("::");if(separator!=leaf.npos)leaf.erase(0,separator+2);
        if(leaf!=name)continue;
        if(result!=-1||value<0||value>255)throw std::runtime_error("Quest Region is ambiguous or out of range");
        result=static_cast<int>(value);
    }
    if(result<0)throw std::runtime_error("Quest Region is not available in this game: "+name);
    return static_cast<uint8_t>(result);
}
}
