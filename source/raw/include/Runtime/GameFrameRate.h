#pragma once
#include "Core/FrameRate.h"
#include <atomic>
#include <cstdint>

namespace PS::GameFrameRate {
inline std::atomic<double> Fps{0};
inline std::atomic<uint64_t> UpdatedMs{0};
inline uint64_t Stamp(FrameRate::Clock::time_point now){return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();}
inline void Tick(){
    // Reuse the engine tick already owned by RuneSchema; no reflection or new hook.
    static FrameRate counter;
    const auto now=FrameRate::Clock::now();
    if(const auto fps=counter.Sample(now)){Fps.store(*fps);UpdatedMs.store(Stamp(now));}
}
inline bool Available(){const auto stamp=UpdatedMs.load();return stamp&&Stamp(FrameRate::Clock::now())-stamp<3000;}
}
