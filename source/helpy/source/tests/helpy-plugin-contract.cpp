#include "Runtime/RuneSchemaPluginApi.h"
#include "Core/HelpyBranding.h"
#include "Core/RsmkResources.h"
#include <windows.h>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <iostream>
#include <source_location>
namespace {
struct Service{RuneSchemaServiceFn Call;void* Context;};
std::map<std::string,Service> Services;
std::vector<std::string> Capabilities;
std::vector<std::string> Logs;
bool ServerDesktop=false;
void Check(bool value,const std::source_location location=std::source_location::current()){
    if(!value)throw std::runtime_error("Plugin boundary contract failed at line "+std::to_string(location.line()));
}
void RS_PLUGIN_CALL Log(uint32_t,const char*,const char* message){Logs.emplace_back(message);}
int32_t RS_PLUGIN_CALL Capability(const char*,const char* value){Capabilities.push_back(value);return RS_PLUGIN_OK;}
int32_t RS_PLUGIN_CALL Register(const char*,const char* name,RuneSchemaServiceFn call,void* context){Services.emplace(name,Service{call,context});return RS_PLUGIN_OK;}
int32_t RS_PLUGIN_CALL Call(const char*,const char* service,const char* request,char* response,uint32_t capacity,uint32_t* size){
    if(!ServerDesktop||std::string(service)!="runeschema.tools"||nlohmann::json::parse(request).value("Action",std::string{})!="Environment")return RS_PLUGIN_NOT_FOUND;
    const auto value=nlohmann::json{{"Storefront","Steam/GOG"},{"DedicatedServer",true}}.dump();
    *size=static_cast<uint32_t>(value.size()+1);if(!response||capacity<*size)return RS_PLUGIN_BUFFER_TOO_SMALL;
    std::memcpy(response,value.c_str(),*size);return RS_PLUGIN_OK;
}
}
int Run(int argc,char** argv){
    Check(argc==2||argc==3);ServerDesktop=argc==3&&std::string(argv[2])=="--server-desktop";
    auto dll=LoadLibraryA(argv[1]);Check(dll!=nullptr);
    const auto query=reinterpret_cast<RuneSchemaPluginQueryFn>(GetProcAddress(dll,"RuneSchemaPlugin_Query"));
    const auto initialize=reinterpret_cast<RuneSchemaPluginInitializeFn>(GetProcAddress(dll,"RuneSchemaPlugin_Initialize"));
    const auto shutdown=reinterpret_cast<RuneSchemaPluginShutdownFn>(GetProcAddress(dll,"RuneSchemaPlugin_Shutdown"));
    Check(query&&initialize&&shutdown);const auto* descriptor=query();
    Check(descriptor->ApiVersion==1&&std::string(descriptor->Id)=="RuneSchema.Helpy");
    Check(std::string(descriptor->Name)=="RSMK (RuneSchema Modding Kit)");
    Check(FindResourceW(dll,MAKEINTRESOURCEW(IDI_RSMK),RT_GROUP_ICON)!=nullptr);
    const auto small=static_cast<HICON>(LoadImageW(dll,MAKEINTRESOURCEW(IDI_RSMK),IMAGE_ICON,16,16,LR_SHARED));
    const auto large=static_cast<HICON>(LoadImageW(dll,MAKEINTRESOURCEW(IDI_RSMK),IMAGE_ICON,32,32,LR_SHARED));
    Check(small&&large);
    RuneSchemaHostApi host{};host.StructSize=sizeof(host);host.ApiVersion=1;
    void* instance=nullptr;
    Check(initialize(nullptr,&instance)==RS_PLUGIN_INCOMPATIBLE_API);
    Check(initialize(&host,&instance)==RS_PLUGIN_INCOMPATIBLE_API);
    host.Log=Log;host.RegisterCapability=Capability;host.RegisterService=Register;host.CallService=Call;
    host.ApiVersion=999;Check(initialize(&host,&instance)==RS_PLUGIN_INCOMPATIBLE_API);host.ApiVersion=1;
    Check(initialize(&host,&instance)==RS_PLUGIN_OK&&instance);
    const auto cleanup=[&](void*){shutdown(instance);FreeLibrary(dll);};
    std::unique_ptr<void,decltype(cleanup)> guard(reinterpret_cast<void*>(1),cleanup);
    Check(Capabilities.size()==6&&Services.size()==2);
    // Mirror the real host's post-initialize manifest gate. Loading the DLL
    // alone does not prove the host will keep it or start its F2 window.
    auto manifest=std::filesystem::path(argv[1]).parent_path().parent_path()/"plugin.json";
    if(!std::filesystem::exists(manifest))manifest=std::filesystem::path(HELPY_PLUGIN_PACKAGE_ROOT)/"plugin.json";
    std::ifstream manifestFile(manifest);Check(manifestFile.good());
    const auto declaration=nlohmann::json::parse(manifestFile);
    Check(declaration.at("Version")==descriptor->Version);
    for(const auto& capability:declaration.at("Capabilities"))
        Check(std::find(Capabilities.begin(),Capabilities.end(),capability.get<std::string>())!=Capabilities.end());
    auto about=Services.at("helpy.about");uint32_t size=0;
    Check(about.Call(about.Context,"{}",nullptr,0,&size)==RS_PLUGIN_BUFFER_TOO_SMALL&&size>0);
    std::vector<char> buffer(size);
    Check(about.Call(about.Context,"{}",buffer.data(),size,&size)==RS_PLUGIN_OK);
    Check(nlohmann::json::parse(buffer.data()).at("plugin")=="RuneSchema.Helpy");
    auto push=Services.at("helpy.tools.push");
    Check(push.Call(push.Context,"invalid json",nullptr,0,&size)==RS_PLUGIN_FAILED);
    Check(push.Call(push.Context,R"({"Kind":"bootstrap","Available":true,"Generation":2,"Payload":{"Result":{"Status":"Ready"},"Catalog":{}}})",nullptr,0,&size)==RS_PLUGIN_OK);
    Check(push.Call(push.Context,R"({"Kind":"clear","Available":false,"Generation":3,"Payload":{"Status":"World changed"}})",nullptr,0,&size)==RS_PLUGIN_OK);
    if(argc==3&&(std::string(argv[2])=="--start-hidden"||ServerDesktop)){
        const auto start=reinterpret_cast<RuneSchemaPluginPhaseFn>(GetProcAddress(dll,"RuneSchemaPlugin_OnUnrealInit"));
        Check(start&&start(instance)==RS_PLUGIN_OK);
        bool ready=false;for(const auto& log:Logs)if(log.find("standalone RSMK ready")!=std::string::npos)ready=true;
        Check(ready);
        if(ServerDesktop){
            HWND own=nullptr;
            for(HWND window=FindWindowExW(nullptr,nullptr,L"RuneSchema.Helpy.Window",nullptr);window;window=FindWindowExW(nullptr,window,L"RuneSchema.Helpy.Window",nullptr)){
                DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(pid==GetCurrentProcessId()){own=window;break;}
            }
            Check(own&&IsWindowVisible(own));
            wchar_t title[128]{};GetWindowTextW(own,title,128);Check(std::wstring(title)==PS::HelpyBranding::WindowTitle);
            Check(SendMessageW(own,WM_GETICON,ICON_SMALL,0)!=0&&SendMessageW(own,WM_GETICON,ICON_BIG,0)!=0);
            SendMessageW(own,WM_CLOSE,0,0);Check(IsWindowVisible(own)&&IsIconic(own));
            ShowWindow(own,SW_SHOWNOACTIVATE);Check(IsWindowVisible(own));
        }
    }
    return 0;
}
int main(int argc,char** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    try{return Run(argc,argv);}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
