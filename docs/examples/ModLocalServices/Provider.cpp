// SPDX-License-Identifier: MIT
#include "Runtime/RuneSchemaPluginApi.h"
#include "RuneSchemaModApi.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <new>

#if defined(_WIN32)
#define EXAMPLE_EXPORT extern "C" __declspec(dllexport)
#else
#define EXAMPLE_EXPORT extern "C" __attribute__((visibility("default")))
#endif
namespace {
constexpr char PluginId[]="Example.ShapeMath";
struct Scale { const std::array<double,4> Factors{1,2,3,4}; };
int32_t RS_MOD_CALL Evaluate(void*,const void* resource,const double* input,uint32_t count,uint64_t,
    double* result,uint32_t capacity,uint32_t* size,char* error,uint32_t errorCapacity){
    if(size)*size=0;
    if(!resource||!input||count!=4||!result||capacity<4||!size)return RS_MOD_INVALID_ARGUMENT;
    const auto& scale=*static_cast<const Scale*>(resource);std::array<double,4> values{};
    for(uint32_t i=0;i<4;++i){
        values[i]=input[i]*scale.Factors[i];
        if(!std::isfinite(values[i])){
            if(error&&errorCapacity)std::snprintf(error,errorCapacity,"Scale result is not finite");
            return RS_MOD_FAILED;
        }
    }
    for(uint32_t i=0;i<4;++i)result[i]=values[i];*size=4;return RS_MOD_OK;
}
void RS_MOD_CALL Release(void*,const void* resource){delete static_cast<const Scale*>(resource);}
}
EXAMPLE_EXPORT const RuneSchemaPluginDescriptor* RS_PLUGIN_CALL RuneSchemaPlugin_Query(){
    static const RuneSchemaPluginDescriptor value{sizeof(value),1,PluginId,"Example immutable scale provider","1.0.0"};return &value;
}
EXAMPLE_EXPORT int32_t RS_PLUGIN_CALL RuneSchemaPlugin_Initialize(const RuneSchemaHostApi* host,void** instance){
    if(!instance)return RS_PLUGIN_INVALID_ARGUMENT;*instance=nullptr;
    if(!host||host->StructSize<offsetof(RuneSchemaHostApi,GetModApi)+sizeof(host->GetModApi)
        ||host->ApiVersion!=RUNESCHEMA_PLUGIN_API_VERSION||!host->GetModApi)return RS_PLUGIN_INCOMPATIBLE_API;
    const auto* api=host->GetModApi(PluginId);
    if(!api||api->StructSize<sizeof(RuneSchemaModApiV1)||api->ApiVersion!=RS_MOD_API_VERSION
        ||!api->OwnerContext||!api->RegisterProvider||!api->RegisterResource||!api->ReleaseResource)return RS_PLUGIN_INCOMPATIBLE_API;
    RuneSchemaModProviderV1 provider{sizeof(provider),1,"Scale4","Example.Scale.v1",4,4,Evaluate,Release,nullptr};
    uint64_t providerHandle=0;
    if(api->RegisterProvider(api->OwnerContext,&provider,&providerHandle)!=RS_MOD_OK)return RS_PLUGIN_FAILED;
    auto* scale=new(std::nothrow) Scale;if(!scale)return RS_PLUGIN_FAILED;
    RuneSchemaModResourceV1 resource{sizeof(resource),1,"Default",1,1,scale};uint64_t resourceHandle=0;
    if(api->RegisterResource(api->OwnerContext,providerHandle,&resource,&resourceHandle)!=RS_MOD_OK){delete scale;return RS_PLUGIN_FAILED;}
    // Core owns the resource and drains/releases it before Shutdown, including initialization failure.
    return RS_PLUGIN_OK;
}
EXAMPLE_EXPORT void RS_PLUGIN_CALL RuneSchemaPlugin_Shutdown(void*){}
