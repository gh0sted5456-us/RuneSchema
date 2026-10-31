#pragma once
#include <chrono>
#include <algorithm>
#include <cstdint>
namespace PS {
struct HelpyPushPacing {
    std::uint64_t Next=0;
    unsigned Delay(std::uint64_t now)const{return now<Next?static_cast<unsigned>(std::min<std::uint64_t>(40,Next-now)):0;}
    void Polled(std::uint64_t now){Next=now+40;}
};
struct HelpyFramePacing {
    using Clock=std::chrono::steady_clock;
    Clock::time_point Next{};
    bool Focused=false;
    int Fps=60;
    bool Due(Clock::time_point now,bool visible,bool focused,int fps=60){
        if(!visible){Next={};Focused=false;return false;}
        fps=std::clamp(fps,15,144);
        const bool reset=Next==Clock::time_point{}||fps!=Fps||focused!=Focused;
        if(!reset&&now<Next)return false;
        const auto period=std::chrono::nanoseconds(1000000000/(focused?fps:10));
        Next=reset||now-Next>=period?now+period:Next+period;
        Fps=fps;Focused=focused;return true;
    }
    unsigned WaitMs(Clock::time_point now,bool visible)const{
        const auto maximum=visible&&Focused?16u:50u;
        if(!visible)return maximum;
        if(Next<=now)return 0;
        const auto remaining=std::chrono::ceil<std::chrono::milliseconds>(Next-now).count();
        return std::min(maximum,static_cast<unsigned>(remaining));
    }
};
}
