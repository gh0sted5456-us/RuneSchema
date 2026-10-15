#include "Core/GroundSurface.h"
#include "Core/GroundingRetry.h"
#include "Loader/VendorSpawnGate.h"
#include <cassert>
#include <limits>
#include <vector>
template<class F> bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
    using namespace PS::Grounding;
    auto range=SearchRange::Around(-3500);assert(range.Top==1500&&range.Bottom==-13500);
    range.ObserveCell(-22000,-15000);assert(range.Bottom==-22256&&range.Top==1500);
    assert(range.Contains(-22000)&&!range.Contains(-23000));
    range.ObserveCell(19000,25000);assert(range.Top==25256&&range.Bottom==-22256);
    const auto prior=range;range.ObserveCell(-1e9,1e9);range.ObserveCell(10,-10);range.ObserveCell(0,std::numeric_limits<double>::quiet_NaN());
    assert(range.Top==prior.Top&&range.Bottom==prior.Bottom);
    assert(!range.Contains(std::numeric_limits<double>::infinity()));
    assert(Rejects([]{SearchRange::Around(std::numeric_limits<double>::quiet_NaN());}));
    assert(Rejects([]{SearchRange::Around(0,-1,10);}));
    // A terrain-level placement must not select an overhead bridge/roof.
    const std::vector<double> stacked{900,500,25,0,-400};
    const auto floorAt=[&](double reference){const auto r=FloorRange({1500,-1000},reference);for(double z:stacked)if(r.Contains(z))return z;return -99999.0;};
    assert(floorAt(0)==25); // a local stair tread beats terrain, not the roof
    assert(floorAt(500)==500); // an intentional bridge/deck reference
    assert(floorAt(-400)==-400); // a lower floor is not pulled upstairs
    assert(Rejects([]{FloorRange({100,0},-100);}));
    assert(!Reference(nlohmann::json::object()));
    assert(Reference({{"GroundReferenceZ",-3550}})==-3550);
    assert(Reference(nlohmann::json::object(),123)==123);
    assert(Rejects([]{Reference({{"GroundReferenceZ","$"}});}));
    assert(Rejects([]{Reference({{"GroundReferenceZ",1e20}});}));
    std::vector<bool> calls;
    assert(QuerySurface([&](bool complex){calls.push_back(complex);return true;}));assert(calls==std::vector<bool>{false});
    calls.clear();assert(QuerySurface([&](bool complex){calls.push_back(complex);return complex;}));assert(calls==std::vector<bool>({false,true}));
    calls.clear();assert(!QuerySurface([&](bool complex){calls.push_back(complex);return false;}));assert(calls.size()==2);
    calls.clear();assert(Rejects([&]{QuerySurface([&](bool complex)->bool{calls.push_back(complex);throw std::runtime_error("layout changed");});}));assert(calls.size()==1);
    // The NPC one-shot gate is independent of reversible collision readiness.
    DragonWilds::VendorSpawnGate gate;Retry retry;int rows=0,actors=0;
    const auto attempt=[&](bool hit){if(!hit){retry.Missed();return;}retry.Resolved();if(gate.Begin()){++rows;++actors;}};
    attempt(false);assert(gate.Pending()&&rows==0&&actors==0);
    retry.Advance(1);retry.Advance(1);assert(retry.Ready());attempt(true);attempt(true);
    assert(!gate.Pending()&&rows==1&&actors==1);
    retry={};gate.ResetForMap();assert(gate.Pending()&&!retry.Pending);
}
