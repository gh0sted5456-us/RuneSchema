#include "Core/MapProjection.h"
#include "Core/MapAtlas.h"
#include "Core/MapSpawnRequest.h"
#include "Core/BuildingMenu.h"
#include "Core/MapParty.h"
#include <cassert>

static void CheckAtlas() {
    using namespace PS::MapAtlas;
    const auto mainland = Mainland.Project(-90000, 90000);
    assert(MatchesMainland(mainland));
    auto rotated = mainland;
    rotated.Right = {0, 1};
    rotated.Up = {1, 0};
    assert(rotated.Valid() && !MatchesMainland(rotated));
    const auto overview = Frame(OverviewTab).Project(-90000, 90000);
    assert(MatchesMainland(overview));
    assert(!overview.Contains(ScornedPortal));
    const auto scorned = Scorned.Project(-90000, 90000);
    const auto closeEnough = [](Point a, Point b) { return std::abs(a.X-b.X) < .00001 && std::abs(a.Y-b.Y) < .00001; };
    assert(closeEnough(mainland.ToUV(MainlandPortal), {0.25631999855476, 0.42419898608810}));
    assert(closeEnough(scorned.ToUV(ScornedPortal), {0.7620233596, 0.8557505899}));
    for (const auto& [projection, world] : std::array{
            std::pair{mainland, MainlandPortal}, std::pair{scorned, ScornedPortal},
            std::pair{overview, MainlandPortal}}) {
        const auto uv = projection.ToUV(world);
        assert(closeEnough(projection.ToWorld(uv), world));
        PS::MapProjection::Viewport view{1040, 420, projection.HalfWidth/projection.HalfHeight};
        view.Zoom = view.FitZoom();
        view.Clamp();
        const auto pixel = view.ToPixel(uv);
        assert(closeEnough(projection.ToWorld(view.ToUV(pixel)), world));
        view.ZoomAt(2, pixel);
        view.Pan({-23, 18});
        assert(closeEnough(projection.ToWorld(view.ToUV(view.ToPixel(uv))), world));
    }
    assert(RegionAt(ScornedPortal) == ScornedTab);
    assert(RegionNameAt(ScornedPortal) == "Rasthin's Rest");
    assert(RegionNameAt({-797000, 688000}) == "Scorned Wilderness");
    assert(RegionNameAt({-848000, 610000}).empty());
    assert(RegionAt({-797000, 688000}) == ScornedTab);
    assert(RegionAt({-848000, 610000}) == OverviewTab); // Art padding is outside the native polygons.
    assert(CoordinateTab({-848000, 610000}) == ScornedTab);
    assert(ContainsSurface({-848000, 610000}));
    const auto edge=Point{-848000,610000};
    assert(closeEnough(edge,scorned.ToWorld(scorned.ToUV(edge))));
    assert(RegionAt(MainlandPortal) >= FirstRegionTab);
    assert(ScornedDisplayOnly({-1500,-21950}));
    assert(!ScornedDisplayOnly(MainlandPortal));
    assert(!ScornedDisplayOnly(ScornedPortal));
    for (size_t tab = 0; tab < TabCount; ++tab) {
        const auto frame = Frame(tab);
        const auto projection = frame.Project(-90000, 90000);
        assert(projection.Valid());
        const auto w = frame.Max.X - frame.Min.X, h = frame.Max.Y - frame.Min.Y;
        if (tab >= FirstRegionTab) {
            for (const auto point : SurfaceAreas[tab - FirstRegionTab].Shape) {
                assert(frame.Contains(point));
                assert(closeEnough(projection.ToUV(point), {(point.X-frame.Min.X)/w, (point.Y-frame.Min.Y)/h}));
            }
        }
        for (const auto size : std::array{Point{1040, 420}, Point{360, 540}}) {
            PS::MapProjection::Viewport fit{size.X, size.Y, w/h};
            fit.Zoom = fit.FitZoom();
            fit.Clamp();
            assert(!fit.WorldAt(projection, {-1, size.Y/2}));
            assert(!fit.WorldAt(projection, {size.X+1, size.Y/2}));
            if (std::abs(size.X/size.Y-w/h)>.01) assert(!fit.WorldAt(projection, {0,0}));
            const Point world{frame.Min.X+.56*w, frame.Min.Y+.43*h}, uv{.56,.43};
            const auto before = fit.WorldAt(projection, fit.ToPixel(uv));
            assert(before && closeEnough(*before, world));
            fit.ZoomAt(fit.FitZoom()*2, fit.ToPixel(uv));
            fit.Pan({13,-18});
            const auto after = fit.WorldAt(projection, fit.ToPixel(uv));
            assert(after && closeEnough(*after, world));
        }
    }
    PS::MapProjection::Viewport view{1000, 500, overview.HalfWidth/overview.HalfHeight};
    view.Zoom = view.FitZoom();
    view.Clamp();
    RasterClip clip;
    assert(ClipRaster(overview, view, Mainland, clip));
    assert(closeEnough(clip.RasterMin, {0, 0}) && closeEnough(clip.RasterMax, {1, 1}));
    assert(closeEnough(clip.ScreenMin, view.ToPixel(overview.ToUV(Mainland.Min))));
    assert(closeEnough(clip.ScreenMax, view.ToPixel(overview.ToUV(Mainland.Max))));
    assert(!ClipRaster(overview, view, Scorned, clip));
    assert(ClipRaster(scorned, view, Scorned, clip));
    assert(clip.ScreenMin.X >= -.00001 && clip.ScreenMin.Y >= -.00001);
    assert(clip.ScreenMax.X <= view.Width + .00001 && clip.ScreenMax.Y <= view.Height + .00001);

    TabState tabs;
    tabs.Following = true;
    tabs.Track(ScornedPortal);
    assert(tabs.Requested == ScornedTab);
    tabs.Show(ScornedTab);
    assert(tabs.Following && tabs.Active == ScornedTab);
    tabs.Show(OverviewTab);
    tabs.Track(ScornedPortal);
    assert(!tabs.Following && tabs.Active == OverviewTab && tabs.Requested == OverviewTab);
}

int main(){
    CheckAtlas();
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
        for(const auto zoom:std::array{view.FitZoom(),1.0,3.0})for(const auto delta:std::array{Point{12,0},Point{-12,0},Point{0,12},Point{0,-12},Point{12,-12}}){
            auto controller=view;controller.Zoom=zoom;controller.ControllerPan=true;controller.Clamp();const auto before=controller.Center;controller.Pan(delta);const auto after=controller.Center;
            assert(delta.X==0?after.X==before.X:(after.X-before.X)*delta.X<0);
            assert(delta.Y==0?after.Y==before.Y:(after.Y-before.Y)*delta.Y<0);
            controller.Clamp();assert(controller.Center.X==after.X&&controller.Center.Y==after.Y);
            controller.Pan({1e9,-1e9});assert(controller.Center.X==0&&controller.Center.Y==1);
        }
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
