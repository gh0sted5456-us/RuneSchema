#pragma once
#include <nlohmann/json.hpp>
#include <cmath>
#include <stdexcept>
namespace DragonWilds::SpawnFields {
using Json=nlohmann::json;
struct ParsedLocation {Json Vector;bool Ground=false;double GroundOffset=0;};
inline ParsedLocation ParseLocation(const Json& field) {
    Json vector;
    if(field.is_array()&&field.size()==3)vector={{"X",field[0]},{"Y",field[1]},{"Z",field[2]}};
    else if(field.is_object()&&field.contains("X")&&field.contains("Y")&&field.contains("Z"))
        vector={{"X",field.at("X")},{"Y",field.at("Y")},{"Z",field.at("Z")}};
    else throw std::runtime_error("Location requires [X,Y,Z] or {X,Y,Z}");
    if(!vector["X"].is_number()||!vector["Y"].is_number())throw std::runtime_error("Location X and Y must be numeric");
    ParsedLocation result{vector};
    if(vector["Z"].is_string()) {
        const auto text=vector["Z"].get<std::string>();
        if(text.empty()||text.front()!='$')throw std::runtime_error("Location.Z string must use $, $+offset, or $-offset");
        if(text.size()>1) {
            std::size_t consumed=0;
            try{result.GroundOffset=std::stod(text.substr(1),&consumed);}
            catch(...){throw std::runtime_error("Location.Z $ offset must be numeric");}
            if(consumed!=text.size()-1)throw std::runtime_error("Location.Z $ offset must be numeric");
        }
        if(!std::isfinite(result.GroundOffset)||std::abs(result.GroundOffset)>100000)
            throw std::runtime_error("Location.Z $ offset must be finite and between -100000 and 100000");
        result.Ground=true;result.Vector["Z"]=0.0;
    } else if(!vector["Z"].is_number())throw std::runtime_error("Location Z must be numeric or use $, $+offset, or $-offset");
    for(const auto* axis:{"X","Y","Z"})if(!std::isfinite(result.Vector[axis].get<double>()))
        throw std::runtime_error("Location coordinates must be finite");
    return result;
}
inline void Drops(const Json& drops) {
    if(!drops.is_array() || drops.size()>16)throw std::runtime_error("AdditionalDrops must be an array of at most 16 items");
    for(const auto& drop:drops) {
        if(!drop.is_object())throw std::runtime_error("AdditionalDrops entries must be objects");
        for(const auto& [key,value]:drop.items())if(key!="Item" && key!="Min" && key!="Max" && key!="ChancePercent")throw std::runtime_error("Unknown AdditionalDrops field: "+key);
        const auto path=drop.at("Item").get<std::string>();
        if(path.empty() || path[0]!='/' || path.find('.')==path.npos || path.find_first_of("\r\n\t")!=path.npos)throw std::runtime_error("AdditionalDrops.Item requires an item object path");
        if(!drop.at("Min").is_number_integer() || !drop.at("Max").is_number_integer())throw std::runtime_error("Drop quantities must be integers");
        const auto min=drop.at("Min").get<int>(),max=drop.at("Max").get<int>();
        const auto chance=drop.at("ChancePercent").get<double>();
        if(min<1 || max<min || max>10000 || !std::isfinite(chance) || chance<0 || chance>100)throw std::runtime_error("Drop quantities require 1 <= Min <= Max <= 10000; chance is 0..100");
    }
}
inline Json Expand(const Json& entry) {
    if(!entry.contains("Grid"))return Json::array({entry});
    if(entry.contains("EventOnly") || (entry.at("Type")!="AISpawnPoint" && entry.at("Type")!="Actor"))throw std::runtime_error("Grid requires an ordinary AI or Actor spawn");
    const auto& grid=entry.at("Grid");
    if(!grid.is_object() || grid.size()!=3 || !grid.at("Rows").is_number_integer() || !grid.at("Columns").is_number_integer())throw std::runtime_error("Grid requires Rows, Columns and SpacingMeters");
    const auto rows=grid.at("Rows").get<int>(),columns=grid.at("Columns").get<int>();
    const auto spacing=grid.at("SpacingMeters").get<double>();
    if(rows<1 || columns<1 || rows>20 || columns>20 || rows*columns>100 || !std::isfinite(spacing) || spacing<0.1 || spacing>1000)throw std::runtime_error("Grid limit: 100 points, 1..20 rows/columns, spacing 0.1..1000 metres");
    const auto origin=ParseLocation(entry.at("Location"));
    const auto x=origin.Vector.at("X").get<double>(),y=origin.Vector.at("Y").get<double>();
    const auto yaw=entry.value("Rotation",Json::object()).value("Yaw",0.0);
    if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(yaw))throw std::runtime_error("Grid origin and yaw must be finite");
    const auto angle=yaw*0.017453292519943295;
    Json result=Json::array();
    for(int r=0;r<rows;++r)for(int c=0;c<columns;++c) {
        auto value=entry;value.erase("Grid");value["Location"]=origin.Vector;
        if(origin.Ground)value["Location"]["Z"]=entry.at("Location").is_array()?entry.at("Location")[2]:entry.at("Location").at("Z");
        const auto dx=c*spacing*100,dy=r*spacing*100;
        value["Location"]["X"]=x+dx*std::cos(angle)-dy*std::sin(angle);
        value["Location"]["Y"]=y+dx*std::sin(angle)+dy*std::cos(angle);
        if(entry.contains("Id"))value["Id"]=entry.at("Id").get<std::string>()+"_"+std::to_string(r)+"_"+std::to_string(c);
        result.push_back(std::move(value));
    }
    return result;
}
}
