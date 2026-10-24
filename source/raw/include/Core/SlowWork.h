#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>

namespace PS::SlowWork {
struct Report {uint64_t ElapsedMs=0,PeakMs=0,SlowCalls=0,Calls=0,TotalUs=0,PeakUs=0;};
// Constant-space aggregates, at most one row per ten seconds per call site.
struct Meter {
    uint64_t NextReportMs=0,PeakUs=0,SlowCalls=0,Calls=0,TotalUs=0;
    bool Started=false;
    std::optional<Report> ObserveUs(uint64_t elapsedUs,uint64_t nowMs) noexcept {
        if(!Started){Started=true;NextReportMs=nowMs+(elapsedUs>=1000000?0:10000);}
        PeakUs=std::max(PeakUs,elapsedUs);TotalUs+=elapsedUs;++Calls;if(elapsedUs>=2000)++SlowCalls;
        if(nowMs<NextReportMs)return {};
        Report result{elapsedUs/1000,(PeakUs+999)/1000,SlowCalls,Calls,TotalUs,PeakUs};
        PeakUs=SlowCalls=Calls=TotalUs=0;NextReportMs=nowMs+10000;
        if(result.PeakUs<2000&&result.TotalUs/result.Calls<500)return {};
        return result;
    }
    std::optional<Report> Observe(uint64_t elapsedMs,uint64_t nowMs) noexcept {return ObserveUs(elapsedMs*1000,nowMs);}
};
}
