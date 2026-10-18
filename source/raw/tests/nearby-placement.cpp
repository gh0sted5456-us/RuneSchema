#include "Core/NearbyPlacement.h"
#include "Core/GroundSurface.h"
#include <cassert>
#include <limits>
#include <set>
template<class F>bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
    using namespace PS::Grounding;
    int calls=0;
    auto exact=Nearby({},[&](double x,double y){++calls;return Site{SiteState::Safe,{x,y,10}};});
    assert(exact&&calls==1&&exact->Position[0]==0&&exact->Position[1]==0);
    calls=0;assert(!Nearby({},[&](double,double){++calls;return Site{SiteState::Pending,{}};}));assert(calls==1);
    calls=0;assert(!Nearby({0},[&](double,double){++calls;return Site{SiteState::Blocked,{}};}));assert(calls==1);
    calls=0;auto migrated=Nearby({},[&](double x,double y){++calls;return Site{calls==3?SiteState::Safe:SiteState::Blocked,{x,y,25}};});
    assert(migrated&&calls==3);assert(std::abs(std::hypot(migrated->Position[0],migrated->Position[1])-100)<0.001);
    std::set<std::array<double,2>> seen;
    calls=0;assert(!Nearby({600},[&](double x,double y){++calls;assert(std::hypot(x,y)<=600.001);assert(seen.insert({x,y}).second);return Site{SiteState::Blocked,{}};}));assert(calls==25);
    // Unavailable alternatives are skipped; they do not turn a failed query
    // into invented coordinates. A binding exception aborts immediately.
    calls=0;auto late=Nearby({150},[&](double x,double y){++calls;return Site{calls==1?SiteState::Blocked:(calls==4?SiteState::Safe:SiteState::Pending),{x,y,0}};});assert(late&&calls==4);
    calls=0;assert(Rejects([&]{Nearby({},[&](double,double)->Site{++calls;throw std::runtime_error("native contract changed");});}));assert(calls==1);
    const double nan=std::numeric_limits<double>::quiet_NaN();
    assert(Rejects([&]{Nearby({},[&](double,double){return Site{SiteState::Safe,{nan,0,0}};});}));
    for(const auto& v:{nlohmann::json(-1),nlohmann::json(1001),nlohmann::json("300"),nlohmann::json(false)})
        assert(Rejects([&]{NearbyPolicy::Parse({{"GroundMigrationRadius",v}});}));
    assert(NearbyPolicy::Parse(nlohmann::json::object()).Radius==300);
    assert(NearbyPolicy::Parse({{"GroundMigrationRadius",0}}).Radius==0);
    assert(!Clearance(nlohmann::json::object()));
    auto footprint=Clearance({{"GroundClearance",{{"Radius",50},{"HalfHeight",100}}}});assert(footprint&&footprint->Radius==50&&footprint->HalfHeight==100);
    assert(Rejects([]{Clearance({{"GroundClearance",{{"Radius",100},{"HalfHeight",50}}}});}));
    assert(Rejects([]{Clearance({{"GroundClearance",{{"Radius",20},{"HalfHeight",50},{"Typo",1}}}});}));
    const PS::SpawnFootprint::Reservation enemy{{0,0,100},{40,90}};
    assert(PS::SpawnFootprint::Overlaps(enemy,{{20,20,100},{40,90}}));
    assert(!PS::SpawnFootprint::Overlaps(enemy,{{100,0,100},{40,90}}));
    assert(!PS::SpawnFootprint::Overlaps(enemy,{{0,0,300},{40,90}}));
    std::vector<PS::SpawnFootprint::Reservation> wave;
    for(int i=0;i<3;++i){
        auto member=Nearby({},[&](double x,double y){
            PS::SpawnFootprint::Reservation candidate{{x,y,100},{40,90}};
            for(const auto& reserved:wave)if(PS::SpawnFootprint::Overlaps(candidate,reserved))return Site{SiteState::Blocked,{}};
            return Site{SiteState::Safe,candidate.Centre};
        });assert(member);wave.push_back({member->Position,{40,90}});
    }
    assert(wave[0].Centre!=wave[1].Centre&&wave[1].Centre!=wave[2].Centre);
    // Fixed local floor window admits stairs but never a roof/bridge/far cave.
    const auto floor=FloorRange(SearchRange::Around(1000),1000);
    assert(floor.Contains(1045)&&floor.Contains(700));assert(!floor.Contains(1300)&&!floor.Contains(699));
    assert(SameNearbyFloor(1000,1045)&&SameNearbyFloor(1000,900));
    assert(!SameNearbyFloor(1000,1046)&&!SameNearbyFloor(1000,899));
    assert(!SameNearbyFloor(1000,700)); // nearby relocation must not drop to a lower storey
}
