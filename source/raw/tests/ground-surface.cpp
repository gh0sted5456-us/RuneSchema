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
