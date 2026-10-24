#include "Core/MapLanding.h"
#include "Core/SpawnCatalogSearch.h"
#include <cassert>
using namespace PS;
template<class F> void rejects(F&& f){bool failed=false;try{f();}catch(...){failed=true;}assert(failed);}
int main(){
    MapProjection::Projection map{{0,0},{0,1},{1,0},10000,10000,-90000,90000};int queries=0;
    const auto high=[&](MapProjection::Point at,double top,double bottom){++queries;assert(top==90000&&bottom==-90000);return MapLanding::Position{at.X,at.Y,8000};};
    const auto landing=MapLanding::Resolve(map,{0,0},40,90,high);assert(queries==5&&landing.Z==8098);assert(MapLanding::Matches(landing,landing));assert(!MapLanding::Matches(landing,{0,0,-8000}));
    const auto hill=MapLanding::Resolve(map,{0,0},40,90,[](auto at,double,double){return MapLanding::Position{at.X,at.Y,8000+at.X*.7};});assert(std::abs(hill.Z-8120.4)<.001);
    rejects([&]{MapLanding::Resolve(map,{0,0},40,90,[](auto at,double,double){return MapLanding::Position{at.X,at.Y,8000+at.X*1.2};});});
    rejects([&]{MapLanding::Resolve(map,{0,0},40,90,[](auto at,double,double){return MapLanding::Position{at.X,at.Y,at.X>0?40.:0.};});});
    rejects([&]{MapLanding::Resolve(map,{0,0},40,90,[](auto at,double,double){return MapLanding::Position{at.X,at.Y,at.X>0?100.:0.};});});
    rejects([&]{MapLanding::Resolve(map,{0,0},40,90,[](auto,double,double)->MapLanding::Position{throw std::runtime_error("unloaded");});});
    rejects([&]{MapLanding::Resolve(map,{0,0},0,90,high);});
    rejects([&]{MapLanding::Resolve(map,{0,0},40,90,[](auto at,double,double){return MapLanding::Position{at.X,at.Y,std::numeric_limits<double>::quiet_NaN()};});});
    const auto beside=MapLanding::ResolveNear(map,{0,0},40,90,true,[](auto at,double,double){if(std::abs(at.X)<50)throw std::runtime_error("stone collision");return MapLanding::Position{at.X,at.Y,100};});assert(beside.X==200&&beside.Z==198);
    const auto search=MapLanding::TravelCandidates({0,0},false);assert(search.size()==25&&search.front().X==0&&search.front().Y==0);
    bool escaped=false;for(const auto candidate:search)try{const auto safe=MapLanding::Resolve(map,candidate,40,90,[](auto at,double,double){if(std::hypot(at.X,at.Y)<250)throw std::runtime_error("blocked");return MapLanding::Position{at.X,at.Y,100};});assert(std::hypot(safe.X,safe.Y)<=400.01);escaped=true;break;}catch(...){}
    assert(escaped);for(const auto candidate:MapLanding::TravelCandidates({0,0},true))assert(std::hypot(candidate.X,candidate.Y)<=800.01);
    nlohmann::json ore={{"Name","Iron"},{"Type","Resource"},{"Class","/Game/Mining/BP_IronNode.BP_IronNode_C"},{"ResourceFamily","Mineral"}};
    assert(SpawnCatalogSearch::Matches(ore,"iron ore node")&&SpawnCatalogSearch::Matches(ore,"IRON nodes"));assert(!SpawnCatalogSearch::Matches(ore,"copper node"));
    assert(!SpawnCatalogSearch::Matches({{"Name","Iron ore"},{"Type","Item"}},"iron ore node"));
    assert(SpawnCatalogSearch::Matches({{"Name","Goblin"},{"Type","AI"}},"goblin enemies"));
}
