#pragma once
#include <chrono>
#include <algorithm>
namespace PS {
struct HelpyFramePacing {
    using Clock=std::chrono::steady_clock;
    Clock::time_point Next{};
    bool Focused=false;
    int Fps=30;
    bool Due(Clock::time_point now,bool visible,bool focused,int fps=30){
        if(!visible){Next={};Focused=false;return false;}
        fps=std::clamp(fps,15,60);
        if(now<Next&&fps==Fps&&!(focused&&!Focused))return false;
        Fps=fps;Focused=focused;Next=now+std::chrono::nanoseconds(1000000000/(focused?fps:10));return true;
    }
    unsigned WaitMs(Clock::time_point now,bool visible)const{
        const auto maximum=visible?16u:50u;
        if(!visible)return maximum;
        if(Next<=now)return 0;
        const auto remaining=std::chrono::ceil<std::chrono::milliseconds>(Next-now).count();
        return std::min(maximum,static_cast<unsigned>(remaining));
    }
};
}
