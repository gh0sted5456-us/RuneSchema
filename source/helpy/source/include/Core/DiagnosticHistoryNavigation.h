#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace PS::DiagnosticHistoryNavigation {
inline std::optional<std::string> AdjacentSession(const std::vector<std::string>& newestFirst,
                                                   std::string_view selected,int direction){
    if(direction!=1&&direction!=-1)return std::nullopt;
    if(selected.empty()){
        if(direction==1&&!newestFirst.empty())return newestFirst.front();
        return std::nullopt;
    }
    const auto it=std::find(newestFirst.begin(),newestFirst.end(),selected);
    if(it==newestFirst.end())return std::nullopt;
    const auto position=static_cast<std::size_t>(it-newestFirst.begin());
    if(direction==1){
        if(position+1>=newestFirst.size())return std::nullopt;
        return newestFirst[position+1];
    }
    if(position==0)return std::string{};
    return newestFirst[position-1];
}
inline std::uint64_t SelectedOccurrence(const std::deque<std::uint64_t>& oldestFirst,
                                        std::uint64_t requested){
    if(oldestFirst.empty())return 0;
    return std::find(oldestFirst.begin(),oldestFirst.end(),requested)!=oldestFirst.end()
        ?requested:oldestFirst.back();
}
inline std::uint64_t AdjacentOccurrence(const std::deque<std::uint64_t>& oldestFirst,
                                        std::uint64_t selected,int direction){
    if(oldestFirst.empty()||(direction!=1&&direction!=-1))return 0;
    const auto id=SelectedOccurrence(oldestFirst,selected);
    const auto it=std::find(oldestFirst.begin(),oldestFirst.end(),id);
    const auto index=static_cast<std::size_t>(it-oldestFirst.begin());
    if(direction==1)return index?oldestFirst[index-1]:0;
    return index+1<oldestFirst.size()?oldestFirst[index+1]:0;
}
template<class Events>std::optional<std::size_t> EventIndex(const Events& events,std::uint64_t id){
    if(!id)return std::nullopt;
    for(std::size_t i=0;i<events.size();++i)if(events[i].Id==id)return i;
    return std::nullopt;
}
}
