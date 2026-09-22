#pragma once
#include <map>
#include <string>
#include <stdexcept>

namespace DragonWilds::Quests {
// Transient observation baseline only. Quest credit itself uses native QuestInts.
class AcquisitionBaseline {
    std::map<std::string,int> counts;
public:
    int Observe(const std::string& item,int count,bool credit) {
        if(item.empty() || count<0)throw std::runtime_error("Invalid acquisition inventory observation");
        const auto [entry,created]=counts.try_emplace(item,count);
        const auto before=entry->second;
        entry->second=count; // Commit before callbacks so duplicate notifications cannot award twice.
        return credit && !created && count>before?count-before:0;
    }
};
}
