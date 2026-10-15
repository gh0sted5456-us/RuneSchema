#include "Core/MapProjection.h"
#include "Core/MapSpawnRequest.h"
#include <cassert>
int main(){
    using namespace PS::MapProjection;
    Projection p{{198000,91450},{0,1},{1,0},210000,210000,-90000,90000};assert(p.Valid());
    for(Point uv:std::array{Point{0,0},Point{1,0},Point{0,1},Point{1,1},Point{0.5,0.5},Point{0.29,0.72}}){const auto world=p.ToWorld(uv),back=p.ToUV(world);assert(std::abs(back.X-uv.X)<1e-9&&std::abs(back.Y-uv.Y)<1e-9&&p.Contains(world));}
    assert(!p.Contains({1e9,0}));assert(Projection::From(p.ToJson()).Valid());assert(!Projection::From({{"Center",nullptr}}).Valid());
    auto definition=nlohmann::json{{"Key","tree"},{"Class","/Game/Trees/Oak.Oak_C"},{"Type","Resource"}};
    const auto request=PS::MapSpawnRequest::Build(p,p.Center,definition,"player",true,1,500,123);
    assert(request["Permanent"]==true&&request["Resource"]==true&&request["TargetLocation"]["X"]==198000&&request["TargetLocation"]["Y"]==91450&&request["PlacementRadius"]==0);
    bool rejected=false;try{PS::MapSpawnRequest::Build(p,{1e9,0},definition,"player",false,1,0,124);}catch(...){rejected=true;}assert(rejected);
    definition["Type"]="AI";const auto temporary=PS::MapSpawnRequest::Build(p,p.Center,definition,"player",false,4,250,124);assert(temporary["Permanent"]==false&&temporary["Resource"]==false&&temporary["Count"]==4&&temporary["PlacementRadius"]==250);
    const auto building=PS::MapSpawnRequest::Building(p,p.Center,{{"Path","/Game/Building/TeleportStone.TeleportStone"}},"named-player",true,90,125);
    assert(building["Action"]=="SpawnBuilding"&&building["Player"]=="named-player"&&building["Permanent"]==true&&building["Count"]==1&&building["Yaw"]==90);
    const auto teleport=PS::MapSpawnRequest::Target(p,p.ToWorld({.25,.75}),"MapTeleport","named-player",126);assert(teleport["Action"]=="MapTeleport"&&teleport["Player"]=="named-player");
    rejected=false;try{PS::MapSpawnRequest::Target(p,p.Center,"MapTeleport","",127);}catch(...){rejected=true;}assert(rejected);
}
