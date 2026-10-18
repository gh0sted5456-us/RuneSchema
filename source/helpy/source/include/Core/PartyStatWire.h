#pragma once
#include "Core/PartyStats.h"
#include <set>
namespace PS::PartyStatWire {
using Json=nlohmann::json;
// Whitelisted presentation only. No inventory paths, permissions or actions
// cross this envelope. Ages use the receiver clock, never host wall-clock time.
inline Json Encode(const Json& stats,int64_t now){
    Json out={{"Version",1},{"Values",Json::object()}};const Json row={{"Stats",stats}};
    for(const char* key:{"Health","Stamina","SpecialPower","Hunger","Thirst","RestLevel","PowerLevel","Food","Rest"})try{
        const auto& value=PartyStats::Find(row,key);if(!PartyStats::Fresh(value,now))continue;
        Json item;
        if(std::string(key)=="PowerLevel"){const auto power=PartyStats::Power(row,now);if(!power)continue;item={{"Value",*power}};}
        else if(std::string(key)=="Rest"){const auto state=value.at("State").get<std::string>();if(state!="Sleeping"&&state!="Resting"&&state!="Not resting")continue;item={{"State",state}};}
        else if(std::string(key)=="Food"){
            if(!value.at("ActiveSlots").is_number_integer()||!value.at("MaximumSlots").is_number_integer())continue;
            const int active=value.at("ActiveSlots"),max=value.at("MaximumSlots");if(active<0||max<=0||max>32||active>max)continue;
            item={{"ActiveSlots",active},{"MaximumSlots",max},{"State",std::to_string(active)+" / "+std::to_string(max)+" food effects active"}};
        }else{const auto gauge=PartyStats::Read(row,key,now);if(!gauge)continue;item={{"Current",gauge->Current},{"Maximum",gauge->Maximum}};}
        item["AgeMs"]=now-value.at("ObservedUnixMs").get<int64_t>();out["Values"][key]=std::move(item);
    }catch(...){}return out;
}
inline Json Decode(const Json& wire,int64_t now,int64_t roundTripMs){
    Json result={{"Source","Host/server native (authenticated roster)"},
        {"Hunger",{{"Reason","Native sustenance not reported by server"}}},
        {"Thirst",{{"Reason","Native hydration not reported by server"}}},
        {"RestLevel",{{"Reason","Native endurance not reported by server"}}}};
    if(!wire.is_object()||!wire.contains("Version")||!wire["Version"].is_number_integer()||wire["Version"]!=1||!wire.contains("Values")||!wire["Values"].is_object()
        ||wire["Values"].size()>9||now<=0||roundTripMs<0||roundTripMs>4000)return Json::object();
    Json local=wire["Values"];
    for(auto& [key,value]:local.items())try{
        if(value.is_object())value.erase("ObservedUnixMs");
        if(!value.is_object()||!value.at("AgeMs").is_number_integer())continue;
        const int64_t age=value.at("AgeMs");if(age<0||age>10000-roundTripMs||now<=age+roundTripMs)continue;
        value["ObservedUnixMs"]=now-age-roundTripMs;
    }catch(...){}
    const auto clean=Encode(local,now);for(const auto& [key,value]:clean["Values"].items()){
        auto item=value;const auto age=item.at("AgeMs").get<int64_t>();item.erase("AgeMs");item["ObservedUnixMs"]=now-age;result[key]=std::move(item);
    }return result;
}
// Merge by unique character identity, not a mutable display name. Never replace
// location, action handles or authority fields supplied by the live map.
inline void Merge(Json& mapRows,const Json& roster,int64_t now){
    if(!mapRows.is_array()||!roster.is_array()||mapRows.size()>256||roster.size()>256)return;
    std::set<std::string> seen,duplicates;
    for(const auto& row:roster)if(row.is_object()&&row.contains("Guid")&&row["Guid"].is_string()){
        const auto guid=row["Guid"].get<std::string>();if(!guid.empty()&&!seen.insert(guid).second)duplicates.insert(guid);
    }
    for(auto& row:mapRows)try{
        const auto guid=row.at("Guid").get<std::string>();if(guid.empty()||duplicates.contains(guid))continue;
        for(const auto& source:roster)if(source.value("Guid",std::string{})==guid&&source.contains("Stats")){
            const auto& stats=source["Stats"];if(stats.value("Source",std::string{})!="Host/server native (authenticated roster)")break;
            auto clean=Decode(Encode(stats,now),now,0);row["Stats"]=std::move(clean);row["Health"]=row["Stats"].value("Health",Json{});break;
        }
    }catch(...){}
}
}
