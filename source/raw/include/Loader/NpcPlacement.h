#pragma once
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>
namespace DragonWilds {
struct NpcPlacement {
    static bool Matches(const double* expected,const std::array<double,3>& actual,bool rotation=false) {
        for(size_t i=0;i<3;++i) {
            if(!std::isfinite(expected[i]) || !std::isfinite(actual[i]))return false;
            const double delta=rotation?std::remainder(actual[i]-expected[i],360.0):actual[i]-expected[i];
            if(std::abs(delta)>(rotation?0.01:0.1))return false;
        }
        return true;
    }
    std::array<double,3> Position{};
    bool Ground=false;
    double Offset=0;
    static NpcPlacement Parse(const nlohmann::json& value) {
        NpcPlacement result;
        nlohmann::json coords;
        if(value.is_array() && value.size()==3)coords=value;
        else if(value.is_object() && value.size()==3 && value.contains("X") && value.contains("Y") && value.contains("Z"))
            coords=nlohmann::json::array({value["X"],value["Y"],value["Z"]});
        else throw std::runtime_error("NPC Location requires [X,Y,Z] or {X,Y,Z}");
        if(coords[2].is_string()) {
            const auto text=coords[2].get<std::string>();
            if(text.empty() || text[0]!='$')throw std::runtime_error("NPC Z requires $, $+offset or $-offset");
            if(text.size()>1) {
                if(text.size()<3 || (text[1]!='+' && text[1]!='-'))throw std::runtime_error("NPC Z offset requires + or -");
                size_t count=0;
                result.Offset=std::stod(text.substr(1),&count);
                if(count!=text.size()-1 || !std::isfinite(result.Offset) || std::abs(result.Offset)>100000)
                    throw std::runtime_error("NPC Z offset must be finite and within +/-100000");
            }
            result.Ground=true;coords[2]=0.0;
        }
        for(size_t i=0;i<3;++i) {
            if(!coords[i].is_number())throw std::runtime_error("NPC coordinates must be finite numbers (Z also supports $)");
            result.Position[i]=coords[i].get<double>();
            if(!std::isfinite(result.Position[i]))throw std::runtime_error("NPC coordinate is not finite");
        }
        return result;
    }
};
}
