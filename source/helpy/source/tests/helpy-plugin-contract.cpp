#include "Runtime/RuneSchemaPluginApi.h"
#include "Core/HelpyBranding.h"
#include "Core/RsmkResources.h"
#include "Core/HelpyNativeFrame.h"
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
    const auto value=nlohmann::json{{"Storefront","Steam/Epic"},{"DedicatedServer",true}}.dump();
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
    Check(std::string(descriptor->Version)==PS::HelpyBranding::Version);
    Check(FindResourceW(dll,MAKEINTRESOURCEW(IDI_RSMK),RT_GROUP_ICON)!=nullptr);
    const auto smallIcon=static_cast<HICON>(LoadImageW(dll,MAKEINTRESOURCEW(IDI_RSMK),IMAGE_ICON,16,16,LR_SHARED));
    const auto largeIcon=static_cast<HICON>(LoadImageW(dll,MAKEINTRESOURCEW(IDI_RSMK),IMAGE_ICON,32,32,LR_SHARED));
    Check(smallIcon&&largeIcon);
    RuneSchemaHostApi host{};host.StructSize=sizeof(host);host.ApiVersion=1;
    void* instance=nullptr;
    Check(initialize(nullptr,&instance)==RS_PLUGIN_INCOMPATIBLE_API);
    Check(initialize(&host,&instance)==RS_PLUGIN_INCOMPATIBLE_API);
    host.Log=Log;host.RegisterCapability=Capability;host.RegisterService=Register;host.CallService=Call;
    host.ApiVersion=999;Check(initialize(&host,&instance)==RS_PLUGIN_INCOMPATIBLE_API);host.ApiVersion=1;
    Check(initialize(&host,&instance)==RS_PLUGIN_OK&&instance);
    const auto cleanup=[&](void*){shutdown(instance);FreeLibrary(dll);};
    std::unique_ptr<void,decltype(cleanup)> guard(reinterpret_cast<void*>(1),cleanup);
    Check(Capabilities.size()==6&&Services.size()==3&&Services.contains("helpy.inventory"));
    {const auto inventory=Services.at("helpy.inventory");uint32_t bytes=0;
        Check(inventory.Call(inventory.Context,R"({"Action":"Capabilities"})",nullptr,0,&bytes)==RS_PLUGIN_BUFFER_TOO_SMALL);
        std::vector<char> reply(bytes);Check(inventory.Call(inventory.Context,R"({"Action":"Capabilities"})",reply.data(),bytes,&bytes)==RS_PLUGIN_OK);
        const auto caps=nlohmann::json::parse(reply.data());Check(caps.at("GameFavorites").is_boolean()&&!caps.contains("ScrapPlanner")&&!caps.contains("StorageFormat"));
        Check(inventory.Call(inventory.Context,R"({"Action":"CycleMark"})",nullptr,0,&bytes)==RS_PLUGIN_BUFFER_TOO_SMALL&&bytes==4096);
        Check(inventory.Call(inventory.Context,R"({"Action":"CycleMark"})",nullptr,0,&bytes)==RS_PLUGIN_BUFFER_TOO_SMALL&&bytes==4096);
        reply.resize(bytes-1);Check(inventory.Call(inventory.Context,R"({"Action":"CycleMark"})",reply.data(),static_cast<uint32_t>(reply.size()),&bytes)==RS_PLUGIN_BUFFER_TOO_SMALL&&bytes==4096);
    }
    Check(!Services.contains("helpy.vision.status"));
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
    Check(nlohmann::json::parse(buffer.data()).at("version")==descriptor->Version);
    auto push=Services.at("helpy.tools.push");
    Check(push.Call(push.Context,"invalid json",nullptr,0,&size)==RS_PLUGIN_FAILED);
    Check(push.Call(push.Context,R"({"Kind":"bootstrap","Available":true,"Generation":2,"Payload":{"Result":{"Status":"Ready"},"Catalog":{}}})",nullptr,0,&size)==RS_PLUGIN_OK);
    Check(push.Call(push.Context,R"({"Kind":"clear","Available":false,"Generation":3,"Payload":{"Status":"World changed"}})",nullptr,0,&size)==RS_PLUGIN_OK);
    if(argc==3&&(std::string(argv[2])=="--start-hidden"||ServerDesktop)){
        const auto start=reinterpret_cast<RuneSchemaPluginPhaseFn>(GetProcAddress(dll,"RuneSchemaPlugin_OnUnrealInit"));
        Check(start&&start(instance)==RS_PLUGIN_OK);
        bool ready=false;for(const auto& log:Logs)if(log.find("standalone RSMK ready")!=std::string::npos)ready=true;
        Check(ready);
        {const auto inventory=Services.at("helpy.inventory");uint32_t bytes=0;
            const char* invalid=R"({"Action":"ScrapPlan","Recipes":false})";
            Check(inventory.Call(inventory.Context,invalid,nullptr,0,&bytes)==RS_PLUGIN_BUFFER_TOO_SMALL);
            std::vector<char> reply(bytes);Check(inventory.Call(inventory.Context,invalid,reply.data(),bytes,&bytes)==RS_PLUGIN_OK);
            Check(nlohmann::json::parse(reply.data()).at("Error")=="Unknown Helpy inventory operation");
        }
        if(ServerDesktop){
            HWND own=nullptr;
            for(HWND window=FindWindowExW(nullptr,nullptr,L"RuneSchema.Helpy.Window",nullptr);window;window=FindWindowExW(nullptr,window,L"RuneSchema.Helpy.Window",nullptr)){
                DWORD pid=0;GetWindowThreadProcessId(window,&pid);wchar_t caption[128]{};GetWindowTextW(window,caption,128);
                if(pid==GetCurrentProcessId()&&std::wstring(caption)==PS::HelpyBranding::WindowTitle){own=window;break;}
            }
            Check(own&&IsWindowVisible(own));
            wchar_t title[128]{};GetWindowTextW(own,title,128);Check(std::wstring(title)==PS::HelpyBranding::WindowTitle);
            Check(SendMessageW(own,WM_GETICON,ICON_SMALL,0)!=0&&SendMessageW(own,WM_GETICON,ICON_BIG,0)!=0);
            POINT origin{};ClientToScreen(own,&origin);
            const auto dpi=GetDpiForWindow(own);const int caption=GetSystemMetricsForDpi(SM_CYCAPTION,dpi)+GetSystemMetricsForDpi(SM_CYSIZEFRAME,dpi)+GetSystemMetricsForDpi(SM_CXPADDEDBORDER,dpi);
            Check(SendMessageW(own,WM_NCHITTEST,0,MAKELPARAM(origin.x+100,origin.y+caption/2))==HTCAPTION);
            Check(SendMessageW(own,WM_NCHITTEST,0,MAKELPARAM(origin.x+10,origin.y+caption/2))==HTSYSMENU);
            PS::HelpyNativeFrame::Metrics frame;frame.Update(own,dpi);
            constexpr int hits[]{HTMINBUTTON,HTMAXBUTTON,HTCLOSE};
            for(int column=0;column<3;++column){const auto rect=frame.ControlRect(column);
                Check(SendMessageW(own,WM_NCHITTEST,0,MAKELPARAM(origin.x+(rect.left+rect.right)/2,origin.y+(rect.top+rect.bottom)/2))==hits[column]);}
            SendMessageW(own,WM_SYSCOMMAND,SC_MINIMIZE,0);Check(IsIconic(own));
            SendMessageW(own,WM_SYSCOMMAND,SC_RESTORE,0);Check(!IsIconic(own));
            ShowWindow(own,SW_MAXIMIZE);Check(IsZoomed(own));ShowWindow(own,SW_RESTORE);Check(!IsZoomed(own));
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
