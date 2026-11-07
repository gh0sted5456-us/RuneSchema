#include "Core/ModCompute.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include <windows.h>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
using PS::ModCompute::Broker;
namespace {
Broker* Current=nullptr;RuneSchemaModApiV1 Api{};bool RejectResource=false;unsigned ApiCalls=0;
void Check(bool value){if(!value)throw std::runtime_error("Mod-local provider example check failed");}
int32_t RS_MOD_CALL Provider(void* owner,const RuneSchemaModProviderV1* descriptor,uint64_t* handle){
    try{*handle=Current->RegisterProvider(owner,*descriptor);return RS_MOD_OK;}catch(...){return RS_MOD_FAILED;}
}
int32_t RS_MOD_CALL Resource(void* owner,uint64_t provider,const RuneSchemaModResourceV1* descriptor,uint64_t* handle){
    if(RejectResource)return RS_MOD_FAILED;
    try{*handle=Current->RegisterResource(owner,provider,*descriptor);return RS_MOD_OK;}catch(...){return RS_MOD_FAILED;}
}
int32_t RS_MOD_CALL Release(void* owner,uint64_t resource){try{Current->Release(owner,resource);return RS_MOD_OK;}catch(...){return RS_MOD_FAILED;}}
const RuneSchemaModApiV1* RS_PLUGIN_CALL GetApi(const char* plugin){++ApiCalls;return std::strcmp(plugin,"Example.ShapeMath")==0?&Api:nullptr;}
PS::ModCompute::Completion Wait(Broker& broker,uint64_t ticket){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    for(;;){auto result=broker.Poll(42,ticket);if(result.Ready)return result;Check(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
}
}
int main(int argc,char** argv)try{
    Check(argc==2);const auto path=std::filesystem::absolute(argv[1]);
    const auto module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);Check(module!=nullptr);
    struct Unload{HMODULE Module;~Unload(){FreeLibrary(Module);}} unload{module};
    Broker broker;Current=&broker;
    const auto query=reinterpret_cast<RuneSchemaPluginQueryFn>(GetProcAddress(module,"RuneSchemaPlugin_Query"));
    const auto initialize=reinterpret_cast<RuneSchemaPluginInitializeFn>(GetProcAddress(module,"RuneSchemaPlugin_Initialize"));
    const auto shutdown=reinterpret_cast<RuneSchemaPluginShutdownFn>(GetProcAddress(module,"RuneSchemaPlugin_Shutdown"));Check(query&&initialize&&shutdown);
    const auto* descriptor=query();Check(descriptor&&descriptor->StructSize==sizeof(*descriptor)&&descriptor->ApiVersion==1&&std::strcmp(descriptor->Id,"Example.ShapeMath")==0);
    RuneSchemaHostApi host{};host.StructSize=offsetof(RuneSchemaHostApi,GetModApi);host.ApiVersion=1;host.GetModApi=GetApi;void* instance=reinterpret_cast<void*>(1);
    Check(initialize(&host,&instance)==RS_PLUGIN_INCOMPATIBLE_API&&instance==nullptr&&ApiCalls==0);
    host.StructSize=sizeof(host);host.GetModApi=nullptr;Check(initialize(&host,&instance)==RS_PLUGIN_INCOMPATIBLE_API);
    host.GetModApi=GetApi;Api={sizeof(Api),1,nullptr,Provider,Resource,Release};Check(initialize(&host,&instance)==RS_PLUGIN_INCOMPATIBLE_API);
    Api.OwnerContext=broker.Bind("Example.ShapeMath","Example");Check(initialize(&host,&instance)==RS_PLUGIN_OK);
    const auto ticket=broker.Submit("Example",42,"Example.ShapeMath/Scale4/Default",{2,3,4,5},1);
    const auto result=Wait(broker,ticket);Check(result.Success&&result.Generation==1&&result.ResourceGeneration==1&&result.Values==std::vector<double>({2,6,12,20}));
    const auto overflow=broker.Submit("Example",42,"Example.ShapeMath/Scale4/Default",{0,0,0,std::numeric_limits<double>::max()},2);
    const auto invalid=Wait(broker,overflow);Check(!invalid.Success&&invalid.Values.empty()&&invalid.Error=="Scale result is not finite");
    broker.Drop("Example.ShapeMath");shutdown(instance);
    Api.OwnerContext=broker.Bind("Example.ShapeMath","Example");RejectResource=true;Check(initialize(&host,&instance)==RS_PLUGIN_FAILED);broker.Drop("Example.ShapeMath");
    broker.Stop();Current=nullptr;return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
