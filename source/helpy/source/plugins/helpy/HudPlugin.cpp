#include "Runtime/RuneSchemaPluginApi.h"
#include "Runtime/HelpyNativeOverlayBridge.h"
#include "RuneSchemaHudApi.h"
#include <windows.h>
#include <bcrypt.h>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <cctype>
#include <cstring>

namespace {
using Json=nlohmann::json;
constexpr RuneSchemaPluginDescriptor Descriptor{sizeof(RuneSchemaPluginDescriptor),1,"RuneSchema.HUD","RuneSchema shared HUD","0.8.1.0"};
std::atomic<const RuneSchemaHostApi*> Host=nullptr;
std::mutex Gate;
struct Client{std::string Owner;DWORD Pid{};HANDLE Process{},Wake{};std::string LastError;bool UiReady=false;uint32_t Command=RS_HUD_COMMAND_NONE;};
std::unordered_map<uint64_t,Client> Clients;
bool SameUser(HANDLE process){
    HANDLE tokens[2]{};
    const bool opened=OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&tokens[0])&&OpenProcessToken(process,TOKEN_QUERY,&tokens[1]);
    std::vector<unsigned char> users[2];bool valid=opened;
    for(unsigned i=0;valid&&i<2;++i){DWORD bytes=0;GetTokenInformation(tokens[i],TokenUser,nullptr,0,&bytes);users[i].resize(bytes);valid=bytes&&GetTokenInformation(tokens[i],TokenUser,users[i].data(),bytes,&bytes);}
    if(valid)valid=EqualSid(reinterpret_cast<TOKEN_USER*>(users[0].data())->User.Sid,reinterpret_cast<TOKEN_USER*>(users[1].data())->User.Sid)!=FALSE;
    for(auto token:tokens)if(token)CloseHandle(token);
    return valid;
}
void InputChanged(bool active){
    const auto* host=Host.load();if(!host)return;
    char output[128]{};uint32_t bytes=0;
    host->CallService(Descriptor.Id,"runeschema.tools",active?R"({"Action":"HudInputCapture","Active":true})":R"({"Action":"HudInputCapture","Active":false})",output,sizeof(output),&bytes);
}
int32_t Reply(const Json& value,char* output,uint32_t capacity,uint32_t* size){
    const auto text=value.dump();if(text.size()+1>RS_HUD_MAX_MESSAGE)return RS_HUD_FAILED;
    *size=uint32_t(text.size()+1);if(!output||capacity<*size)return RS_HUD_BUFFER_TOO_SMALL;
    std::memcpy(output,text.c_str(),*size);return RS_HUD_OK;
}
Json Native(const Json& request){
    try{return PS::HelpyNativeOverlayBridge::Call(request);}
    catch(const std::exception& error){auto status=PS::HelpyNativeOverlayBridge::Call({{"Action","Status"},{"Client",request.value("Client",uint64_t(1))}});status["Failed"]=true;status["Error"]=error.what();return status;}
}
int32_t RS_HUD_CALL Register(const char* owner,uint32_t pid,uint64_t* client){
    if(client)*client=0;
    if(!owner||!pid||!client)return RS_HUD_INVALID_ARGUMENT;
    const auto length=strnlen_s(owner,129);if(!length||length>128)return RS_HUD_INVALID_ARGUMENT;
    for(size_t i=0;i<length;++i)if(!std::isalnum(static_cast<unsigned char>(owner[i]))&&owner[i]!='.'&&owner[i]!='_'&&owner[i]!='-')return RS_HUD_INVALID_ARGUMENT;
    if(!Host.load())return RS_HUD_UNAVAILABLE;
    HANDLE process=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!process)return RS_HUD_NOT_FOUND;
    if(!SameUser(process)||WaitForSingleObject(process,0)!=WAIT_TIMEOUT){CloseHandle(process);return RS_HUD_INVALID_ARGUMENT;}
    try{
        std::lock_guard lock(Gate);
        for(auto row=Clients.begin();row!=Clients.end();)if(WaitForSingleObject(row->second.Process,0)!=WAIT_TIMEOUT){Native({{"Action","Release"},{"Client",row->first}});CloseHandle(row->second.Process);CloseHandle(row->second.Wake);row=Clients.erase(row);}else ++row;
        if(!Host.load()||Clients.size()>=32){CloseHandle(process);return RS_HUD_UNAVAILABLE;}
        uint64_t token=0;
        do{if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&token),sizeof(token),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0){CloseHandle(process);return RS_HUD_FAILED;}}while(token<2||Clients.contains(token));
        const auto wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!wake){CloseHandle(process);return RS_HUD_FAILED;}
        try{Clients.emplace(token,Client{owner,pid,process,wake,{}});}catch(...){CloseHandle(wake);throw;}
        *client=token;return RS_HUD_OK;
    }catch(...){CloseHandle(process);return RS_HUD_FAILED;}
}
int32_t RS_HUD_CALL Call(uint64_t client,const char* input,char* output,uint32_t capacity,uint32_t* size){
    if(!input||!size||capacity>RS_HUD_MAX_MESSAGE||(!output&&capacity)||strnlen_s(input,RS_HUD_MAX_MESSAGE+1)>RS_HUD_MAX_MESSAGE)return RS_HUD_INVALID_ARGUMENT;
    *size=0;if(!Host.load())return RS_HUD_UNAVAILABLE;
    // A sizing/retry call must not acquire, hide or replace another surface.
    if(!output||capacity<RS_HUD_MAX_MESSAGE){*size=RS_HUD_MAX_MESSAGE;return RS_HUD_BUFFER_TOO_SMALL;}
    try{
        auto request=Json::parse(input);if(!request.is_object()||!request.contains("Action")||!request["Action"].is_string())return RS_HUD_INVALID_ARGUMENT;
        const auto action=request["Action"].get<std::string>();
        if(action!="Prepare"&&action!="Status"&&action!="Open"&&action!="Show"&&action!="Hide"&&action!="Release"&&action!="UiReady")return RS_HUD_INVALID_ARGUMENT;
        std::lock_guard lock(Gate);const auto found=Clients.find(client);if(found==Clients.end())return RS_HUD_NOT_FOUND;
        auto& value=found->second;if(WaitForSingleObject(value.Process,0)!=WAIT_TIMEOUT)return RS_HUD_UNAVAILABLE;
        if(action=="UiReady"){
            if(!request.contains("Ready")||!request["Ready"].is_boolean())return RS_HUD_INVALID_ARGUMENT;
            value.UiReady=request["Ready"].get<bool>();value.Command=RS_HUD_COMMAND_NONE;
            if(!value.UiReady){value.Command=RS_HUD_COMMAND_DEACTIVATE;SetEvent(value.Wake);Native({{"Action","Release"},{"Client",client}});}
            return Reply({{"Accepted",true},{"UiReady",value.UiReady}},output,capacity,size);
        }
        request["Client"]=client;request["AppPid"]=value.Pid;
        if(action=="Open"){
            if(!request.contains("Interactive")||!request["Interactive"].is_boolean()||!request.contains("Nonce")||!request["Nonce"].is_string())return RS_HUD_INVALID_ARGUMENT;
            const auto nonce=request["Nonce"].get<std::string>();if(nonce.size()!=32||nonce.find_first_not_of("0123456789abcdef")!=nonce.npos)return RS_HUD_INVALID_ARGUMENT;
        }
        auto result=Native(request);result["Owner"]=value.Owner;result["UiReady"]=value.UiReady;
        const auto error=result.value("Error",std::string{});
        if(!error.empty()&&error!=value.LastError)if(const auto* host=Host.load())host->Log(RS_LOG_WARNING,Descriptor.Id,("[RS-OVL-002] owner="+value.Owner+" "+error).c_str());
        value.LastError=error;return Reply(result,output,capacity,size);
    }catch(...){return RS_HUD_INVALID_ARGUMENT;}
}
void RS_HUD_CALL Close(uint64_t client){
    try{std::lock_guard lock(Gate);const auto found=Clients.find(client);if(found==Clients.end())return;Native({{"Action","Release"},{"Client",client}});SetEvent(found->second.Wake);CloseHandle(found->second.Process);CloseHandle(found->second.Wake);Clients.erase(found);}catch(...){}
}
int32_t RS_HUD_CALL NextCommand(uint64_t client,uint32_t* command){
    if(!command)return RS_HUD_INVALID_ARGUMENT;*command=RS_HUD_COMMAND_NONE;
    std::lock_guard lock(Gate);const auto found=Clients.find(client);if(found==Clients.end())return RS_HUD_NOT_FOUND;
    *command=std::exchange(found->second.Command,uint32_t(RS_HUD_COMMAND_NONE));return RS_HUD_OK;
}
int32_t RS_HUD_CALL WakeEvent(uint64_t client,uint64_t* handle){
    if(!handle)return RS_HUD_INVALID_ARGUMENT;*handle=0;
    std::lock_guard lock(Gate);const auto found=Clients.find(client);if(found==Clients.end())return RS_HUD_NOT_FOUND;
    HANDLE copy{};if(!DuplicateHandle(GetCurrentProcess(),found->second.Wake,GetCurrentProcess(),&copy,0,FALSE,DUPLICATE_SAME_ACCESS))return RS_HUD_FAILED;
    *handle=uint64_t(reinterpret_cast<uintptr_t>(copy));return RS_HUD_OK;
}
int32_t RS_PLUGIN_CALL Extensions(void*,const char* input,char* output,uint32_t capacity,uint32_t* size){
    if(!input||!size||capacity>RUNESCHEMA_PLUGIN_MAX_MESSAGE||strnlen_s(input,RS_HUD_MAX_MESSAGE+1)>RS_HUD_MAX_MESSAGE)return RS_PLUGIN_INVALID_ARGUMENT;
    if(!output||capacity<RS_HUD_MAX_MESSAGE){*size=RS_HUD_MAX_MESSAGE;return RS_PLUGIN_BUFFER_TOO_SMALL;}
    if(!Host.load())return RS_PLUGIN_NOT_FOUND;
    try{
        const auto request=Json::parse(input);const auto action=request.at("Action").get<std::string>();std::lock_guard lock(Gate);Json result;
        if(action=="List"){
            auto entries=Json::array();for(const auto& [id,client]:Clients)entries.push_back({{"Client",id},{"Owner",client.Owner},{"UiReady",client.UiReady&&WaitForSingleObject(client.Process,0)==WAIT_TIMEOUT}});
            result={{"Available",true},{"Clients",entries}};
        }else if(action=="Activate"||action=="Deactivate"){
            const auto found=Clients.find(request.at("Client").get<uint64_t>());
            if(found==Clients.end()||WaitForSingleObject(found->second.Process,0)!=WAIT_TIMEOUT)result={{"Accepted",false},{"Error","The extension is disconnected"}};
            else if(action=="Activate"&&!found->second.UiReady)result={{"Accepted",false},{"Error","The extension has not verified its UI producer"}};
            else{found->second.Command=action=="Activate"?RS_HUD_COMMAND_ACTIVATE:RS_HUD_COMMAND_DEACTIVATE;SetEvent(found->second.Wake);result={{"Accepted",true}};}
        }else return RS_PLUGIN_INVALID_ARGUMENT;
        return Reply(result,output,capacity,size)==RS_HUD_OK?RS_PLUGIN_OK:RS_PLUGIN_FAILED;
    }catch(...){return RS_PLUGIN_INVALID_ARGUMENT;}
}
// The existing Helpy frontend retains its own authenticated Core transport.
int32_t RS_PLUGIN_CALL Helpy(void*,const char* input,char* output,uint32_t capacity,uint32_t* size){
    if(!input||!size||capacity>RUNESCHEMA_PLUGIN_MAX_MESSAGE||strnlen_s(input,RS_HUD_MAX_MESSAGE+1)>RS_HUD_MAX_MESSAGE)return RS_PLUGIN_INVALID_ARGUMENT;
    if(!output||capacity<RS_HUD_MAX_MESSAGE){*size=RS_HUD_MAX_MESSAGE;return RS_PLUGIN_BUFFER_TOO_SMALL;}
    if(!Host.load())return RS_PLUGIN_NOT_FOUND;
    try{auto request=Json::parse(input);request["Client"]=uint64_t(1);return Reply(Native(request),output,capacity,size)==RS_HUD_OK?RS_PLUGIN_OK:RS_PLUGIN_FAILED;}catch(...){return RS_PLUGIN_INVALID_ARGUMENT;}
}
const RuneSchemaHudApi Api{sizeof(RuneSchemaHudApi),1,Register,Call,Close,NextCommand,WakeEvent};
}
extern "C" __declspec(dllexport) const RuneSchemaHudApi* RS_HUD_CALL RuneSchemaHud_Query() noexcept{return &Api;}
extern "C" __declspec(dllexport) const RuneSchemaPluginDescriptor* RS_PLUGIN_CALL RuneSchemaPlugin_Query() noexcept{return &Descriptor;}
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_Initialize(const RuneSchemaHostApi* host,void** instance) noexcept{
    if(!host||!instance||host->StructSize<sizeof(RuneSchemaHostApi)||!host->CallService||!host->RegisterService||!host->RegisterCapability||!host->Log)return RS_PLUGIN_INCOMPATIBLE_API;
    *instance=nullptr;
    try{char response[128]{};uint32_t bytes=0;if(host->CallService(Descriptor.Id,"runeschema.tools",R"({"Action":"HudCapabilities"})",response,sizeof(response),&bytes)!=RS_PLUGIN_OK)return RS_PLUGIN_INCOMPATIBLE_API;const auto capabilities=Json::parse(response);if(!capabilities.value("InputOwnership",false)||!capabilities.value("Extensions",false))return RS_PLUGIN_INCOMPATIBLE_API;}catch(...){return RS_PLUGIN_INCOMPATIBLE_API;}
    if(host->RegisterCapability(Descriptor.Id,"hud.surface")!=RS_PLUGIN_OK||host->RegisterCapability(Descriptor.Id,"hud.input-owner")!=RS_PLUGIN_OK||host->RegisterCapability(Descriptor.Id,"hud.extensions")!=RS_PLUGIN_OK||host->RegisterService(Descriptor.Id,"hud.helpy",Helpy,nullptr)!=RS_PLUGIN_OK||host->RegisterService(Descriptor.Id,"hud.extensions",Extensions,nullptr)!=RS_PLUGIN_OK)return RS_PLUGIN_FAILED;
    Host=host;PS::HelpyNativeOverlayBridge::SetInputObserver(InputChanged);*instance=const_cast<RuneSchemaHudApi*>(&Api);return RS_PLUGIN_OK;
}
extern "C" __declspec(dllexport) void RS_PLUGIN_CALL RuneSchemaPlugin_Shutdown(void*) noexcept{
    PS::HelpyNativeOverlayBridge::Shutdown();InputChanged(false);Host=nullptr;
    PS::HelpyNativeOverlayBridge::SetInputObserver(nullptr);
    std::lock_guard lock(Gate);for(const auto& [id,client]:Clients){SetEvent(client.Wake);CloseHandle(client.Process);CloseHandle(client.Wake);}Clients.clear();
}
