#pragma once
#include "Core/DiagnosticCodes.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace PS::RuntimeHealth {
struct Event {Diagnostics::Id Code;std::string Message;};
struct Process {uint32_t Pid=0;uint64_t Created=0;std::optional<double> Cpu,Gpu;std::optional<uint64_t> WorkingSet,PrivateBytes;};
struct Sample {double At=0;std::optional<double> Cpu,Gpu,Memory;Process Helpy,Game;std::string GpuStatus;};
struct Settings {bool Resources=true,Gpu=true,UE4SS=true,Breaks=true;double CpuLimit=90,MemoryLimit=90,GpuLimit=98;unsigned LuaMessages=100,OverlapSeconds=5;bool StartupAlerts=false;};
class PlayTime {
    uint32_t Pid=0;double Last=-1,Elapsed=0;bool Active=false;unsigned NextHour=3;
public:
    double Seconds()const{return Elapsed;}
    std::optional<unsigned> Tick(uint32_t pid,bool active,bool enabled,double now){
        if(!pid||pid!=Pid){Pid=pid;Last=now;Elapsed=0;Active=false;NextHour=3;}
        if(!pid)return {};
        const double gap=now-Last;if(active&&Active&&gap>=0&&gap<=20)Elapsed+=gap;
        Last=now;Active=active;const auto hour=static_cast<unsigned>(Elapsed/3600);
        if(hour<NextHour)return {};NextHour=hour+1;return enabled?std::optional<unsigned>(hour):std::nullopt;
    }
};
inline std::string Reminder(unsigned hour){return "You have been playing for "+(hour==3?std::string("three"):std::to_string(hour))+" hours, consider taking a 10 minute break.";}
class Resources {
    struct Gate{unsigned High=0;bool Reported=false;double Last=-1e9;};std::array<Gate,6> Gates{};
    uint32_t GamePid=0;uint64_t GameCreated=0;std::array<Process,2> Baseline{};double BaselineAt=-1,LastSample=-1,BeganAt=-1;
public:
    std::vector<Event> Review(const Sample& sample,const Settings& settings){
        if(!settings.Resources){*this={};return {};}
        if(GamePid!=sample.Game.Pid||GameCreated!=sample.Game.Created){*this={};GamePid=sample.Game.Pid;GameCreated=sample.Game.Created;}
        if(LastSample>=0&&sample.At-LastSample>20){Gates={};Baseline={};BaselineAt=-1;}
        LastSample=sample.At;if(BeganAt<0)BeganAt=sample.At;
        if(!settings.StartupAlerts&&sample.At-BeganAt<120){Gates={};Baseline={sample.Helpy,sample.Game};BaselineAt=sample.At;return {};}
        std::vector<Event> events;
        const auto check=[&](size_t key,bool high,Diagnostics::Id code,std::string text){auto& gate=Gates[key];
            if(!high){gate.High=0;gate.Reported=false;return;}
            gate.High=std::min(gate.High+1,3u);if(gate.High==3&&!gate.Reported&&sample.At-gate.Last>=600){gate.Reported=true;gate.Last=sample.At;events.push_back({code,std::move(text)});}};
        const auto percent=[](const std::optional<double>& value,double threshold){return value&&std::isfinite(*value)&&*value>=threshold;};
        for(size_t i=0;i<2;++i){const auto& process=i?sample.Game:sample.Helpy;const auto name=i?"Dragonwilds":"Helpy";
            check(i,process.Pid&&percent(process.Cpu,settings.CpuLimit),Diagnostics::Id::ProcessCpuHigh,std::string(name)+" used "+std::to_string(process.Cpu.value_or(0))+"% of total CPU capacity across three samples. This is measured process load, not attribution to a mod.");
            check(i+2,settings.Gpu&&process.Pid&&percent(process.Gpu,settings.GpuLimit),Diagnostics::Id::ProcessGpuHigh,std::string(name)+" busiest GPU engine reached "+std::to_string(process.Gpu.value_or(0))+"% across three samples. High GPU use can be normal; compare frame rate and workload before changing mods.");
        }
        const auto memory=[](const auto& value){return value?std::to_string(*value/1048576)+" MiB":std::string("unavailable");};
        check(4,percent(sample.Memory,settings.MemoryLimit),Diagnostics::Id::SystemMemoryPressure,"System physical memory use is "+std::to_string(sample.Memory.value_or(0))+"%. Helpy working set="+memory(sample.Helpy.WorkingSet)+"; Dragonwilds="+memory(sample.Game.WorkingSet)+". Other processes may account for the remaining pressure.");
        if(BaselineAt<0||sample.At-BaselineAt>=60){
            if(BaselineAt>=0)for(size_t i=0;i<2;++i){const auto& current=i?sample.Game:sample.Helpy;const auto& old=Baseline[i];
                if(current.Pid&&current.Pid==old.Pid&&current.Created==old.Created&&current.PrivateBytes&&old.PrivateBytes&&*current.PrivateBytes>*old.PrivateBytes+256ULL*1048576&&sample.At-Gates[5].Last>=600){
                    Gates[5].Last=sample.At;events.push_back({Diagnostics::Id::ProcessMemoryGrowth,std::string(i?"Dragonwilds":"Helpy")+" private memory grew by "+std::to_string((*current.PrivateBytes-*old.PrivateBytes)/1048576)+" MiB over at least one minute. Loading and caches can explain growth; this alone does not establish a leak."});}}
            Baseline={sample.Helpy,sample.Game};BaselineAt=sample.At;
        }
        return events;
    }
};
class LuaActivity {
    struct Row{std::string Name;bool Managed=false,Error=false,Seen=false,High=false;uint64_t Messages=0;double LastReport=-1e9;};
    std::map<std::string,Row> Rows;std::map<std::string,std::string> Aliases;double Began=-1,SlowAt=-1e9;
    static std::string Lower(std::string text){for(auto& c:text)if(c>='A'&&c<='Z')c=char(c+32);return text;}
public:
    void Add(std::string name,bool managed,const std::string& label={}){
        if(name.empty()||name.size()>128||Rows.size()>=2048||std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32||c==127;}))return;const auto key=Lower(name);Rows[key]={name,managed};
        for(const auto& value:{name,label})if(!value.empty()&&value.size()<=128){const auto alias=Lower(value);const auto found=Aliases.find(alias);if(found==Aliases.end())Aliases[alias]=key;else if(found->second!=key)found->second.clear();}
    }
    void Slow(double now){SlowAt=now;}
    std::string Status(const std::string& name)const{const auto at=Rows.find(Lower(name));return at==Rows.end()?"Not observed":at->second.Error?"Error reported in log":at->second.High?"High log activity observed":at->second.Seen?"Observed log activity":"Not observed";}
    std::optional<Event> Observe(const std::string& line,double now,bool live=true){
        if(line.size()>16384)return {};std::string key;size_t start=0;
        for(unsigned token=0;token<16;++token){start=line.find('[',start);if(start==line.npos)break;const auto end=line.find(']',++start);if(end==line.npos)break;
            if(end-start<=128){const auto found=Aliases.find(Lower(line.substr(start,end-start)));if(found!=Aliases.end()&&!found->second.empty()){key=found->second;break;}}start=end+1;}
        const auto text=Lower(line);if(key.empty()&&(text.find("lua mod")!=text.npos||text.find("[lua-failed]")!=text.npos)){
            const auto quote=line.find_first_of("\"'");if(quote!=line.npos){const auto end=line.find(line[quote],quote+1);if(end!=line.npos&&end-quote<=129){const auto found=Aliases.find(Lower(line.substr(quote+1,end-quote-1)));if(found!=Aliases.end())key=found->second;}}}
        if(key.empty())return {};auto& row=Rows.at(key);if(live){row.Seen=true;row.Messages=std::min<uint64_t>(row.Messages+1,1000000);}
        const bool failed=text.find("[lua-failed]")!=text.npos||text.find("error executing script")!=text.npos||text.find("lua error")!=text.npos||text.find("failed to start lua")!=text.npos;
        if(row.Managed&&failed){row.Error=true;if(live&&now-row.LastReport>=600){row.LastReport=now;return Event{Diagnostics::Id::LuaCompanionError,row.Name+" reported a Lua startup/execution error. See the original UE4SS log; this does not assert that every function in the companion failed."};}}
        return {};
    }
    std::vector<Event> Review(double now,const Settings& settings){
        if(!settings.UE4SS){for(auto& [key,row]:Rows)row.Messages=0;Began=now;return {};}
        if(Began<0){Began=now;return {};}if(now-Began<5)return {};std::vector<Event> events;
        for(auto& [key,row]:Rows){row.High=row.Messages>=std::clamp(settings.LuaMessages,10u,5000u);if(row.High&&now-row.LastReport>=600&&events.size()<3){row.LastReport=now;
                events.push_back({Diagnostics::Id::LuaLogActivity,row.Name+" emitted "+std::to_string(row.Messages)+" attributed log messages in "+std::to_string(now-Began)+" seconds. This measures log output, not callbacks, CPU time or polling frequency."});
                if(now-SlowAt<=std::clamp(settings.OverlapSeconds,1u,30u)&&events.size()<3)events.push_back({Diagnostics::Id::LuaActivityOverlap,row.Name+" log activity overlapped a measured RuneSchema slow-work report within the configured window. Correlation does not establish which mod caused the delay."});}
            row.Messages=0;}
        Began=now;return events;
    }
};
}
