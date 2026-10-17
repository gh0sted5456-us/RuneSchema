#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>

namespace PS::SlowWork {
struct Report {uint64_t ElapsedMs=0,PeakMs=0,SlowCalls=0;};
// Constant-space, per-call-site diagnostics. A fast frame produces no output;
// repeated stalls produce at most one row per ten seconds, not per actor.
struct Meter {
    uint64_t NextReportMs=0,PeakMs=0,SlowCalls=0;
    std::optional<Report> Observe(uint64_t elapsedMs,uint64_t nowMs) noexcept {
        if(elapsedMs<100)return {};
        PeakMs=std::max(PeakMs,elapsedMs);++SlowCalls;
        if(nowMs<NextReportMs)return {};
        Report result{elapsedMs,PeakMs,SlowCalls};
        PeakMs=SlowCalls=0;NextReportMs=nowMs+10000;return result;
    }
};
}
