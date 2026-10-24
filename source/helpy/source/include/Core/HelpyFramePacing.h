#pragma once
#include <chrono>
namespace PS {
struct HelpyFramePacing {
    using Clock=std::chrono::steady_clock;
    Clock::time_point Next{};
    bool Focused=false;
    bool Due(Clock::time_point now,bool visible,bool focused){
        if(!visible){Next={};Focused=false;return false;}
        if(now<Next&&!(focused&&!Focused))return false;
        Focused=focused;Next=now+(focused?std::chrono::milliseconds(34):std::chrono::milliseconds(100));return true;
    }
};
}
