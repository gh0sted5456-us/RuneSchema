#pragma once
#include "Core/RuntimeHealth.h"
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <chrono>
#include <string_view>

// One serialized file worker owns the PDH query and process baselines.
namespace PS::WindowsResourceSample {
inline double Now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline uint64_t Time(FILETIME value){return (uint64_t(value.dwHighDateTime)<<32)|value.dwLowDateTime;}
inline std::optional<double> Cpu(uint64_t before,uint64_t after,double seconds,unsigned processors){
    if(after<before||seconds<=0||!processors)return {};return std::clamp(double(after-before)/(seconds*10000000.0*processors)*100,0.0,100.0);
}
inline std::optional<uint32_t> GpuPid(std::wstring_view name){
    if(!name.starts_with(L"pid_"))return {};uint64_t pid=0;size_t i=4;
    for(;i<name.size()&&name[i]>=L'0'&&name[i]<=L'9';++i){pid=pid*10+name[i]-L'0';if(pid>UINT32_MAX)return {};}
    return i>4&&i<name.size()&&name[i]==L'_'?std::optional<uint32_t>(static_cast<uint32_t>(pid)):std::nullopt;
}
class Sampler {
    struct Previous{uint32_t Pid=0;uint64_t Created=0,Ticks=0;double At=0;};std::array<Previous,2> Processes{};
    PDH_HQUERY Query=nullptr;PDH_HCOUNTER Counter=nullptr;bool GpuAttempted=false,Warm=false;
    uint64_t Idle=0,Kernel=0,User=0;bool SystemWarm=false;
    RuntimeHealth::Process Process(uint32_t pid,size_t slot,double now){
        RuntimeHealth::Process result;result.Pid=pid;auto& old=Processes[slot];if(!pid){old={};return result;}
        struct Handle{HANDLE Value;~Handle(){if(Value)CloseHandle(Value);}} handle{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ,FALSE,pid)};
        if(!handle.Value)handle.Value=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!handle.Value){old={};return result;}
        FILETIME created{},exit{},kernel{},user{};
        if(GetProcessTimes(handle.Value,&created,&exit,&kernel,&user)){
            result.Created=Time(created);const auto ticks=Time(kernel)+Time(user);
            if(old.Pid==pid&&old.Created==result.Created)result.Cpu=Cpu(old.Ticks,ticks,now-old.At,GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
            old={pid,result.Created,ticks,now};
        }else old={};
        PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
        if(GetProcessMemoryInfo(handle.Value,reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory))){result.WorkingSet=memory.WorkingSetSize;result.PrivateBytes=memory.PrivateUsage;}
        return result;
    }
    void Gpu(RuntimeHealth::Sample& sample,bool enabled){
        if(!enabled){if(Query)PdhCloseQuery(Query);Query=nullptr;Counter=nullptr;GpuAttempted=Warm=false;sample.GpuStatus="GPU counters disabled";return;}
        if(!GpuAttempted){GpuAttempted=true;
            if(PdhOpenQueryW(nullptr,0,&Query)!=ERROR_SUCCESS||PdhAddEnglishCounterW(Query,L"\\GPU Engine(*)\\Utilization Percentage",0,&Counter)!=ERROR_SUCCESS){
                if(Query)PdhCloseQuery(Query);Query=nullptr;Counter=nullptr;
            }
        }
        if(!Query||!Counter){sample.GpuStatus="GPU counters unavailable on this driver/system";return;}
        if(PdhCollectQueryData(Query)!=ERROR_SUCCESS){sample.GpuStatus="GPU samples unavailable";return;}
        if(!Warm){Warm=true;sample.GpuStatus="Collecting GPU baseline";return;}
        for(unsigned attempt=0;attempt<2;++attempt){DWORD bytes=0,count=0;
            const auto size=PdhGetFormattedCounterArrayW(Counter,PDH_FMT_DOUBLE,&bytes,&count,nullptr);
            if(size!=PDH_MORE_DATA||!bytes||bytes>4*1024*1024){sample.GpuStatus="GPU samples unavailable";return;}
            std::vector<PDH_FMT_COUNTERVALUE_ITEM_W> storage((bytes+sizeof(PDH_FMT_COUNTERVALUE_ITEM_W)-1)/sizeof(PDH_FMT_COUNTERVALUE_ITEM_W));
            const auto status=PdhGetFormattedCounterArrayW(Counter,PDH_FMT_DOUBLE,&bytes,&count,storage.data());if(status==PDH_MORE_DATA)continue;
            if(status!=ERROR_SUCCESS||count>storage.size()){sample.GpuStatus="GPU samples unavailable";return;}
            std::map<std::wstring,double> engines;
            for(DWORD i=0;i<count;++i){const auto& item=storage[i];if(!item.szName||(item.FmtValue.CStatus!=PDH_CSTATUS_VALID_DATA&&item.FmtValue.CStatus!=PDH_CSTATUS_NEW_DATA)||!std::isfinite(item.FmtValue.doubleValue))continue;
                const auto value=std::clamp(item.FmtValue.doubleValue,0.0,100.0);const std::wstring_view name(item.szName);const auto pid=GpuPid(name);if(!pid)continue;
                if(*pid==sample.Helpy.Pid)sample.Helpy.Gpu=std::max(sample.Helpy.Gpu.value_or(0),value);
                if(sample.Game.Pid&&*pid==sample.Game.Pid)sample.Game.Gpu=std::max(sample.Game.Gpu.value_or(0),value);
                if(const auto engine=name.find(L"_luid_");engine!=name.npos)engines[std::wstring(name.substr(engine))]+=value;
            }
            for(const auto& [engine,value]:engines)sample.Gpu=std::max(sample.Gpu.value_or(0),std::min(value,100.0));
            sample.GpuStatus="Busiest sampled engine; unavailable process instances remain unmeasured";return;
        }sample.GpuStatus="GPU instances changed while reading";
    }
public:
    Sampler()=default;Sampler(const Sampler&)=delete;Sampler& operator=(const Sampler&)=delete;
    ~Sampler(){if(Query)PdhCloseQuery(Query);}
    RuntimeHealth::Sample Read(uint32_t game,bool gpu){
        RuntimeHealth::Sample sample;sample.At=Now();sample.Helpy=Process(GetCurrentProcessId(),0,sample.At);sample.Game=Process(game,1,sample.At);
        MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);if(GlobalMemoryStatusEx(&memory))sample.Memory=memory.dwMemoryLoad;
        FILETIME idle{},kernel{},user{};
        if(GetActiveProcessorGroupCount()==1&&GetSystemTimes(&idle,&kernel,&user)){
            const auto currentIdle=Time(idle),currentKernel=Time(kernel),currentUser=Time(user);
            if(SystemWarm&&currentKernel>=Kernel&&currentUser>=User&&currentIdle>=Idle){const auto total=currentKernel-Kernel+currentUser-User;
                if(total)sample.Cpu=std::clamp(100.0*(1-double(currentIdle-Idle)/total),0.0,100.0);}
            Idle=currentIdle;Kernel=currentKernel;User=currentUser;SystemWarm=true;
        }Gpu(sample,gpu);return sample;
    }
};
}
