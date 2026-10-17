#pragma once
#include "Core/SlowWork.h"
#include "Utility/Logging.h"
#include <chrono>
#include <utility>

namespace PS::SlowWork {
class Scope {
    Meter& Stats;
    const char* Phase;
    std::chrono::steady_clock::time_point Began=std::chrono::steady_clock::now();
public:
    Scope(Meter& meter,const char* phase):Stats(meter),Phase(phase){}
    ~Scope() noexcept {
        try {
            const auto now=std::chrono::steady_clock::now();
            const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(now-Began).count();
            const auto stamp=std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            if(const auto report=Stats.Observe(static_cast<uint64_t>(elapsed),static_cast<uint64_t>(stamp))){
                if(report->PeakMs>=1000)
                    PS::Log<RC::LogLevel::Warning>(STR("[PERFORMANCE][SLOW-WORK] phase={} elapsed_ms={} peak_ms={} slow_calls={}; attribution only, not a network rejection.\n"),
                        PS::ToWideSafe(Phase),report->ElapsedMs,report->PeakMs,report->SlowCalls);
                else PS::Log<RC::LogLevel::Verbose>(STR("[PERFORMANCE][SLOW-WORK] phase={} elapsed_ms={} peak_ms={} slow_calls={}; detailed timing, no gameplay/network rejection.\n"),
                        PS::ToWideSafe(Phase),report->ElapsedMs,report->PeakMs,report->SlowCalls);
            }
        }catch(...){} // Diagnostics must not turn a slow callback into a crash.
    }
};
template<class Fn> decltype(auto) Measure(const char* phase,Fn&& function){
    // Each lambda expression is a distinct type/call site. No global map,
    // locks, file I/O or retained UObject references on the frame path.
    static thread_local Meter meter;
    Scope scope(meter,phase);return std::forward<Fn>(function)();
}
}
