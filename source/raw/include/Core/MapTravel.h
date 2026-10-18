#pragma once
#include "Core/MapLanding.h"
#include <cstdint>
namespace PS::MapTravel {
// Times are monotonic milliseconds. No sleeps, blocking stream flushes, or
// position changes belong in the preparation phase.
struct Gate {
    enum class Step {Wait,Probe,Expired,Invalidated};
    std::int64_t Began=0,CommittedAt=0;unsigned ReadySamples=0;bool Committed=false,Stopped=false;
    explicit Gate(std::int64_t now=0):Began(now){}
    Step Poll(std::int64_t now,bool valid,bool streamingReady){
        if(Stopped||!valid||now<Began){Stopped=true;return Step::Invalidated;}
        if(now-(Committed?CommittedAt:Began)>=(Committed?60000:30000)){Stopped=true;return Step::Expired;}
        if(Committed)return Step::Wait;
        ReadySamples=streamingReady?std::min(ReadySamples+1,2u):0;
        return ReadySamples>=2&&now-Began>=200?Step::Probe:Step::Wait;
    }
    bool Commit(std::int64_t now){if(Stopped||Committed||now<Began||now-Began>=30000||ReadySamples<2)return false;Committed=true;CommittedAt=now;return true;}
};
inline bool Arrived(MapLanding::Position target,MapLanding::Position actual,double halfHeight){
    return std::isfinite(actual.X)&&std::isfinite(actual.Y)&&std::isfinite(actual.Z)
        &&std::isfinite(halfHeight)&&halfHeight>=1&&halfHeight<=1000
        &&std::abs(target.X-actual.X)<=50&&std::abs(target.Y-actual.Y)<=50
        &&std::abs(target.Z-actual.Z)<=std::max(50.0,halfHeight);
}
}
