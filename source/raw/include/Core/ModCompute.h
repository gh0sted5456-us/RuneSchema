#pragma once
#include "RuneSchemaModApi.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace PS::ModCompute {
inline bool Token(const std::string& text){return !text.empty()&&text.size()<=96&&text!="."&&text!=".."&&std::all_of(text.begin(),text.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-';});}
inline std::string Text(const char* text){if(!text)throw std::runtime_error("Missing native provider ID");size_t length=0;while(length<=96&&text[length])++length;if(length>96)throw std::runtime_error("Native provider ID is too long");return {text,length};}
struct Completion {uint64_t Component=0,Ticket=0,Generation=0,ResourceGeneration=0;bool Ready=false,Success=false;std::vector<double> Values;std::string Error;};
class Broker {
    struct Owner {std::string Plugin,Mod;};
    struct Resource {uint64_t Handle=0,Provider=0,Generation=0;std::string Id;const void* Value=nullptr;bool Retired=false;};
    struct Request {uint64_t Component=0,Ticket=0,Generation=0;std::shared_ptr<Resource> Data;std::vector<double> Input;};
    struct Provider {
        uint64_t Handle=0,ActiveResource=0;Owner* Bound=nullptr;std::string Id,Type;RuneSchemaModProviderV1 Api{};
        bool Stopping=false;std::deque<Request> Queue;std::condition_variable Wake;std::thread Worker;
    };
    struct Session {std::string Owner;uint64_t Generation=0,Latest=0,Active=0;std::optional<Request> Pending;std::shared_ptr<Resource> LatestData;Completion Result;};
    std::mutex Gate;std::condition_variable Drained;
    std::map<std::string,std::unique_ptr<Owner>> Owners;
    std::map<uint64_t,std::unique_ptr<Provider>> Providers;
    std::map<uint64_t,std::shared_ptr<Resource>> Resources;
    std::map<uint64_t,Session> Sessions;std::map<uint64_t,Completion> Completed;std::atomic<bool> CompletionReady=false;
    uint64_t Sequence=0;
    uint64_t Next(){if(Sequence==INT64_MAX)throw std::runtime_error("Native service handle space exhausted");return ++Sequence;}
    Owner* Bound(void* context){for(const auto& [id,owner]:Owners)if(owner.get()==context)return owner.get();throw std::runtime_error("Native mod owner is unavailable");}
    void Finish(const Request& request,Completion result){
        const auto found=Sessions.find(request.Component);if(found==Sessions.end())return;auto& session=found->second;
        if(session.Active==request.Ticket)session.Active=0;
        if(session.Latest==request.Ticket){session.Result=result;Completed.insert_or_assign(request.Component,std::move(result));CompletionReady.store(true,std::memory_order_release);}
        if(session.Pending){auto pending=std::move(*session.Pending);session.Pending.reset();Schedule(std::move(pending));}
    }
    void Schedule(Request request){
        auto& session=Sessions.at(request.Component);const auto provider=Providers.find(request.Data->Provider);
        if(request.Data->Retired||provider==Providers.end()||provider->second->Stopping){
            Finish(request,{request.Component,request.Ticket,request.Generation,request.Data->Generation,true,false,{},"Native resource is unavailable"});return;
        }
        session.Active=request.Ticket;provider->second->Queue.push_back(std::move(request));provider->second->Wake.notify_one();
    }
    void Work(Provider* provider){
        std::unique_lock lock(Gate);
        for(;;){
            provider->Wake.wait(lock,[&]{return provider->Stopping||!provider->Queue.empty();});
            if(provider->Stopping)break;
            auto request=std::move(provider->Queue.front());provider->Queue.pop_front();
            if(!Sessions.contains(request.Component)||request.Data->Retired){if(Sessions.contains(request.Component))Finish(request,{request.Component,request.Ticket,request.Generation,request.Data->Generation,true,false,{},"Native resource is unavailable"});continue;}
            provider->ActiveResource=request.Data->Handle;lock.unlock();
            Completion result{request.Component,request.Ticket,request.Generation,request.Data->Generation,true,false};
            std::array<double,RS_MOD_MAX_RESULTS> values{};std::array<char,RS_MOD_ERROR_BYTES> error{};uint32_t count=0;
            try{
                const auto code=provider->Api.Evaluate(provider->Api.Context,request.Data->Value,request.Input.data(),static_cast<uint32_t>(request.Input.size()),request.Generation,
                    values.data(),provider->Api.MaxResultCount,&count,error.data(),static_cast<uint32_t>(error.size()));
                if(code!=RS_MOD_OK){const auto end=std::find(error.begin(),error.end(),'\0');result.Error=end==error.end()?"Provider error is not terminated":std::string(error.begin(),end);if(result.Error.empty())result.Error="Native provider rejected the calculation";try{(void)nlohmann::json(result.Error).dump();}catch(...){result.Error="Native provider returned invalid UTF-8 error text";}}
                else if(count>provider->Api.MaxResultCount||!std::all_of(values.begin(),values.begin()+count,[](double v){return std::isfinite(v);}))result.Error="Native provider returned an invalid result";
                else{result.Success=true;result.Values.assign(values.begin(),values.begin()+count);}
            }catch(...){result.Error="Native provider threw an exception";}
            lock.lock();provider->ActiveResource=0;
            if(request.Data->Retired||provider->Stopping){result.Success=false;result.Values.clear();result.Error="Native resource was retired";}
            Finish(request,std::move(result));Drained.notify_all();
        }
        Drained.notify_all();
    }
public:
    ~Broker(){Stop();}
    void* Bind(const std::string& plugin,const std::string& owner){
        if(!Token(plugin)||!Token(owner)||!plugin.starts_with(owner+"."))throw std::runtime_error("Native plugin owner namespace mismatch");
        std::lock_guard lock(Gate);if(Owners.contains(plugin))throw std::runtime_error("Native plugin owner is already bound");
        auto value=std::make_unique<Owner>(Owner{plugin,owner});auto* context=value.get();Owners.emplace(plugin,std::move(value));return context;
    }
    uint64_t RegisterProvider(void* context,const RuneSchemaModProviderV1& descriptor){
        if(descriptor.StructSize<sizeof(descriptor)||descriptor.ApiVersion!=RS_MOD_API_VERSION||!descriptor.Evaluate||!descriptor.ReleaseResource
            ||!descriptor.InputCount||descriptor.InputCount>RS_MOD_MAX_INPUTS||!descriptor.MaxResultCount||descriptor.MaxResultCount>RS_MOD_MAX_RESULTS)
            throw std::runtime_error("Invalid native provider descriptor");
        const auto id=Text(descriptor.Id),type=Text(descriptor.ResourceType);if(!Token(id)||!Token(type))throw std::runtime_error("Invalid native provider ID/type");
        std::unique_lock lock(Gate);auto* owner=Bound(context);
        for(const auto& [handle,p]:Providers)if(p->Bound==owner&&p->Id==id)throw std::runtime_error("Duplicate native provider");
        auto provider=std::make_unique<Provider>();provider->Handle=Next();provider->Bound=owner;provider->Id=id;provider->Type=type;provider->Api=descriptor;
        auto* pointer=provider.get();const auto handle=provider->Handle;Providers.emplace(handle,std::move(provider));
        try{pointer->Worker=std::thread([this,pointer]{Work(pointer);});}catch(...){Providers.erase(handle);throw;}return handle;
    }
    uint64_t RegisterResource(void* context,uint64_t providerHandle,const RuneSchemaModResourceV1& descriptor){
        if(descriptor.StructSize<sizeof(descriptor)||descriptor.ApiVersion!=RS_MOD_API_VERSION)throw std::runtime_error("Invalid immutable resource ABI");
        const auto id=Text(descriptor.Id);
        if(!Token(id)||!descriptor.DataVersion||!descriptor.Generation||descriptor.Generation>INT64_MAX||!descriptor.Value)
            throw std::runtime_error("Invalid immutable resource descriptor");
        std::lock_guard lock(Gate);auto* owner=Bound(context);const auto provider=Providers.find(providerHandle);
        if(provider==Providers.end()||provider->second->Bound!=owner||provider->second->Stopping)throw std::runtime_error("Native resource provider is unavailable");
        for(const auto& [handle,data]:Resources)if(data->Provider==providerHandle&&data->Id==id)throw std::runtime_error("Duplicate native resource");
        auto data=std::make_shared<Resource>(Resource{Next(),providerHandle,descriptor.Generation,id,descriptor.Value,false});const auto handle=data->Handle;Resources.emplace(handle,std::move(data));return handle;
    }
    void Release(void* context,uint64_t handle){
        std::unique_lock lock(Gate);auto* owner=Bound(context);const auto found=Resources.find(handle);
        if(found==Resources.end())throw std::runtime_error("Native resource was not found");
        auto data=found->second;auto& provider=*Providers.at(data->Provider);if(provider.Bound!=owner)throw std::runtime_error("Native resource belongs to another owner");data->Retired=true;
        for(auto& [key,session]:Sessions)if(session.LatestData==data){session.Result.Ready=true;session.Result.Success=false;session.Result.Values.clear();session.Result.Error="Native resource was retired";Completed.insert_or_assign(key,session.Result);CompletionReady.store(true,std::memory_order_release);}
        for(auto& [key,session]:Sessions)if(session.Pending&&session.Pending->Data==data){auto pending=*session.Pending;session.Pending.reset();Finish(pending,{pending.Component,pending.Ticket,pending.Generation,data->Generation,true,false,{},"Native resource was retired"});}
        std::deque<Request> discarded;for(const auto& request:provider.Queue)if(request.Data==data)discarded.push_back(request);
        std::erase_if(provider.Queue,[&](const auto& request){return request.Data==data;});
        for(const auto& request:discarded)Finish(request,{request.Component,request.Ticket,request.Generation,data->Generation,true,false,{},"Native resource was retired"});
        Drained.wait(lock,[&]{return provider.ActiveResource!=handle;});Resources.erase(handle);lock.unlock();provider.Api.ReleaseResource(provider.Api.Context,data->Value);
    }
    uint64_t Submit(const std::string& owner,uint64_t component,const std::string& name,const std::vector<double>& input,uint64_t generation){
        if(!component||!generation||generation>INT64_MAX||!std::all_of(input.begin(),input.end(),[](double v){return std::isfinite(v);}))throw std::runtime_error("Invalid component, generation or input vector");
        std::unique_lock lock(Gate);std::shared_ptr<Resource> data;Provider* provider=nullptr;
        for(const auto& [handle,candidate]:Resources){auto* p=Providers.at(candidate->Provider).get();if(p->Bound->Mod==owner&&p->Bound->Plugin+"/"+p->Id+"/"+candidate->Id==name&&!candidate->Retired&&!p->Stopping){data=candidate;provider=p;break;}}
        if(!data||input.size()!=provider->Api.InputCount)throw std::runtime_error("Native resource or exact input count is unavailable");
        const auto existing=Sessions.find(component);if(existing==Sessions.end()&&Sessions.size()>=256)throw std::runtime_error("Too many active native service components");
        auto& session=Sessions[component];if(!session.Owner.empty()&&session.Owner!=owner)throw std::runtime_error("Component owner changed");
        if(generation<=session.Generation)throw std::runtime_error("Compute generation must increase");
        const auto ticket=Next();Request request{component,ticket,generation,data,input};session.Owner=owner;session.Generation=generation;session.Latest=ticket;session.LatestData=data;
        session.Result={component,ticket,generation,data->Generation,false,false};Completed.erase(component);
        if(session.Active)session.Pending=std::move(request);else Schedule(std::move(request));return ticket;
    }
    Completion Poll(uint64_t component,uint64_t ticket){std::lock_guard lock(Gate);const auto found=Sessions.find(component);if(found==Sessions.end()||found->second.Latest!=ticket)throw std::runtime_error("Compute ticket is stale or belongs to another component");return found->second.Result;}
    std::vector<Completion> TakeCompleted(){
        if(!CompletionReady.load(std::memory_order_acquire))return {};
        std::lock_guard lock(Gate);std::vector<Completion> result;result.reserve(Completed.size());
        for(auto& [id,value]:Completed)result.push_back(std::move(value));Completed.clear();CompletionReady.store(false,std::memory_order_release);return result;
    }
    void Cancel(uint64_t component){std::lock_guard lock(Gate);Sessions.erase(component);Completed.erase(component);for(auto& [id,p]:Providers)std::erase_if(p->Queue,[&](const auto& request){return request.Component==component;});}
    void Pause(const std::string& plugin){
        std::vector<Provider*> selected;
        {std::lock_guard lock(Gate);const auto owner=Owners.find(plugin);if(owner==Owners.end())return;
            for(auto& [id,p]:Providers)if(p->Bound==owner->second.get()){p->Stopping=true;selected.push_back(p.get());p->Wake.notify_all();}
            for(auto& [id,data]:Resources)if(std::find(selected.begin(),selected.end(),Providers.at(data->Provider).get())!=selected.end())data->Retired=true;
            for(auto& [key,session]:Sessions)if(session.LatestData&&session.LatestData->Retired){session.Result.Ready=true;session.Result.Success=false;session.Result.Values.clear();session.Result.Error="Native resource was retired";Completed.insert_or_assign(key,session.Result);CompletionReady.store(true,std::memory_order_release);}
        }
        for(auto* provider:selected)if(provider->Worker.joinable())provider->Worker.join();
    }
    void Drop(const std::string& plugin){
        Pause(plugin);
        std::vector<Provider*> selected;std::vector<std::pair<Provider*,const void*>> release;
        {std::lock_guard lock(Gate);const auto owner=Owners.find(plugin);if(owner==Owners.end())return;
            for(auto& [id,p]:Providers)if(p->Bound==owner->second.get())selected.push_back(p.get());
        }
        {std::lock_guard lock(Gate);
            for(auto* provider:selected){for(const auto& request:provider->Queue)Finish(request,{request.Component,request.Ticket,request.Generation,request.Data->Generation,true,false,{},"Native provider stopped"});provider->Queue.clear();}
            for(auto it=Resources.begin();it!=Resources.end();)if(it->second->Retired&&std::find(selected.begin(),selected.end(),Providers.at(it->second->Provider).get())!=selected.end()){release.emplace_back(Providers.at(it->second->Provider).get(),it->second->Value);it=Resources.erase(it);}else ++it;
        }
        for(const auto& [provider,value]:release)try{provider->Api.ReleaseResource(provider->Api.Context,value);}catch(...){}
        {std::lock_guard lock(Gate);for(auto* provider:selected)Providers.erase(provider->Handle);Owners.erase(plugin);}
    }
    void Stop(){for(;;){std::string plugin;{std::lock_guard lock(Gate);if(Owners.empty())break;plugin=Owners.begin()->first;}Drop(plugin);}std::lock_guard lock(Gate);Sessions.clear();Completed.clear();CompletionReady.store(false,std::memory_order_release);}
};
}
