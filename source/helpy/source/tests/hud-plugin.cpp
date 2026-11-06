#include "RuneSchemaHudApi.h"
#include "HudSurfaceOwner.h"
#include "RuneSchemaHudSurface.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include <windows.h>
#include <cassert>
#include <cstring>
#include <set>
#include <string>
#include <cstdio>
static std::set<std::string> Capabilities,Services;
static RuneSchemaServiceFn ExtensionService=nullptr;
static bool CompatibleCore=false;
static void RS_PLUGIN_CALL Log(uint32_t,const char*,const char*){}
static int32_t RS_PLUGIN_CALL Capability(const char*,const char* name){Capabilities.emplace(name);return RS_PLUGIN_OK;}
static int32_t RS_PLUGIN_CALL Service(const char*,const char* name,RuneSchemaServiceFn callback,void*){Services.emplace(name);if(std::string(name)=="hud.extensions")ExtensionService=callback;return RS_PLUGIN_OK;}
static int32_t RS_PLUGIN_CALL Core(const char*,const char*,const char* request,char* output,uint32_t capacity,uint32_t* size){
    if(!CompatibleCore||std::strcmp(request,R"({"Action":"HudCapabilities"})"))return RS_PLUGIN_NOT_FOUND;
    const char response[]=R"({"InputOwnership":true,"Extensions":true})";*size=sizeof(response);if(capacity<*size)return RS_PLUGIN_BUFFER_TOO_SMALL;std::memcpy(output,response,sizeof(response));return RS_PLUGIN_OK;
}
int main(int argc,char** argv){
    assert(argc==2);PS::HudSurfaceOwner owner;
    assert(owner.CanOpen(1,false,false)&&!owner.CanOpen(0,true,false));
    owner.Open(1,false);assert(owner.CanOpen(2,true,true)&&!owner.CanOpen(2,false,true));
    owner.Open(2,true);assert(!owner.CanOpen(1,true,true)&&!owner.CanOpen(1,false,true));
    owner.Release(1);assert(owner.Owns(2));assert(owner.CanOpen(1,true,false));
    owner.Release(2);assert(!owner.Client&&!owner.Interactive);
    const auto library=LoadLibraryExA(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);assert(library);
    const auto query=reinterpret_cast<RuneSchemaHudQueryFn>(GetProcAddress(library,"RuneSchemaHud_Query"));assert(query);
    const auto* api=query();assert(api&&api->StructSize==sizeof(RuneSchemaHudApi)&&api->RegisterClient&&api->Call&&api->CloseClient);
    uint64_t first=0,second=0;assert(api->RegisterClient("Test",GetCurrentProcessId(),&first)==RS_HUD_UNAVAILABLE&&!first);
    const auto initialize=reinterpret_cast<RuneSchemaPluginInitializeFn>(GetProcAddress(library,"RuneSchemaPlugin_Initialize"));
    const auto shutdown=reinterpret_cast<RuneSchemaPluginShutdownFn>(GetProcAddress(library,"RuneSchemaPlugin_Shutdown"));assert(initialize&&shutdown);
    RuneSchemaHostApi host{};host.StructSize=sizeof(host);host.Log=Log;host.RegisterCapability=Capability;host.RegisterService=Service;host.CallService=Core;
    void* instance=nullptr;assert(initialize(&host,&instance)==RS_PLUGIN_INCOMPATIBLE_API&&!instance);CompatibleCore=true;
    assert(initialize(&host,&instance)==RS_PLUGIN_OK&&instance);
    assert(Capabilities.contains("hud.surface")&&Capabilities.contains("hud.input-owner")&&Services.contains("hud.helpy"));
    // Warm the Windows crypto provider before measuring adapter-owned handles.
    uint64_t warm=0;assert(api->RegisterClient("Fixture.Warmup",GetCurrentProcessId(),&warm)==RS_HUD_OK);api->CloseClient(warm);
    DWORD originalHandles=0;assert(GetProcessHandleCount(GetCurrentProcess(),&originalHandles));
    assert(api->RegisterClient("bad/path",GetCurrentProcessId(),&first)==RS_HUD_INVALID_ARGUMENT);
    assert(api->RegisterClient("Fixture.One",GetCurrentProcessId(),&first)==RS_HUD_OK&&first>1);
    assert(api->RegisterClient("Fixture.Two",GetCurrentProcessId(),&second)==RS_HUD_OK&&second>1&&second!=first);
    char output[RS_HUD_MAX_MESSAGE]{};uint32_t bytes=0;
    assert(ExtensionService&&ExtensionService(nullptr,R"({"Action":"List"})",output,sizeof(output),&bytes)==RS_PLUGIN_OK);
    const auto activate=std::string(R"({"Action":"Activate","Client":)")+std::to_string(second)+"}";
    assert(ExtensionService(nullptr,activate.c_str(),output,sizeof(output),&bytes)==RS_PLUGIN_OK&&std::strstr(output,"false"));
    uint32_t command=99;assert(api->NextCommand(second,&command)==RS_HUD_OK&&command==RS_HUD_COMMAND_NONE);
    uint64_t wake=0;assert(api->DuplicateWakeEvent(second,&wake)==RS_HUD_OK&&wake);
    const auto event=reinterpret_cast<HANDLE>(uintptr_t(wake));assert(WaitForSingleObject(event,0)==WAIT_TIMEOUT);
    assert(api->Call(second,R"({"Action":"UiReady","Ready":true})",output,sizeof(output),&bytes)==RS_HUD_OK);
    assert(ExtensionService(nullptr,activate.c_str(),output,sizeof(output),&bytes)==RS_PLUGIN_OK&&std::strstr(output,"true"));
    assert(WaitForSingleObject(event,0)==WAIT_OBJECT_0&&api->NextCommand(second,&command)==RS_HUD_OK&&command==RS_HUD_COMMAND_ACTIVATE);
    assert(api->NextCommand(second,&command)==RS_HUD_OK&&command==RS_HUD_COMMAND_NONE);
    assert(ExtensionService(nullptr,R"({"Action":"Activate","Owner":"Fixture.Two"})",output,sizeof(output),&bytes)==RS_PLUGIN_OK&&std::strstr(output,"true"));
    assert(api->NextCommand(second,&command)==RS_HUD_OK&&command==RS_HUD_COMMAND_ACTIVATE);
    uint64_t duplicate=0;assert(api->RegisterClient("Fixture.Two",GetCurrentProcessId(),&duplicate)==RS_HUD_OK);
    assert(ExtensionService(nullptr,R"({"Action":"Activate","Owner":"Fixture.Two"})",output,sizeof(output),&bytes)==RS_PLUGIN_OK&&std::strstr(output,"More than one"));api->CloseClient(duplicate);
    assert(ExtensionService(nullptr,R"({"Action":"Toggle","Owner":"Fixture.Two"})",output,sizeof(output),&bytes)==RS_PLUGIN_OK&&std::strstr(output,"true"));
    assert(api->NextCommand(second,&command)==RS_HUD_OK&&command==RS_HUD_COMMAND_ACTIVATE);
    const auto deactivate=std::string(R"({"Action":"Deactivate","Client":)")+std::to_string(second)+"}";
    assert(ExtensionService(nullptr,deactivate.c_str(),output,sizeof(output),&bytes)==RS_PLUGIN_OK);
    assert(WaitForSingleObject(event,0)==WAIT_OBJECT_0&&api->NextCommand(second,&command)==RS_HUD_OK&&command==RS_HUD_COMMAND_DEACTIVATE);
    assert(api->Call(first,R"({"Action":"Status"})",nullptr,0,&bytes)==RS_HUD_BUFFER_TOO_SMALL&&bytes==sizeof(output));
    assert(api->Call(first,R"({"Action":"Status"})",output,sizeof(output),&bytes)==RS_HUD_OK&&std::strstr(output,"Fixture.One"));
    assert(api->Call(second,R"({"Action":"GuideReservation","Enabled":true})",output,sizeof(output),&bytes)==RS_HUD_INVALID_ARGUMENT);
    assert(api->Call(second,R"({"Action":"Open","Interactive":true})",output,sizeof(output),&bytes)==RS_HUD_INVALID_ARGUMENT);
    assert(api->Call(second,"[]",output,sizeof(output),&bytes)==RS_HUD_INVALID_ARGUMENT);
    api->CloseClient(first);assert(api->Call(first,R"({"Action":"Status"})",output,sizeof(output),&bytes)==RS_HUD_NOT_FOUND);
    assert(api->Call(second,R"({"Action":"Status"})",output,sizeof(output),&bytes)==RS_HUD_OK);
    CloseHandle(event);api->CloseClient(second);api->CloseClient(second);
    DWORD steadyHandles=0;assert(GetProcessHandleCount(GetCurrentProcess(),&steadyHandles));
    for(unsigned i=0;i<100;++i){
        uint64_t repeated=0,signal=0;assert(api->RegisterClient("Fixture.Repeated",GetCurrentProcessId(),&repeated)==RS_HUD_OK);
        assert(api->DuplicateWakeEvent(repeated,&signal)==RS_HUD_OK);
        assert(api->Call(repeated,R"({"Action":"UiReady","Ready":true})",output,sizeof(output),&bytes)==RS_HUD_OK);
        const auto open=std::string(R"({"Action":"Activate","Client":)")+std::to_string(repeated)+"}";
        assert(ExtensionService(nullptr,open.c_str(),output,sizeof(output),&bytes)==RS_PLUGIN_OK);
        assert(WaitForSingleObject(reinterpret_cast<HANDLE>(uintptr_t(signal)),0)==WAIT_OBJECT_0);
        assert(api->NextCommand(repeated,&command)==RS_HUD_OK&&command==RS_HUD_COMMAND_ACTIVATE);
        assert(api->Call(repeated,R"({"Action":"UiReady","Ready":false})",output,sizeof(output),&bytes)==RS_HUD_OK);
        assert(CloseHandle(reinterpret_cast<HANDLE>(uintptr_t(signal))));api->CloseClient(repeated);
    }
    DWORD closedHandles=0;assert(GetProcessHandleCount(GetCurrentProcess(),&closedHandles));
    std::fprintf(stderr,"HUD handle counts: registration=%lu, complete lifecycle=%lu, after 100 full lifecycles=%lu\n",originalHandles,steadyHandles,closedHandles);
    assert(closedHandles<=steadyHandles);
    shutdown(instance);
    assert(api->RegisterClient("AfterShutdown",GetCurrentProcessId(),&first)==RS_HUD_UNAVAILABLE);
    FreeLibrary(library);
}
