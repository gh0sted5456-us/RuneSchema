#include "Core/MapProjection.h"
#include "Core/MapViewport.h"
#include "Core/MapSpawnRequest.h"
#include "Core/BuildingMenu.h"
#include "Core/MapParty.h"
#include <cassert>
int main(){
    using Json=nlohmann::json;
    Json party={{"World","/World"},{"CanBuildAndTeleport",true},{"PartyTravelVersion",2},{"Players",Json::array({
        {{"Path","host"},{"Name","Same name"},{"Guid","host-guid"},{"IsLocal",true}},
        {{"Path","guest"},{"Name","Same name"},{"Guid","guest-guid"},{"IsLocal",false}}})}};
    const auto to=PS::MapParty::Travel(party,"guest",false,42,123);
    assert(to["Action"]=="MapTeleport"&&to["Player"]=="host"&&to["TargetPlayer"]=="guest"&&to["Generation"]==42&&!to.contains("TargetLocation"));
    const auto recall=PS::MapParty::Travel(party,"guest",true,42,124);
    assert(recall["Player"]=="guest"&&recall["TargetPlayer"]=="host"&&recall["PlayerGuid"]=="guest-guid"&&recall["TargetPlayerGuid"]=="host-guid");
    party["Players"].push_back({{"Path","other"},{"Name","Other guest"},{"Guid","other-guid"}});
    const auto between=PS::MapParty::Between(party,"guest","other",42,125);
    assert(between["Player"]=="guest"&&between["TargetPlayer"]=="other");party["Players"].erase(party["Players"].end()-1);
    const auto denied=[&](const Json& snapshot,const char* path){bool blocked=false;try{PS::MapParty::Travel(snapshot,path,false,42,125);}catch(...){blocked=true;}assert(blocked);};
    denied(party,"host");denied(party,"disconnected");
    auto invalid=party;invalid["CanBuildAndTeleport"]=false;denied(invalid,"guest");
    invalid=party;invalid.erase("PartyTravelVersion");denied(invalid,"guest");
    invalid=party;invalid["Players"][0]["IsLocal"]=false;denied(invalid,"guest");
    invalid=party;invalid["Players"][1]["IsLocal"]=true;denied(invalid,"guest");
    invalid=party;invalid["Players"][1]["Guid"]="";denied(invalid,"guest");
    invalid=party;invalid["Players"].push_back(invalid["Players"][1]);denied(invalid,"guest");
    invalid=party;invalid["World"]="";denied(invalid,"guest");
    const nlohmann::json chest={{"Name","Iron Chest"},{"CategoryId",2},{"Category","Furniture"},{"Group","Storage"},{"Path","/Game/IronChest.IronChest"}};
    assert(PS::BuildingMenu::Matches(chest,"iron")&&PS::BuildingMenu::Matches(chest,"STORAGE",2)&&!PS::BuildingMenu::Matches(chest,"iron",1)&&!PS::BuildingMenu::Matches(chest,"roof"));
    assert(PS::BuildingMenu::BlockedReason(true,true,true,true,true,false,false).empty());
    assert(PS::BuildingMenu::BlockedReason(true,false,true,true,true,false,false).find("Nothing has been placed")!=std::string::npos);
    assert(!PS::BuildingMenu::BlockedReason(false,true,true,true,true,false,false).empty());
    assert(!PS::BuildingMenu::BlockedReason(true,true,true,true,true,true,false).empty());
    using namespace PS::MapProjection;
    Projection p{{198000,91450},{0,1},{1,0},210000,210000,-90000,90000};assert(p.Valid());
    const auto toPoint=PS::MapParty::ToPoint(party,"guest",p,p.Center,42,126);
    assert(toPoint["PartyPoint"]==true&&toPoint["Player"]=="guest"&&toPoint["TargetLocation"]["X"]==198000&&!toPoint.contains("TargetPlayer"));
    bool badDestination=false;try{PS::MapParty::ToPoint(party,"guest",p,{1e9,0},42,127);}catch(...){badDestination=true;}assert(badDestination);
    for(Point uv:std::array{Point{0,0},Point{1,0},Point{0,1},Point{1,1},Point{0.5,0.5},Point{0.29,0.72}}){const auto world=p.ToWorld(uv),back=p.ToUV(world);assert(std::abs(back.X-uv.X)<1e-9&&std::abs(back.Y-uv.Y)<1e-9&&p.Contains(world));}
    assert(!p.Contains({1e9,0}));assert(Projection::From(p.ToJson()).Valid());assert(!Projection::From({{"Center",nullptr}}).Valid());
    auto definition=nlohmann::json{{"Key","tree"},{"Class","/Game/Trees/Oak.Oak_C"},{"Type","Resource"}};
    const auto request=PS::MapSpawnRequest::Build(p,p.Center,definition,"player",true,1,500,123);
    assert(request["Permanent"]==true&&request["Resource"]==true&&request["TargetLocation"]["X"]==198000&&request["TargetLocation"]["Y"]==91450&&request["PlacementRadius"]==0);
    bool rejected=false;try{PS::MapSpawnRequest::Build(p,{1e9,0},definition,"player",false,1,0,124);}catch(...){rejected=true;}assert(rejected);
    definition["Type"]="AI";const auto temporary=PS::MapSpawnRequest::Build(p,p.Center,definition,"player",false,4,250,124);assert(temporary["Permanent"]==false&&temporary["Resource"]==false&&temporary["Count"]==4&&temporary["PlacementRadius"]==250);
    const auto building=PS::MapSpawnRequest::Building(p,p.Center,{{"Path","/Game/Building/TeleportStone.TeleportStone"}},"named-player",true,90,125);
    assert(building["Action"]=="SpawnBuilding"&&building["Player"]=="named-player"&&building["Permanent"]==true&&building["Count"]==1&&building["Yaw"]==90);
    auto prefab=nlohmann::json{{"schema","rsdwtools.buildings.v1"},{"pieces",nlohmann::json::array({{{"piece_id",1},{"piece_data_name","/Game/Buildings/Wall.Wall"},{"x",10},{"y",20},{"z",30}}})}};
    const auto import=PS::MapSpawnRequest::Prefab(p,p.Center,prefab,"named-player",90,42,128);
    assert(import["Action"]=="ImportBuildings"&&import["Document"]==prefab&&import["Generation"]==42&&import["Yaw"]==90&&import["_RequestId"]==128);
    for(const auto bad:nlohmann::json::array({nullptr,nlohmann::json::object(),{{"schema","other"},{"pieces",prefab["pieces"]}},{{"schema","rsdwtools.buildings.v1"},{"pieces",nlohmann::json::array()}}})){
        rejected=false;try{PS::MapSpawnRequest::Prefab(p,p.Center,bad,"named-player",0,42,129);}catch(...){rejected=true;}assert(rejected);
    }
    rejected=false;try{PS::MapSpawnRequest::Prefab(p,{1e9,0},prefab,"named-player",0,42,129);}catch(...){rejected=true;}assert(rejected);
    const auto teleport=PS::MapSpawnRequest::Target(p,p.ToWorld({.25,.75}),"MapTeleport","named-player",126);assert(teleport["Action"]=="MapTeleport"&&teleport["Player"]=="named-player");
    rejected=false;try{PS::MapSpawnRequest::Target(p,p.Center,"MapTeleport","",127);}catch(...){rejected=true;}assert(rejected);
    for(const auto dimensions:std::array{Point{1600,600},Point{760,900},Point{3440,900},Point{800,800}}){
        Viewport view{dimensions.X,dimensions.Y,1,1,{0.5,0.5}};view.Clamp();
        for(const auto uv:std::array{Point{0.5,0.5},Point{0.45,0.52},Point{0.55,0.48}}){const auto pixel=view.ToPixel(uv),back=view.ToUV(pixel);
            assert(std::abs(back.X-uv.X)<1e-9&&std::abs(back.Y-uv.Y)<1e-9);}
        const auto span=view.Span();assert(std::abs(dimensions.X/span.X-dimensions.Y/span.Y)<1e-6); // No texture stretching.
        const Point mouse{dimensions.X*.55,dimensions.Y*.45};const auto anchor=view.ToUV(mouse);
        view.ZoomAt(8,mouse);const auto anchored=view.ToUV(mouse);
        assert(std::abs(anchor.X-anchored.X)<1e-9&&std::abs(anchor.Y-anchored.Y)<1e-9);
        view.ZoomAt(1000,mouse);assert(view.Zoom==32);view.Pan({1e9,-1e9});
        assert(Viewport::InMap(view.ToUV({0,0}))&&Viewport::InMap(view.ToUV(dimensions)));
        view.Zoom=view.FitZoom();view.Clamp();const auto corner=view.ToPixel({0,0}),opposite=view.ToPixel({1,1});
        assert(corner.X>=-1e-6&&corner.Y>=-1e-6&&opposite.X<=dimensions.X+1e-6&&opposite.Y<=dimensions.Y+1e-6);
    }
}
