#pragma once
#include "Runtime/PluginCatalog.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include "Core/RuneSchemaDataRoot.h"
#include "DeveloperToolsKey.h"
#include <windows.h>
#include <mutex>
#include <unordered_set>
#include <functional>

namespace PS {
class HelpyPluginHost {
 struct Loaded {std::string Id;HMODULE Module;void* Instance;RuneSchemaPluginShutdownFn Shutdown;};
 std::vector<Loaded> loaded;
 mutable std::mutex gate;std::unordered_set<std::string> owners;std::string registering;DWORD registrationThread=0;
 std::function<void(const std::string&)> cleanup;
public:
 ~HelpyPluginHost(){Shutdown();}
 bool Authorized(const char* id)const{if(!id)return false;std::lock_guard lock(gate);return owners.contains(id);}
 bool CanRegister(const char* id)const{if(!id)return false;std::lock_guard lock(gate);return registrationThread==GetCurrentThreadId()&&registering==id;}
 void Load(const std::filesystem::path& root,const RuneSchemaHostApi* api,std::function<void(const std::string&)> removeServices){
  Shutdown();cleanup=std::move(removeServices);std::vector<std::string> errors;
  std::vector<PluginCatalog::Plugin> manifests;try{manifests=PluginCatalog::DiscoverRuntime(root,&errors);}catch(const std::exception& error){api->Log(RS_LOG_WARNING,"RuneSchema.Helpy",(std::string("[RS-PLUG-001] ")+error.what()).c_str());return;}
  for(const auto& text:errors)api->Log(RS_LOG_WARNING,"RuneSchema.Helpy",("[RS-PLUG-009] "+text).c_str());
  for(const auto& manifest:manifests){
   if(!manifest.Enabled||manifest.Host!="helpy")continue;
   if(manifest.Id=="RuneSchema.DeveloperTools"&&!DeveloperToolsKey::Present(manifest.Root)){api->Log(RS_LOG_INFO,manifest.Id.c_str(),"Advanced Tooling locked: add a valid devmenu.key and restart Helpy");continue;}
   HMODULE module=nullptr;void* instance=nullptr;RuneSchemaPluginShutdownFn stop=nullptr;const char* code="[RS-PLUG-005] ";
   try{
    if(manifest.ApiVersion!=RUNESCHEMA_PLUGIN_API_VERSION||manifest.EntryPoint.empty())throw std::runtime_error("Unsupported Helpy plugin API or missing entry point");
    RuneSchemaData::Safe(manifest.EntryPoint);
    code="[RS-PLUG-002] ";if(!std::filesystem::is_regular_file(manifest.EntryPoint))throw std::runtime_error("Declared Helpy plugin DLL is missing");
    code="[RS-PLUG-003] ";DWORD previousMode=0;const bool quiet=SetThreadErrorMode(SEM_FAILCRITICALERRORS,&previousMode)!=0;
    module=LoadLibraryExW(manifest.EntryPoint.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);const auto loadError=GetLastError();
    if(quiet)SetThreadErrorMode(previousMode,nullptr);
    if(!module)throw std::runtime_error("Cannot load Helpy plugin DLL (Windows error "+std::to_string(loadError)+")");
    const auto query=reinterpret_cast<RuneSchemaPluginQueryFn>(GetProcAddress(module,"RuneSchemaPlugin_Query"));
    const auto start=reinterpret_cast<RuneSchemaPluginInitializeFn>(GetProcAddress(module,"RuneSchemaPlugin_Initialize"));
    stop=reinterpret_cast<RuneSchemaPluginShutdownFn>(GetProcAddress(module,"RuneSchemaPlugin_Shutdown"));
    code="[RS-PLUG-004] ";if(!query||!start||!stop)throw std::runtime_error("Helpy plugin API exports are missing");
    code="[RS-PLUG-005] ";const auto* descriptor=query();
    if(!descriptor||descriptor->StructSize<sizeof(RuneSchemaPluginDescriptor)||descriptor->ApiVersion!=RUNESCHEMA_PLUGIN_API_VERSION||!descriptor->Id||manifest.Id!=descriptor->Id||!descriptor->Version||manifest.Version!=descriptor->Version||!start||!stop)throw std::runtime_error("Helpy plugin exports/identity do not match its manifest");
    {std::lock_guard lock(gate);owners.insert(manifest.Id);registering=manifest.Id;registrationThread=GetCurrentThreadId();}
    code="[RS-PLUG-006] ";if(start(api,&instance)!=RS_PLUGIN_OK||!instance)throw std::runtime_error("Helpy plugin initialization failed");
    code="[RS-PLUG-007] ";if(const auto ui=reinterpret_cast<RuneSchemaPluginPhaseFn>(GetProcAddress(module,"RuneSchemaPlugin_OnUiInit"));ui&&ui(instance)!=RS_PLUGIN_OK)throw std::runtime_error("Helpy plugin UI initialization failed");
    {std::lock_guard lock(gate);registering.clear();registrationThread=0;}
    loaded.push_back({manifest.Id,module,instance,stop});module=nullptr;instance=nullptr;
    api->Log(RS_LOG_INFO,manifest.Id.c_str(),"Native Helpy plugin loaded");
   }catch(const std::exception& error){
    if(instance&&stop)stop(instance);
    if(cleanup)cleanup(manifest.Id);
    {std::lock_guard lock(gate);owners.erase(manifest.Id);registering.clear();registrationThread=0;}
    if(module)FreeLibrary(module);api->Log(RS_LOG_WARNING,manifest.Id.c_str(),(std::string(code)+error.what()).c_str());
   }
  }
 }
 void Shutdown()noexcept{
  for(auto it=loaded.rbegin();it!=loaded.rend();++it){try{it->Shutdown(it->Instance);}catch(...){}
   if(cleanup)try{cleanup(it->Id);}catch(...){}
   {std::lock_guard lock(gate);owners.erase(it->Id);}FreeLibrary(it->Module);
  }loaded.clear();
 }
};
}
