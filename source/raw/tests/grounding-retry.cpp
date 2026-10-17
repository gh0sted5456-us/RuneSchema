#include "Core/GroundingRetry.h"
#include <cassert>
#include <limits>
int main(){
    using namespace PS::Grounding;Retry r;assert(r.Ready()&&!r.Pending);
    assert(r.Missed());assert(r.Pending&&!r.Ready()&&r.Wait==2);
    r.Advance(-1);r.Advance(std::numeric_limits<double>::quiet_NaN());assert(r.Wait==2);
    r.Advance(999);assert(r.Wait==1);r.Advance(1);assert(r.Ready());
    for(int i=1;i<8;++i){assert(!r.Missed());for(int tick=0;tick<30;++tick)r.Advance(1);}
    assert(r.Pending&&!r.Ready()&&r.Attempts==8);r.Advance(1000);assert(!r.Ready());
    // Late physics readiness must recover without a second cell notification.
    // Encounter deadlines remain bounded separately from persistent placement.
    assert(r.PlacementReady());
    for(int i=0;i<32;++i){assert(!r.Missed());assert(!r.PlacementReady());for(int tick=0;tick<30;++tick)r.Advance(1);assert(r.PlacementReady()&&!r.Ready());}
    assert(r.Attempts==40);r.Resolved();assert(r.PlacementReady()&&r.Ready()&&!r.Pending);
    for(int i=0;i<8;++i)r.Missed();
    r.CellShown();assert(r.Ready()&&r.Pending&&r.Attempts==0);assert(!r.Missed());
    r.Resolved();assert(!r.Pending&&r.Ready());r={};assert(r.Missed());
    try{throw SurfacePending();}catch(const SurfacePending&){return 0;}return 1;
}
