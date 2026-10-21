#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <vector>
#include <cmath>
namespace PS::AdditionalLoggingInfo {
inline constexpr unsigned Enabled=1,FilePaths=2,Region=4,NetworkRates=8,SavePaths=16,AverageLatency=32;
struct Counter{uint64_t Id,Received,Sent;};
struct Rates{double DownloadMbps=0,UploadMbps=0;size_t Interfaces=0;};
class RateWindow {
    std::map<uint64_t,Counter> Previous;std::optional<double> Time;
public:
    void Reset(){Previous.clear();Time.reset();}
    std::optional<Rates> Sample(double now,const std::vector<Counter>& counters){
        if(!std::isfinite(now)||counters.size()>4096){Reset();return {};}
        Rates rates;const auto elapsed=Time?now-*Time:0;std::map<uint64_t,Counter> next;
        for(const auto& row:counters){if(!next.emplace(row.Id,row).second){Reset();return {};}
            const auto found=Previous.find(row.Id);if(elapsed<=0||found==Previous.end())continue;const auto& old=found->second;
            if(row.Received<old.Received||row.Sent<old.Sent)continue;
            rates.DownloadMbps+=double(row.Received-old.Received)*8/elapsed/1000000.;
            rates.UploadMbps+=double(row.Sent-old.Sent)*8/elapsed/1000000.;++rates.Interfaces;
        }
        Previous=std::move(next);Time=now;
        if(!rates.Interfaces)return {};return rates;
    }
};
struct LatencyMean {
    double Sum=0;size_t Samples=0;
    bool Add(double milliseconds){if(!std::isfinite(milliseconds)||milliseconds<=0||milliseconds>60000)return false;Sum+=milliseconds;++Samples;return true;}
    std::optional<double> Value()const {return Samples?std::optional<double>(Sum/Samples):std::nullopt;}
};
}
