#ifdef NDEBUG
#undef NDEBUG
#endif
#include "Core/ModCompute.h"
#include <cassert>
#include <chrono>
#include <future>
#include <latch>
using namespace PS::ModCompute;
struct Context {
    std::mutex Gate;std::condition_variable Changed;bool Entered=false,Continue=false;std::vector<uint64_t> Calls;int Active=0,Maximum=0,Released=0,Mode=0;
    std::thread::id Owner=std::this_thread::get_id();
};
static int32_t RS_MOD_CALL Evaluate(void* opaque,const void* resource,const double* input,uint32_t count,uint64_t generation,double* result,uint32_t,uint32_t* size,char* error,uint32_t){
    auto& context=*static_cast<Context*>(opaque);assert(std::this_thread::get_id()!=context.Owner&&count==2&&resource);
    {std::unique_lock lock(context.Gate);context.Maximum=std::max(context.Maximum,++context.Active);context.Calls.push_back(generation);if(generation==1){context.Entered=true;context.Changed.notify_all();context.Changed.wait(lock,[&]{return context.Continue;});}}
    result[0]=input[0]+input[1]+*static_cast<const double*>(resource);*size=1;
    if(context.Mode==1)result[0]=std::numeric_limits<double>::quiet_NaN();
    if(context.Mode==2){error[0]=char(0xff);error[1]=0;}
    {std::lock_guard lock(context.Gate);--context.Active;}
    return context.Mode==2?RS_MOD_FAILED:RS_MOD_OK;
}
static void RS_MOD_CALL Release(void* opaque,const void*){auto& context=*static_cast<Context*>(opaque);assert(std::this_thread::get_id()==context.Owner);std::lock_guard lock(context.Gate);assert(context.Active==0);++context.Released;}
template<class F> void Reject(F operation){bool rejected=false;try{operation();}catch(const std::exception&){rejected=true;}assert(rejected);}
Completion Wait(Broker& broker,uint64_t component,uint64_t ticket){const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);for(;;){auto result=broker.Poll(component,ticket);if(result.Ready)return result;assert(std::chrono::steady_clock::now()<until);std::this_thread::yield();}}
int main(){
    Context context;double data=10;Broker broker;assert(broker.TakeCompleted().empty());auto* owner=broker.Bind("Example.Math","Example"),*other=broker.Bind("Other.Math","Other");
    RuneSchemaModProviderV1 descriptor{sizeof(descriptor),1,"Probe","TypedData",2,1,Evaluate,Release,&context};
    const auto provider=broker.RegisterProvider(owner,descriptor);Reject([&]{broker.RegisterProvider(owner,descriptor);});
    RuneSchemaModResourceV1 resource{sizeof(resource),1,"Default",1,7,&data};
    Reject([&]{broker.RegisterResource(other,provider,resource);});const auto handle=broker.RegisterResource(owner,provider,resource);
    Reject([&]{broker.Submit("Other",10,"Example.Math/Probe/Default",{1,2},1);});
    Reject([&]{broker.Submit("Example",10,"Example.Math/Probe/Default",{1},1);});
    const auto first=broker.Submit("Example",10,"Example.Math/Probe/Default",{1,2},1);
    {std::unique_lock lock(context.Gate);assert(context.Changed.wait_for(lock,std::chrono::seconds(2),[&]{return context.Entered;}));}
    const auto skipped=broker.Submit("Example",10,"Example.Math/Probe/Default",{2,3},2);const auto latest=broker.Submit("Example",10,"Example.Math/Probe/Default",{3,4},3);
    Reject([&]{broker.Poll(10,first);});Reject([&]{broker.Poll(10,skipped);});Reject([&]{broker.Poll(11,latest);});
    {std::lock_guard lock(context.Gate);context.Continue=true;context.Changed.notify_all();}
    const auto completed=Wait(broker,10,latest);assert(completed.Success&&completed.Values==std::vector<double>{17}&&completed.ResourceGeneration==7);
    assert(context.Maximum==1&&context.Calls==std::vector<uint64_t>({1,3}));Reject([&]{broker.Submit("Example",10,"Example.Math/Probe/Default",{1,2},3);});
    const auto notices=broker.TakeCompleted();assert(notices.size()==1&&notices[0].Ticket==latest);assert(broker.TakeCompleted().empty());
    Reject([&]{broker.Release(other,handle);});broker.Release(owner,handle);assert(context.Released==1&&!broker.Poll(10,latest).Success);const auto retired=broker.TakeCompleted();assert(retired.size()==1&&!retired[0].Success);assert(broker.TakeCompleted().empty());
    const auto renewed=broker.RegisterResource(owner,provider,resource);context.Mode=1;
    const auto bad=broker.Submit("Example",10,"Example.Math/Probe/Default",{1,2},4);assert(!Wait(broker,10,bad).Success);
    context.Mode=2;const auto text=broker.Submit("Example",10,"Example.Math/Probe/Default",{1,2},5);assert(Wait(broker,10,text).Error=="Native provider returned invalid UTF-8 error text");
    context.Mode=0;{std::lock_guard lock(context.Gate);context.Entered=false;context.Continue=false;}
    const auto active=broker.Submit("Example",20,"Example.Math/Probe/Default",{1,2},1);
    {std::unique_lock lock(context.Gate);assert(context.Changed.wait_for(lock,std::chrono::seconds(2),[&]{return context.Entered;}));}
    auto releaseWorker=std::async(std::launch::async,[&]{const auto result=Wait(broker,20,active);assert(!result.Success&&result.Error=="Native resource was retired");std::lock_guard lock(context.Gate);context.Continue=true;context.Changed.notify_all();});
    broker.Release(owner,renewed);releaseWorker.get();assert(context.Released==2);
    broker.Cancel(10);Reject([&]{broker.Poll(10,text);});broker.Drop("Example.Math");assert(context.Released==2);Reject([&]{broker.Release(owner,renewed);});
    broker.Stop();assert(context.Released==2&&broker.TakeCompleted().empty());
}
