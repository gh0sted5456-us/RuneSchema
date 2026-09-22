#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <optional>
#include <limits>
#include <cmath>
#include <stdexcept>
#include <string>

namespace DragonWilds::Quests {
struct RepeatPolicy {
    bool Enabled=false;
    int64_t CooldownSeconds=0;
    int64_t ResetPeriodSeconds=0;
    int64_t ResetOffsetSeconds=0;
    double RewardMultiplierPerRun=0.0;
    double MaximumRewardMultiplier=1.0;
};
inline RepeatPolicy ParseRepeat(const nlohmann::json& data) {
    RepeatPolicy value;
    if(data.contains("Repeatable")) {
        if(!data.at("Repeatable").is_boolean())throw std::runtime_error("Repeatable must be boolean");
        value.Enabled=data.at("Repeatable").get<bool>();
    }
    if(!data.contains("Repeat"))return value;
    if(!value.Enabled || !data.at("Repeat").is_object())throw std::runtime_error("Repeat requires Repeatable true and an object");
    const auto& repeat=data.at("Repeat");
    for(const auto& [key,ignored]:repeat.items())if(key!="CooldownSeconds" && key!="Reset"
        && key!="RewardMultiplierPerRun" && key!="MaximumRewardMultiplier")throw std::runtime_error("Unknown Repeat field");
    if(repeat.contains("RewardMultiplierPerRun")) {
        if(!repeat.at("RewardMultiplierPerRun").is_number())throw std::runtime_error("RewardMultiplierPerRun must be numeric");
        value.RewardMultiplierPerRun=repeat.at("RewardMultiplierPerRun").get<double>();
        if(!std::isfinite(value.RewardMultiplierPerRun) || value.RewardMultiplierPerRun<0 || value.RewardMultiplierPerRun>10)
            throw std::runtime_error("RewardMultiplierPerRun must be from 0 to 10");
    }
    if(repeat.contains("MaximumRewardMultiplier")) {
        if(!repeat.at("MaximumRewardMultiplier").is_number())throw std::runtime_error("MaximumRewardMultiplier must be numeric");
        value.MaximumRewardMultiplier=repeat.at("MaximumRewardMultiplier").get<double>();
        if(!std::isfinite(value.MaximumRewardMultiplier) || value.MaximumRewardMultiplier<1 || value.MaximumRewardMultiplier>100)
            throw std::runtime_error("MaximumRewardMultiplier must be from 1 to 100");
    }
    if(value.RewardMultiplierPerRun>0 && !repeat.contains("MaximumRewardMultiplier"))
        throw std::runtime_error("Scaled repeat rewards require MaximumRewardMultiplier");
    if(repeat.contains("CooldownSeconds")) {
        const auto& seconds=repeat.at("CooldownSeconds");
        if(!seconds.is_number_integer() || seconds<0 || seconds>31536000)throw std::runtime_error("CooldownSeconds must be 0..31536000");
        value.CooldownSeconds=seconds.get<int64_t>();
    }
    if(repeat.contains("Reset")) {
        const auto& reset=repeat.at("Reset");
        if(!reset.is_object())throw std::runtime_error("Reset must be an object");
        for(const auto& [key,ignored]:reset.items())if(key!="Frequency" && key!="HourUTC" && key!="MinuteUTC" && key!="WeekdayUTC")throw std::runtime_error("Unknown Reset field");
        const auto frequency=reset.at("Frequency").get<std::string>();
        if(frequency!="Daily" && frequency!="Weekly")throw std::runtime_error("Reset Frequency must be Daily or Weekly");
        const auto integer=[&](const char* name,int maximum,int fallback) {
            if(!reset.contains(name))return fallback;
            const auto& n=reset.at(name);
            if(!n.is_number_integer() || n<0 || n>maximum)throw std::runtime_error("Reset time is out of range");
            return n.get<int>();
        };
        value.ResetPeriodSeconds=frequency=="Daily"?86400:604800;
        value.ResetOffsetSeconds=integer("HourUTC",23,0)*3600+integer("MinuteUTC",59,0)*60;
        if(frequency=="Weekly") {
            if(!reset.contains("WeekdayUTC"))throw std::runtime_error("Weekly reset requires WeekdayUTC (Monday=0)");
            value.ResetOffsetSeconds+=((integer("WeekdayUTC",6,0)+4)%7)*86400;
        } else if(reset.contains("WeekdayUTC"))throw std::runtime_error("Daily reset cannot specify WeekdayUTC");
    }
    return value;
}
inline int64_t CheckedTimeAdd(int64_t a,int64_t b) {
    if(a<0 || b<0 || a>std::numeric_limits<int64_t>::max()-b)throw std::runtime_error("Quest timestamp overflow");
    return a+b;
}
// Returns the first permitted time after the preceding completed run. No catch-up credits.
inline std::optional<int64_t> RepeatAvailableAt(const RepeatPolicy& policy,int64_t completed) {
    if(completed<0)throw std::runtime_error("Invalid completion timestamp");
    if(!policy.Enabled)return std::nullopt;
    int64_t next=CheckedTimeAdd(completed,policy.CooldownSeconds);
    if(policy.ResetPeriodSeconds) {
        const auto period=policy.ResetPeriodSeconds,offset=policy.ResetOffsetSeconds;
        if((period!=86400 && period!=604800) || offset<0 || offset>=period)throw std::runtime_error("Invalid reset policy");
        int64_t boundary=offset;
        if(completed>=offset)boundary=CheckedTimeAdd(completed,period-(completed-offset)%period);
        if(boundary>next)next=boundary;
    }
    return next;
}
inline bool RepeatReady(const RepeatPolicy& policy,int64_t completed,int64_t now,int64_t highWater) {
    if(now<0 || highWater<0 || now<highWater)throw std::runtime_error("Quest clock moved backwards; retry refused");
    const auto available=RepeatAvailableAt(policy,completed);
    return available && now>=*available;
}
}
