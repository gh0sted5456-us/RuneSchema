#pragma once
#include <chrono>
#include <optional>

namespace PS {
struct FrameRate {
    using Clock=std::chrono::steady_clock;
    Clock::time_point Started{},Last{};
    unsigned Frames=0;
    bool Active=false;
    std::optional<double> Sample(Clock::time_point now,std::chrono::milliseconds interval=std::chrono::seconds(1)){
        if(!Active||now<Last||now-Last>std::chrono::seconds(2)){
            Started=Last=now;Frames=0;Active=true;return {};
        }
        Last=now;++Frames;
        const auto elapsed=now-Started;
        if(elapsed<interval)return {};
        const double fps=Frames/std::chrono::duration<double>(elapsed).count();
        Started=now;Frames=0;return fps;
    }
};
}
