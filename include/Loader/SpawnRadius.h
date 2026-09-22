#pragma once
#include <nlohmann/json.hpp>
#include <cmath>
#include <stdexcept>

namespace DragonWilds::Spawns {
inline nlohmann::json RadiusProperties(const nlohmann::json& data) {
    using Json=nlohmann::json;
    const bool meters=data.contains("SpawnRadiusMeters");
    const bool minimum=data.contains("MinSpawnDistance"), maximum=data.contains("MaxSpawnDistance");
    if(!meters && !minimum && !maximum)return Json::object();
    if(meters && (minimum || maximum))throw std::runtime_error("SpawnRadiusMeters cannot be combined with legacy spawn distances");
    const auto distance=[&](const char* field,double low,double high) {
        const auto& value=data.at(field);
        if(!value.is_number())throw std::runtime_error(std::string(field)+" must be numeric");
        const auto number=value.get<double>();
        if(!std::isfinite(number) || number<low || number>high)throw std::runtime_error(std::string(field)+" is outside supported bounds");
        return number;
    };
    Json result={{"bOverrideSpawnRadius",true}};
    if(meters) {
        const auto radius=distance("SpawnRadiusMeters",0.01,10000)*100.0;
        if(data.contains("RequiresActivation") && data.at("RequiresActivation")!=false)
            throw std::runtime_error("SpawnRadiusMeters requires automatic native activation; RequiresActivation must be false or omitted");
        if(data.contains("Properties")) {
            const auto& properties=data.at("Properties");
            for(const auto* field:{"bOverrideSpawnRadius","MinDistanceForSpawnPointToSpawn","MaxDistanceForSpawnPointToSpawn","bRequiresActivation"})
                if(properties.contains(field))throw std::runtime_error("SpawnRadiusMeters conflicts with raw radius/activation Properties");
        }
        result["MinDistanceForSpawnPointToSpawn"]=0.0;
        result["MaxDistanceForSpawnPointToSpawn"]=radius;
        result["bRequiresActivation"]=false;
    } else {
        if(minimum)result["MinDistanceForSpawnPointToSpawn"]=distance("MinSpawnDistance",0,100000000);
        if(maximum)result["MaxDistanceForSpawnPointToSpawn"]=distance("MaxSpawnDistance",0,100000000);
        if(minimum && maximum && data.at("MinSpawnDistance").get<double>()>data.at("MaxSpawnDistance").get<double>())
            throw std::runtime_error("MinSpawnDistance cannot exceed MaxSpawnDistance");
    }
    return result;
}
}
