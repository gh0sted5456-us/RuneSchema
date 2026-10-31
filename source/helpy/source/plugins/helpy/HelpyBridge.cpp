#include <windows.h>
#include <shellapi.h>
#include <cstring>
#include <memory>
#include <atomic>
#include "Runtime/RuneSchemaPluginApi.h"
#include "Runtime/HelpySettings.h"
#include "Runtime/HelpyPaths.h"
#include "Runtime/HostServices.h"
#include "Core/HelpyEnvironment.h"
#include "Core/HelpyBranding.h"
#include "Core/HelpyInventoryService.h"
#include "HelpyDesktopLaunch.h"
#include "Runtime/HelpyNativeOverlayBridge.h"
namespace {
HMODULE Module{};
constexpr RuneSchemaPluginDescriptor Descriptor{sizeof(RuneSchemaPluginDescriptor),RUNESCHEMA_PLUGIN_API_VERSION,"RuneSchema.Helpy","RSMK (RuneSchema Modding Kit)",PS::HelpyBranding::Version};
struct State{const RuneSchemaHostApi* Host{};std::atomic<bool> Ready=false,InventoryReady=false;std::filesystem::file_time_type MarksStamp{};std::mutex InventoryMutex;std::jthread Launcher;std::string NativeReport;};
bool ReadCore(State* state,const nlohmann::json& request,nlohmann::json& result){
    const auto input=request.dump();std::vector<char> output(8192);uint32_t size=0;
    auto code=state->Host->CallService(Descriptor.Id,"runeschema.tools",input.c_str(),output.data(),static_cast<uint32_t>(output.size()),&size);
    if(code==RS_PLUGIN_BUFFER_TOO_SMALL&&size>output.size()&&size<=RUNESCHEMA_PLUGIN_MAX_MESSAGE){output.resize(size);code=state->Host->CallService(Descriptor.Id,"runeschema.tools",input.c_str(),output.data(),static_cast<uint32_t>(output.size()),&size);}
    if(code!=RS_PLUGIN_OK||!size||size>output.size())return false;if(output[size-1]=='\0')--size;result=nlohmann::json::parse(output.data(),output.data()+size);return true;
}
int32_t Reply(const nlohmann::json& value,char* output,uint32_t capacity,uint32_t* size){if(!size)return RS_PLUGIN_INVALID_ARGUMENT;const auto text=value.dump();if(text.size()+1>RUNESCHEMA_PLUGIN_MAX_MESSAGE)return RS_PLUGIN_FAILED;*size=static_cast<uint32_t>(text.size()+1);if(!output||capacity<*size)return RS_PLUGIN_BUFFER_TOO_SMALL;std::memcpy(output,text.c_str(),*size);return RS_PLUGIN_OK;}
int32_t RS_PLUGIN_CALL About(void*,const char*,char* out,uint32_t capacity,uint32_t* size){return Reply({{"plugin",Descriptor.Id},{"version",Descriptor.Version},{"ui","standalone-exe"},{"umg",false}},out,capacity,size);}
int32_t RS_PLUGIN_CALL Ignore(void*,const char*,char*,uint32_t,uint32_t* size){if(!size)return RS_PLUGIN_INVALID_ARGUMENT;*size=0;return RS_PLUGIN_OK;}
int32_t RS_PLUGIN_CALL Status(void* context,const char*,char* out,uint32_t capacity,uint32_t* size){const auto* state=static_cast<State*>(context);return Reply({{"Ready",state&&state->Ready.load()},{"BridgeOnly",true}},out,capacity,size);}
int32_t RS_PLUGIN_CALL NativeOverlay(void* context,const char* request,char* out,uint32_t capacity,uint32_t* size){
    if(!request||!size)return RS_PLUGIN_INVALID_ARGUMENT;if(!out||capacity<8192){*size=8192;return RS_PLUGIN_BUFFER_TOO_SMALL;}
    try{
        auto payload=nlohmann::json::parse(request);nlohmann::json result;
        try{result=PS::HelpyNativeOverlayBridge::Call(payload);}
        catch(const std::exception& error){result=PS::HelpyNativeOverlayBridge::Call({{"Action","Status"}});result["Failed"]=true;result["Error"]=error.what();}
        if(auto* state=static_cast<State*>(context);state&&(result.value("Ready",false)||result.value("Failed",false))){
            const bool failed=result.value("Failed",false);const auto key=failed?result.value("Error",std::string{}):result.value("Api",std::string{})+(result.value("Presented",false)&&result.value("Visible",false)?" visible":result.value("Attached",false)?" attached":" ready");
            std::lock_guard lock(state->InventoryMutex);if(key!=state->NativeReport){state->NativeReport=key;const auto message=std::string(failed?"[RS-OVL-002] ":result.value("Presented",false)&&result.value("Visible",false)?"[RS-OVL-001] ":"[RS-OVL-003] ")+result.dump();state->Host->Log(failed?RS_LOG_WARNING:RS_LOG_INFO,Descriptor.Id,message.c_str());}
        }
        return Reply(result,out,capacity,size);
    }catch(const std::exception& error){return Reply({{"Ready",false},{"Failed",true},{"Error",error.what()}},out,capacity,size);}
}
int32_t RS_PLUGIN_CALL InventoryService(void* context,const char* requestJson,char* response,uint32_t capacity,uint32_t* size){
    auto* state=static_cast<State*>(context);if(!state||!requestJson||!size)return RS_PLUGIN_INVALID_ARGUMENT;
    try{
        std::lock_guard lock(state->InventoryMutex);const auto request=nlohmann::json::parse(requestJson);const auto action=request.value("Action",std::string{});
        // Mutations return bounded acknowledgements, not the complete storage document.
        if(PS::HelpyInventoryService::Mutating(action)&&(!response||capacity<PS::HelpyInventoryService::MutationReplyBytes)){*size=PS::HelpyInventoryService::MutationReplyBytes;return RS_PLUGIN_BUFFER_TOO_SMALL;}
        nlohmann::json value;
        if(action!="Capabilities"&&!state->InventoryReady.load())value={{"Error","Helpy inventory service is not initialized"}};
        else try{
            const auto file=PS::HelpyPersonalization::File;std::error_code error;const auto stamp=std::filesystem::last_write_time(file,error);
            if(!error&&stamp!=state->MarksStamp){PS::HelpyPersonalization::Load(file);state->MarksStamp=stamp;}
            const auto revision=PS::HelpyPersonalization::Revision.load();value=PS::HelpyInventoryService::Run(PS::HostServices::StateDirectory(),request);
            if(revision!=PS::HelpyPersonalization::Revision.load()){state->MarksStamp=std::filesystem::last_write_time(file,error);nlohmann::json ignored;ReadCore(state,{{"Action","HelpyProfileChanged"}},ignored);}
        }
        catch(const std::exception& error){value={{"Error",error.what()}};}
        const auto text=value.dump();if(text.size()+1>RUNESCHEMA_PLUGIN_MAX_MESSAGE)return RS_PLUGIN_FAILED;
        *size=static_cast<uint32_t>(text.size()+1);if(!response||capacity<*size)return RS_PLUGIN_BUFFER_TOO_SMALL;
        std::memcpy(response,text.c_str(),*size);return RS_PLUGIN_OK;
    }catch(...){return RS_PLUGIN_FAILED;}
}
}
extern "C" __declspec(dllexport) const RuneSchemaPluginDescriptor* RS_PLUGIN_CALL RuneSchemaPlugin_Query()noexcept{return &Descriptor;}
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_Initialize(const RuneSchemaHostApi* host,void** instance)noexcept{
    if(!host||!instance||host->StructSize<sizeof(RuneSchemaHostApi)||host->ApiVersion!=RUNESCHEMA_PLUGIN_API_VERSION||!host->Log||!host->RegisterCapability||!host->RegisterService||!host->CallService)return RS_PLUGIN_INCOMPATIBLE_API;
    *instance=nullptr;auto* state=new(std::nothrow)State{host};if(!state)return RS_PLUGIN_FAILED;
    for(const char* name:{"helpy.navigation","helpy.item-lab","helpy.recipe-studio","helpy.map","authoring.live-preview","authoring.export"})if(host->RegisterCapability(Descriptor.Id,name)!=RS_PLUGIN_OK){delete state;return RS_PLUGIN_DUPLICATE;}
    const std::pair<const char*,RuneSchemaServiceFn> services[]{{"helpy.about",About},{"helpy.tools.push",Ignore},{"helpy.inventory",InventoryService},{"helpy.desktop.status",Status},{"helpy.desktop.connection",Ignore},{"helpy.native-overlay",NativeOverlay}};
    for(const auto& [name,callback]:services)if(host->RegisterService(Descriptor.Id,name,callback,state)!=RS_PLUGIN_OK){delete state;return RS_PLUGIN_DUPLICATE;}
    *instance=state;return RS_PLUGIN_OK;
}
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_OnUiInit(void*)noexcept{return RS_PLUGIN_OK;}
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_OnUnrealInit(void* context)noexcept{
    auto* state=static_cast<State*>(context);if(!state)return RS_PLUGIN_INVALID_ARGUMENT;
    try{
        wchar_t image[32768]{};const auto length=GetModuleFileNameW(Module,image,32768);if(!length||length>=32768)throw std::runtime_error("Cannot locate Helpy bridge");
        const auto plugin=std::filesystem::path(image).parent_path().parent_path();PS::HelpyPaths::Configure(plugin);PS::HelpySettings::Configure(plugin);PS::HelpySettings::EnsureLoaded();if(!PS::HelpySettings::IsEnabled())return RS_PLUGIN_OK;
        nlohmann::json environment,bridge;if(ReadCore(state,{{"Action","Environment"}},environment))PS::HelpyEnvironment::Configure(environment);
        state->Ready=ReadCore(state,{{"Action","DesktopBridgeStatus"}},bridge)&&bridge.value("Ready",false);
        PS::RuneSchemaProfile::Load(PS::HostServices::StateDirectory());PS::HelpyPersonalization::Load(PS::RuneSchemaProfile::Ready?PS::RuneSchemaProfile::Data("labels.json"):PS::HostServices::StateDirectory()/"personalization.json");state->InventoryReady=true;
        if(!state->Ready){state->Host->Log(RS_LOG_WARNING,Descriptor.Id,"[RS-UI-002] Helpy game bridge unavailable; the external app can still browse its cache offline.");return RS_PLUGIN_OK;}
        if(!PS::HelpySettings::StartAlongsideGame())return RS_PLUGIN_OK;
        USEROBJECTFLAGS flags{};DWORD bytes=0;if(!GetUserObjectInformationW(GetProcessWindowStation(),UOI_FLAGS,&flags,sizeof(flags),&bytes)||!(flags.dwFlags&WSF_VISIBLE))return RS_PLUGIN_OK;
        const auto executable=plugin/"dll/Helpy.exe";if(!std::filesystem::is_regular_file(executable)){state->Host->Log(RS_LOG_WARNING,Descriptor.Id,"[RS-UI-002] Helpy.exe is missing. Install the matching application beside the bridge DLL.");return RS_PLUGIN_OK;}
        const std::string platform=PS::HelpyEnvironment::Server()?"server":PS::HelpyEnvironment::Current==PS::HelpyEnvironment::Kind::GamePass?"gamepass":PS::HelpyEnvironment::Current==PS::HelpyEnvironment::Kind::Epic?"epic":"steam";
        const auto root=plugin.parent_path().parent_path();const bool hidden=!PS::HelpySettings::OpenOnGameStart();
        state->Launcher=std::jthread([state,root,platform,hidden]{try{PS::HelpyDesktopLaunch::Start(root,platform,hidden);}catch(const std::exception& error){const auto message="[RS-UI-002] Helpy desktop launch failed: "+std::string(error.what());state->Host->Log(RS_LOG_WARNING,Descriptor.Id,message.c_str());}});
        return RS_PLUGIN_OK;
    }catch(const std::exception& error){state->Host->Log(RS_LOG_ERROR,Descriptor.Id,error.what());return RS_PLUGIN_FAILED;}catch(...){return RS_PLUGIN_FAILED;}
}
extern "C" __declspec(dllexport) void RS_PLUGIN_CALL RuneSchemaPlugin_Shutdown(void* context)noexcept{PS::HelpyNativeOverlayBridge::Shutdown();PS::RuneSchemaProfile::Shutdown();delete static_cast<State*>(context);}
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID reserved){if(reason==DLL_PROCESS_ATTACH)Module=module;if(reason==DLL_PROCESS_DETACH&&reserved)PS::ProcessExitInProgress.store(true,std::memory_order_relaxed);return TRUE;}
