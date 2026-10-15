#include "Runtime/RuneSchemaPluginApi.h"
#include <windows.h>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
namespace {
struct Service{RuneSchemaServiceFn Call;void* Context;};
std::map<std::string,Service> Services;
std::vector<std::string> Capabilities;
std::vector<std::string> Logs;
void Check(bool value){if(!value)throw std::runtime_error("Plugin boundary contract failed");}
void RS_PLUGIN_CALL Log(uint32_t,const char*,const char* message){Logs.emplace_back(message);}
int32_t RS_PLUGIN_CALL Capability(const char*,const char* value){Capabilities.push_back(value);return RS_PLUGIN_OK;}
int32_t RS_PLUGIN_CALL Register(const char*,const char* name,RuneSchemaServiceFn call,void* context){Services.emplace(name,Service{call,context});return RS_PLUGIN_OK;}
int32_t RS_PLUGIN_CALL Call(const char*,const char*,const char*,char*,uint32_t,uint32_t*){return RS_PLUGIN_NOT_FOUND;}
}
int main(int argc,char** argv){
    Check(argc==2||argc==3);auto dll=LoadLibraryA(argv[1]);Check(dll!=nullptr);
    const auto query=reinterpret_cast<RuneSchemaPluginQueryFn>(GetProcAddress(dll,"RuneSchemaPlugin_Query"));
    const auto initialize=reinterpret_cast<RuneSchemaPluginInitializeFn>(GetProcAddress(dll,"RuneSchemaPlugin_Initialize"));
    const auto shutdown=reinterpret_cast<RuneSchemaPluginShutdownFn>(GetProcAddress(dll,"RuneSchemaPlugin_Shutdown"));
    Check(query&&initialize&&shutdown);const auto* descriptor=query();
    Check(descriptor->ApiVersion==1&&std::string(descriptor->Id)=="RuneSchema.Helpy");
    RuneSchemaHostApi host{};host.StructSize=sizeof(host);host.ApiVersion=1;
    void* instance=nullptr;
    Check(initialize(nullptr,&instance)==RS_PLUGIN_INCOMPATIBLE_API);
    Check(initialize(&host,&instance)==RS_PLUGIN_INCOMPATIBLE_API);
    host.Log=Log;host.RegisterCapability=Capability;host.RegisterService=Register;host.CallService=Call;
    host.ApiVersion=999;Check(initialize(&host,&instance)==RS_PLUGIN_INCOMPATIBLE_API);host.ApiVersion=1;
    Check(initialize(&host,&instance)==RS_PLUGIN_OK&&instance);
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
    if(argc==3&&std::string(argv[2])=="--start-hidden"){
        const auto start=reinterpret_cast<RuneSchemaPluginPhaseFn>(GetProcAddress(dll,"RuneSchemaPlugin_OnUnrealInit"));
        Check(start&&start(instance)==RS_PLUGIN_OK);
        bool ready=false;for(const auto& log:Logs)if(log.find("standalone Helpy ready")!=std::string::npos)ready=true;
        Check(ready);
    }
    shutdown(instance);FreeLibrary(dll);
}
