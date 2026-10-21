#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include "Runtime/AdditionalLogging.h"
#include "Core/AdditionalLoggingInfo.h"
#include "Core/AdditionalLoggingConfiguration.h"
#include "Core/SystemDiagnostics.h"
#include "Runtime/HostServices.h"
#include "Runtime/Storefront.h"
#include "Runtime/NetworkContext.h"
#include "SDK/WeakObjectHandle.h"
#include "Utility/DiagnosticLog.h"
#include <Unreal/Hooks/Hooks.hpp>
#include <Helpers/String.hpp>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <memory>
#pragma comment(lib,"iphlpapi.lib")
namespace PS::AdditionalLogging {
using namespace RC::Unreal;using Json=nlohmann::json;
namespace I=AdditionalLoggingInfo;namespace AH=DragonWilds::ActorHelper;namespace C=DragonWilds::ReflectedContract;
namespace {
std::atomic_uint Flags=0,Revision=0;unsigned observedRevision=~0u;
std::atomic_bool SamplingReady=false;
Hook::GlobalCallbackId tickHook=Hook::ERROR_ID;
I::RateWindow rateWindow;std::chrono::steady_clock::time_point next{};
WeakObjectHandle sampleWorld;std::vector<WeakObjectHandle> playerStates;size_t cursor=0;I::LatencyMean latency;bool authoritative=false;
unsigned Bits(const Json& d){return AdditionalLoggingConfiguration::Bits(d);}
void Row(const std::string& text){DiagnosticLog::Write(RC::to_wstring("[RuneSchema] [group=additional] "+text),true);}
std::string Path(const std::filesystem::path& path){if(path.empty())return "unavailable";const auto value=path.u8string();std::string out(value.begin(),value.end());for(auto& c:out)if(static_cast<unsigned char>(c)<32)c='?';return out.substr(0,32768);}
void Paths(unsigned flags){
    if(flags&I::FilePaths){wchar_t executable[32768]{};const auto length=GetModuleFileNameW(nullptr,executable,_countof(executable));
        Row("Paths (opt-in): executable="+Path(length&&length<_countof(executable)?std::filesystem::path(executable):std::filesystem::path{})+"; RuneSchema="+Path(HostServices::ModDirectory())+"; cache="+Path(HostServices::CacheDirectory())+"; log="+Path(HostServices::ModDirectory()/"RuneSchema.log"));}
    if(flags&I::Region){wchar_t iso[16]{};const auto region=GetUserGeoID(GEOCLASS_NATION);
        const auto count=region==GEOID_NOT_AVAILABLE?0:GetGeoInfoW(region,GEO_ISO2,iso,_countof(iso),0);
        Row("Windows configured region (not physical geolocation): "+(count>1?SystemDiagnostics::UTF8(iso):std::string("unavailable"))+"; no IP/GPS lookup performed");}
    if(flags&I::SavePaths){
        if(Storefront::Current()==Storefront::Kind::GamePass)Row("Native save provider directory (opt-in): "+Path(HostServices::XboxSaveRoot())+"; active container/payload filename is provider-owned and not exposed");
        else{wchar_t local[32768]{};const auto length=GetEnvironmentVariableW(L"LOCALAPPDATA",local,_countof(local));
            if(length&&length<_countof(local)){const auto saved=std::filesystem::path(local)/"RSDragonwilds/Saved";
                Row("Known native save directories (opt-in): character="+Path(saved/"SaveCharacters")+"; world="+Path(saved/"SaveGames")+"; active payload filename not exposed");}
            else Row("Native save provider directory: unavailable");}
    }
}
void NetworkRates(){
    MIB_IF_TABLE2* table=nullptr;
    if(GetIfTable2(&table)!=NO_ERROR||!table){rateWindow.Reset();Row("Adapter traffic: unavailable");return;}
    const std::unique_ptr<MIB_IF_TABLE2,decltype(&FreeMibTable)> owned(table,&FreeMibTable);
    std::vector<I::Counter> counters;
    if(table->NumEntries<=4096)for(ULONG i=0;i<table->NumEntries;++i){const auto& row=table->Table[i];
        if(row.OperStatus==IfOperStatusUp&&row.InterfaceAndOperStatusFlags.HardwareInterface&&!row.InterfaceAndOperStatusFlags.FilterInterface
            &&(row.Type==IF_TYPE_ETHERNET_CSMACD||row.Type==IF_TYPE_IEEE80211))counters.push_back({row.InterfaceLuid.Value,row.InOctets,row.OutOctets});}
    const bool bounded=table->NumEntries<=4096;
    if(!bounded){rateWindow.Reset();Row("Adapter traffic: interface bound exceeded");return;}
    const auto value=rateWindow.Sample(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(),counters);
    if(!value){Row("Adapter traffic: waiting for a second comparable sample; not an internet speed test");return;}
    std::ostringstream out;out<<std::fixed<<std::setprecision(3)<<"Adapter traffic: upload="<<value->UploadMbps<<" Mbps; download="<<value->DownloadMbps<<" Mbps; interfaces="<<value->Interfaces<<"; observed device-wide traffic, not connection capacity or game-only traffic";Row(out.str());
}
void BeginLatency(){
    playerStates.clear();cursor=0;latency={};sampleWorld.Reset();
    const auto context=Network::Detect();
    if(!context.World||context.Mode==Network::Role::Unknown||context.Mode==Network::Role::Standalone){Row("World average network latency: unavailable (no verified multiplayer world)");return;}
    auto* gameState=AH::GetObjectRef(context.World,TEXT("GameState"));
    auto* property=gameState?CastField<FArrayProperty>(gameState->GetClassPrivate()->GetPropertyByNameInChain(TEXT("PlayerArray"))):nullptr;
    auto* inner=property?CastField<FObjectPropertyBase>(property->GetInner()):nullptr;
    auto* playerClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.PlayerState"));
    if(!gameState||!C::Fits(property,gameState->GetClassPrivate()->GetPropertiesSize())||!inner||inner->GetElementSize()!=sizeof(UObject*)||!playerClass)throw std::runtime_error("native PlayerArray contract unavailable");
    auto* values=property->ContainerPtrToValuePtr<FScriptArray>(gameState);
    if(!values||values->Num()<0||values->Num()>256||(values->Num()&&!values->GetData()))throw std::runtime_error("native PlayerArray bound unavailable");
    for(int i=0;i<values->Num();++i){auto* state=inner->GetObjectPropertyValue(static_cast<uint8_t*>(values->GetData())+size_t(i)*sizeof(UObject*));
        if(state&&state->IsA(playerClass)&&state->GetWorld()==context.World){WeakObjectHandle handle;handle.Assign(state);playerStates.push_back(handle);}}
    sampleWorld.Assign(context.World);authoritative=Network::OwnsGameplay(context.Mode);
    if(playerStates.empty())Row("World average network latency: unavailable (no player-state samples)");
}
void PumpLatency(){
    if(playerStates.empty())return;
    auto* world=sampleWorld.Get();if(!world||world->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))){playerStates.clear();return;}
    const auto start=std::chrono::steady_clock::now();size_t processed=0;
    while(cursor<playerStates.size()&&processed++<16&&std::chrono::steady_clock::now()-start<std::chrono::milliseconds(2)){
        auto* state=playerStates[cursor++].Get();if(!state||state->GetWorld()!=world)continue;
        auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Engine.PlayerState:GetPingInMilliseconds"));
        auto* output=function?CastField<FFloatProperty>(function->GetReturnProperty()):nullptr;
        if(!function||!function->HasAnyFunctionFlags(FUNC_Native)||function->HasAnyFunctionFlags(FUNC_Net)||C::Count(function)!=1||!C::Output(output,function)||output->GetSize()!=sizeof(float))throw std::runtime_error("native ping getter unavailable");
        AH::FunctionCall call(state,function);call.Invoke();latency.Add(call.Result<float>());
    }
    if(cursor==playerStates.size()){
        const auto average=latency.Value();std::ostringstream out;out<<"World average network latency: ";
        if(average)out<<std::fixed<<std::setprecision(2)<<*average<<" ms";else out<<"unavailable";
        out<<"; measured players="<<latency.Samples<<"/"<<playerStates.size()<<"; "<<(authoritative?"server-observed native player ping":"client-observed replicated player ping; values may be stale")<<"; zero/invalid samples excluded";Row(out.str());playerStates.clear();
    }
}
void Tick(){
    const auto flags=Flags.load();
    if(!(flags&I::Enabled)){rateWindow.Reset();playerStates.clear();next={};observedRevision=~0u;return;}
    const auto revision=Revision.load();const auto now=std::chrono::steady_clock::now();
    if(revision!=observedRevision){rateWindow.Reset();playerStates.clear();next={};observedRevision=revision;Paths(flags);}
    if(now>=next){next=now+std::chrono::seconds(30);
        if(flags&I::NetworkRates)NetworkRates();
        if(flags&I::AverageLatency)try{BeginLatency();}catch(...){playerStates.clear();Row("World average network latency: unavailable (native world/player contract not verified)");}
    }
    if(flags&I::AverageLatency)try{PumpLatency();}catch(...){playerStates.clear();Row("World average network latency: unavailable (native ping getter not verified)");}
}
}
Json Read(){auto result=AdditionalLoggingConfiguration::Read(HostServices::SettingsDirectory());if(result.value("Ok",false))result["Active"]=SamplingReady.load()&&bool(Flags.load()&I::Enabled);return result;}
Json Save(const Json& request){
    const auto flags=Bits(request.at("Document"));auto result=AdditionalLoggingConfiguration::Save(HostServices::SettingsDirectory(),request);
    if(result.value("Ok",false)){Flags=flags;++Revision;result["Active"]=SamplingReady.load()&&bool(flags&I::Enabled);result["Status"]=SamplingReady.load()?"Additional logging updated for the next sampler tick. Existing log entries are not erased; review paths before sharing.":"Options saved, but native sampling is unavailable in this session. No runtime measurements are claimed.";}return result;
}
void Initialize(){
    if(tickHook!=Hook::ERROR_ID)return;const auto snapshot=Read();if(snapshot.value("Ok",false))Flags=Bits(snapshot.at("Document"));
    Hook::FCallbackOptions options{};options.OwnerModName=TEXT("RuneSchema");options.HookName=TEXT("AdditionalLoggingOptIn");
    tickHook=Hook::RegisterEngineTickPostCallback([](auto&,auto*,float,bool){try{Tick();}catch(...){next=std::chrono::steady_clock::now()+std::chrono::seconds(30);}},options);
    SamplingReady=tickHook!=Hook::ERROR_ID;
}
void Shutdown()noexcept{Flags=0;SamplingReady=false;if(tickHook!=Hook::ERROR_ID)Hook::UnregisterCallback(tickHook);tickHook=Hook::ERROR_ID;playerStates.clear();rateWindow.Reset();}
}
