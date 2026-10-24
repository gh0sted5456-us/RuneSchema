#pragma once
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
namespace PS::PartyStats {
struct Gauge { double Current,Maximum; double Fraction()const{return Current/Maximum;} };
inline bool Fresh(const nlohmann::json& stat,int64_t now){
    try{const auto at=stat.at("ObservedUnixMs").get<int64_t>();return at>0&&at<=now&&now-at<=10000;}catch(...){return false;}
}
inline const nlohmann::json& Find(const nlohmann::json& row,const char* key){
    static const nlohmann::json missing;
    if(!row.is_object())return missing;
    const auto all=row.find("Stats");if(all!=row.end()&&all->is_object()){const auto it=all->find(key);if(it!=all->end())return *it;}
    const auto it=row.find(key);return it!=row.end()?*it:missing;
}
inline std::optional<Gauge> Read(const nlohmann::json& row,const char* key,int64_t now){
    const auto& stat=Find(row,key);if(!Fresh(stat,now))return {};
    try{if(!stat.at("Current").is_number()||!stat.at("Maximum").is_number())return {};
        const double value=stat.at("Current").get<double>(),maximum=stat.at("Maximum").get<double>();
        if(!std::isfinite(value)||!std::isfinite(maximum)||value<0||maximum<=0||maximum>1e9||value>maximum)return {};return Gauge{value,maximum};
    }catch(...){return {};}
}
inline std::optional<double> Power(const nlohmann::json& row,int64_t now){
    const auto& stat=Find(row,"PowerLevel");if(!Fresh(stat,now))return {};
    try{if(!stat.at("Value").is_number())return {};const double value=stat.at("Value").get<double>();if(std::isfinite(value)&&value>=0&&value<=1e6)return value;}catch(...){}return {};
}
inline std::string Reason(const nlohmann::json& row,const char* key,int64_t now){
    const auto& stat=Find(row,key);if(stat.is_null())return "Not reported by this runtime";
    const auto reason=stat.find("Reason");if(reason!=stat.end()&&reason->is_string())return reason->get<std::string>();
    return Fresh(stat,now)?"Native state is not a numeric gauge":"Observation expired; waiting for a fresh native reading";
}
}
