#pragma once
#include "Loader/TimeOfDay.h"
#include "Loader/VendorOffers.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <optional>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>

namespace DragonWilds::VendorCategoryGate {
struct Rule {
    std::string Category;
    std::optional<int> MinimumPowerLevel;
    std::optional<int> MaximumPowerLevel;
    TimeOfDay::Requirement Time=TimeOfDay::Requirement::Any;
    std::string CompletedQuest;
};

inline std::vector<Rule> Parse(const nlohmann::json& value) {
    if(!value.is_array() || value.size()>64)
        throw std::runtime_error("CategoryRules must be an array of at most 64 rules");
    std::vector<Rule> result;std::set<std::string> categories;
    for(const auto& entry:value) {
        if(!entry.is_object())throw std::runtime_error("Each CategoryRules entry must be an object");
        for(const auto& [key,unused]:entry.items())
            if(key!="Category" && key!="MinPowerLevel" && key!="MaxPowerLevel" && key!="TimeOfDay" && key!="QuestCompleted")
                throw std::runtime_error("Unknown vendor category rule field: "+key);
        if(!entry.contains("Category") || !entry.at("Category").is_string())
            throw std::runtime_error("CategoryRules.Category must be a string");
        Rule rule;rule.Category=entry.at("Category").get<std::string>();
        if(rule.Category.empty() || rule.Category.size()>128)
            throw std::runtime_error("CategoryRules.Category must contain 1 to 128 characters");
        if(!categories.insert(rule.Category).second)
            throw std::runtime_error("CategoryRules contains a duplicate category: "+rule.Category);
        const auto power=[&](const char* field)->std::optional<int> {
            if(!entry.contains(field))return std::nullopt;
            if(!entry.at(field).is_number_integer())throw std::runtime_error(std::string(field)+" must be an integer");
            const auto parsed=entry.at(field).get<int>();
            if(parsed<0 || parsed>100)throw std::runtime_error(std::string(field)+" must be between 0 and 100");
            return parsed;
        };
        rule.MinimumPowerLevel=power("MinPowerLevel");rule.MaximumPowerLevel=power("MaxPowerLevel");
        if(rule.MinimumPowerLevel && rule.MaximumPowerLevel && *rule.MinimumPowerLevel>*rule.MaximumPowerLevel)
            throw std::runtime_error("CategoryRules MinPowerLevel cannot exceed MaxPowerLevel");
        if(entry.contains("TimeOfDay")) {
            if(!entry.at("TimeOfDay").is_string())throw std::runtime_error("CategoryRules.TimeOfDay must be Any, Day, or Night");
            rule.Time=TimeOfDay::Parse(entry.at("TimeOfDay").get<std::string>());
        }
        if(entry.contains("QuestCompleted")) {
            if(!entry.at("QuestCompleted").is_string() || entry.at("QuestCompleted").get<std::string>().empty()
                || entry.at("QuestCompleted").get<std::string>().size()>512)
                throw std::runtime_error("CategoryRules.QuestCompleted must be a quest ID");
            rule.CompletedQuest=entry.at("QuestCompleted").get<std::string>();
        }
        if(!rule.MinimumPowerLevel && !rule.MaximumPowerLevel && rule.Time==TimeOfDay::Requirement::Any && rule.CompletedQuest.empty())
            throw std::runtime_error("CategoryRules entry must define a power, time, or completed-quest gate");
        result.push_back(std::move(rule));
    }
    return result;
}

inline nlohmann::json Filter(const nlohmann::json& items,const std::vector<Rule>& rules,
    std::optional<int> power,TimeOfDay::Requirement time,const std::function<bool(const std::string&)>& completed={}) {
    if(!items.is_array())throw std::runtime_error("Vendor Items must be an array");
    nlohmann::json result=nlohmann::json::array();
    for(const auto& item:items) {
        const auto category=VendorOffers::Category(item);
        const auto found=std::find_if(rules.begin(),rules.end(),[&](const Rule& rule){return rule.Category==category;});
        const auto allowed=[&](std::optional<int> minimum,std::optional<int> maximum,TimeOfDay::Requirement required,const std::string& quest){
            if((minimum || maximum) && !power)return false;
            if(minimum && *power<*minimum)return false;if(maximum && *power>*maximum)return false;
            if(required!=TimeOfDay::Requirement::Any && required!=time)return false;
            return quest.empty() || (completed && completed(quest));
        };
        if(found!=rules.end() && !allowed(found->MinimumPowerLevel,found->MaximumPowerLevel,found->Time,found->CompletedQuest))continue;
        const auto number=[&](const char* key)->std::optional<int>{if(!item.contains(key))return {};return item.at(key).get<int>();};
        auto itemTime=TimeOfDay::Requirement::Any;if(item.contains("TimeOfDay"))itemTime=TimeOfDay::Parse(item.at("TimeOfDay").get<std::string>());
        const auto itemQuest=item.value("QuestCompleted",std::string{});
        if(!allowed(number("MinPowerLevel"),number("MaxPowerLevel"),itemTime,itemQuest))continue;
        result.push_back(item);
    }
    return result;
}
}
