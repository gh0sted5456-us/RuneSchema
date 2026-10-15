#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace PS::Grounding {
struct SurfacePending final:std::runtime_error {SurfacePending():std::runtime_error("Ground surface is not available yet") {}};
// A missed collision trace is not permission to guess Z. Retry a bounded burst
// after streaming, then sleep until that cell is shown again.
struct Retry {
    bool Pending=false,Announced=false;
    unsigned Attempts=0;
    double Wait=0;
    bool Ready()const{return !Pending||(Attempts<8&&Wait<=0);}
    bool Missed(){Pending=true;++Attempts;Wait=std::min(30.0,std::ldexp(1.0,std::min(Attempts,5u)));const bool notify=!Announced;Announced=true;return notify;}
    void Advance(double seconds){if(Pending&&std::isfinite(seconds)&&seconds>0)Wait=std::max(0.0,Wait-std::min(seconds,1.0));}
    void CellShown(){if(Pending&&Attempts>=8){Attempts=0;Wait=0;}}
    void Resolved(){Pending=false;Attempts=0;Wait=0;}
};
}
