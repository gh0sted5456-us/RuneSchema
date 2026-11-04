#pragma once
#include "Core/ProcessExitFuture.h"
#include "Core/WindowsResourceSample.h"
#include "Core/UE4SSInventory.h"
#include "Core/DiagnosticLogPaths.h"
#include "Runtime/HostServices.h"
#include <future>
#include <functional>
#include <memory>

namespace PS::HelpyRuntimeMonitor {
using Json=nlohmann::json;
struct Result {uint32_t Pid=0;std::optional<RuntimeHealth::Sample> Sample;Json Inventory;std::string Error;};
inline ProcessExitFuture<Result> Worker;
inline std::shared_ptr<WindowsResourceSample::Sampler> Sampler;
inline std::optional<RuntimeHealth::Sample> Latest;
inline RuntimeHealth::Resources ResourcePolicy;
inline RuntimeHealth::LuaActivity Lua;
inline RuntimeHealth::PlayTime Play;
inline uint32_t CurrentPid=~uint32_t{},InventoryPid=~uint32_t{};
inline double Next=0,EventWindow=0;inline unsigned EventCount=0,Suppressed=0;
inline std::vector<RuntimeHealth::Event> PendingLogs;
inline std::function<void(const RuntimeHealth::Event&)> OnEvent;
inline std::string Status;
inline bool InventoryReady=false;
inline bool Write(const std::filesystem::path& root,const RuntimeHealth::Event& event){
    try{const auto path=DiagnosticLogPaths::Directory(root)/"RuneSchema.log";CacheBundleIO::Safe(path);
        const auto& code=Diagnostics::Get(event.Code);SYSTEMTIME time{};GetSystemTime(&time);char stamp[48]{};
        std::snprintf(stamp,sizeof(stamp),"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds);
        auto message=event.Message.substr(0,2048);for(auto& c:message)if(c=='\r'||c=='\n'||c=='\0')c=' ';
        const std::string line="["+std::string(stamp)+"] [RuneSchema] ["+std::string(code.Severity)+"] ["+std::string(code.Code)+"] "+std::string(code.Title)+": "+message+"\n";
        const auto file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
        DWORD written=0;const bool ok=WriteFile(file,line.data(),static_cast<DWORD>(line.size()),&written,nullptr)&&written==line.size();CloseHandle(file);return ok;
    }catch(...){return false;}
}
inline void Emit(RuntimeHealth::Event event,double now=WindowsResourceSample::Now()){
    if(now-EventWindow>=5){EventWindow=now;EventCount=0;}
    if(EventCount>=8||PendingLogs.size()>=128){++Suppressed;return;}++EventCount;
    if(OnEvent)OnEvent(event);PendingLogs.push_back(std::move(event));
}
inline void ObserveUE4SS(const std::string& line,bool live,const RuntimeHealth::Settings& settings){
    if(settings.UE4SS)if(auto event=Lua.Observe(line,WindowsResourceSample::Now(),live))Emit(std::move(*event));
}
inline void SlowWork(){Lua.Slow(WindowsResourceSample::Now());}
inline void Poll(const RuntimeHealth::Settings& settings,uint32_t pid,bool gameplay,bool visible){
    try{
    const auto now=WindowsResourceSample::Now();
    if(pid!=CurrentPid){CurrentPid=pid;InventoryPid=~uint32_t{};InventoryReady=false;Lua={};ResourcePolicy={};Latest.reset();Next=0;}
    if(auto hour=Play.Tick(pid,gameplay,settings.Breaks,now))Emit({Diagnostics::Id::PlayTimeReminder,RuntimeHealth::Reminder(*hour)},now);
    if(Worker.valid()&&Worker.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
        try{auto result=Worker.get();Status=result.Error;if(!result.Error.empty()&&!InventoryReady){InventoryPid=~uint32_t{};Next=now+30;}if(result.Pid==pid){
            if(result.Sample&&settings.Resources){Latest=std::move(result.Sample);for(auto& event:ResourcePolicy.Review(*Latest,settings))Emit(std::move(event),now);}
            if(result.Inventory.is_array()){Lua={};for(const auto& row:result.Inventory)Lua.Add(row.at("Name").get<std::string>(),row.at("Managed").get<bool>(),row.at("Metadata").value("name",std::string{}));InventoryReady=true;}
        }}catch(const std::exception& error){Status=error.what();}
    }
    for(auto& event:Lua.Review(now,settings))Emit(std::move(event),now);
    if(!settings.Resources){ResourcePolicy={};Latest.reset();Sampler.reset();}
    if(Worker.valid()||now<Next)return;
    const bool sample=settings.Resources&&(pid||visible),inventory=settings.UE4SS&&InventoryPid!=pid;
    if(!sample&&!inventory&&PendingLogs.empty())return;Next=now+5;
    if(sample&&!Sampler)Sampler=std::make_shared<WindowsResourceSample::Sampler>();const auto reader=Sampler;const auto root=HostServices::ModDirectory();
    auto logs=std::move(PendingLogs);PendingLogs.clear();if(inventory)InventoryPid=pid;
    Worker=std::async(std::launch::async,[pid,sample,inventory,reader,root,gpu=settings.Gpu,logs=std::move(logs)]{
        Result result;result.Pid=pid;
        try{if(sample)result.Sample=reader->Read(pid,gpu);if(inventory)result.Inventory=UE4SSInventory::Read(root).at("Rows");}
        catch(const std::exception& error){result.Error=error.what();}
        for(const auto& event:logs)if(!Write(root,event))result.Error="Some diagnostic messages could not be appended to RuneSchema.log; file history is unavailable for those messages.";
        return result;
    });
    }catch(const std::exception& error){Status=error.what();InventoryPid=~uint32_t{};Next=WindowsResourceSample::Now()+30;}
    catch(...){Status="Runtime monitoring is unavailable; ordinary Helpy tools remain available.";Next=WindowsResourceSample::Now()+30;}
}
inline void Shutdown(){
    if(ProcessExitInProgress.load()){PendingLogs.clear();Sampler.reset();OnEvent={};return;}
    if(Worker.valid())try{(void)Worker.get();}catch(...){};
    try{const auto root=HostServices::ModDirectory();for(const auto& event:PendingLogs)(void)Write(root,event);}catch(...){}PendingLogs.clear();Sampler.reset();OnEvent={};
}
}
