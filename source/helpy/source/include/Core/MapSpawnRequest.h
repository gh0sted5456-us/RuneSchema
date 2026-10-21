#pragma once
#include "Core/MapProjection.h"
#include "Core/ItemPresentation.h"
namespace PS::MapSpawnRequest {
inline nlohmann::json Target(const MapProjection::Projection& projection,MapProjection::Point point,const char* action,const std::string& player,uint64_t id){
    if(!projection.Contains(point)||player.empty())throw std::runtime_error("Choose a player and a point within the map");
    return {{"Action",action},{"_Source","HelpyMap"},{"_RequestId",id},{"Player",player},{"TargetLocation",{{"X",point.X},{"Y",point.Y}}}};
}
inline nlohmann::json Building(const MapProjection::Projection& projection,MapProjection::Point point,const nlohmann::json& piece,const std::string& player,bool permanent,double yaw,uint64_t id){
    const auto path=ItemPresentation::Text(piece,"Path");if(path.empty()||path.front()!='/'||!std::isfinite(yaw))throw std::runtime_error("Choose a native building piece");
    auto request=Target(projection,point,"SpawnBuilding",player,id);request.update({{"Building",path},{"Count",1},{"Distance",5.0},{"Scale",1.0},{"Yaw",yaw},{"Height",0.0},{"Permanent",permanent},{"AllowDeconstruction",true}});return request;
}
inline nlohmann::json Prefab(const MapProjection::Projection& projection,MapProjection::Point point,const nlohmann::json& document,const std::string& player,double yaw,uint64_t generation,uint64_t id){
    if(!document.is_object()||document.value("schema",std::string{})!="rsdwtools.buildings.v1"||!document.contains("pieces")||!document["pieces"].is_array()||document["pieces"].empty()||document["pieces"].size()>4096||document.dump().size()>1024*1024||!std::isfinite(yaw))throw std::runtime_error("Choose an RSDW Builder export (1-4096 pieces, up to 1 MiB)");
    auto request=Target(projection,point,"ImportBuildings",player,id);request.update({{"Document",document},{"Yaw",yaw},{"Generation",generation}});return request;
}
inline nlohmann::json Build(const MapProjection::Projection& projection,MapProjection::Point point,const nlohmann::json& definition,
    const std::string& player,bool permanent,int count,double radius,uint64_t requestId) {
    const auto type=ItemPresentation::Text(definition,"Type");const auto key=ItemPresentation::Text(definition,"Key"),path=ItemPresentation::Text(definition,"Class");
    if(!projection.Contains(point)||key.empty()||path.empty()||player.empty()||(type!="AI"&&type!="Resource")||count<1||count>20||!std::isfinite(radius)||radius<0||radius>10000)
        throw std::runtime_error("Choose a player, a supported spawn and a valid map position");
    return {{"Action","Spawn"},{"_Source","HelpyMap"},{"_RequestId",requestId},{"Player",player},{"Definition",key},{"Class",path},{"Resource",type=="Resource"},{"NPC",false},
        {"Permanent",permanent},{"Count",count},{"Scale",1.0},{"Distance",5.0},{"PlacementMode","Radius"},{"PlacementRadius",count==1?0.0:radius},{"Height",0.0},{"Yaw",0.0},
        {"TargetLocation",{{"X",point.X},{"Y",point.Y}}}};
}
}
