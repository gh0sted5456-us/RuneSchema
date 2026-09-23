#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <stdexcept>
namespace PS::SpawnAuthoring {
using Json=nlohmann::json;
inline Json Placement(Json definition,const std::string& id,const std::array<double,3>& position,double yaw) {
    if(id.empty() || id.size()>128 || id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=id.npos)
        throw std::runtime_error("Invalid authored spawn ID");
    for(double value:position)if(!std::isfinite(value))throw std::runtime_error("Placement must be finite");
    if(!std::isfinite(yaw))throw std::runtime_error("Rotation must be finite");
    const auto type=definition.at("Type").get<std::string>();
    if(type!="Actor" && type!="AISpawnPoint")throw std::runtime_error("Only actor or AI placements can be exported");
    definition.erase("EventOnly");definition.erase("$Id");definition["Id"]=id;
    if(definition.contains("VisualEffect") && definition["VisualEffect"].value("Type",std::string{})=="Ghost Glow")definition["VisualEffect"]["Type"]="Ghost";
    definition["Location"]={{"X",position[0]},{"Y",position[1]},{"Z",position[2]}};
    definition["Rotation"]={{"Pitch",0},{"Yaw",yaw},{"Roll",0}};
    return definition;
}
}
