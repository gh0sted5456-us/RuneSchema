#include "Runtime/RuneSchemaPluginApi.h"
#include "DeveloperToolsKey.h"
#include "Core/CacheBundleIO.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <cassert>
#include <cstring>
#include <iostream>
namespace {
using Json=nlohmann::json;RuneSchemaServiceFn Service=nullptr;void* Context=nullptr;bool WrongHost=false;unsigned Calls=0;
void RS_PLUGIN_CALL Log(uint32_t,const char*,const char*){}
int32_t RS_PLUGIN_CALL Capability(const char* owner,const char* id){return std::string_view(owner)=="RuneSchema.DeveloperTools"&&std::string_view(id)=="helpy.developer-tools"?RS_PLUGIN_OK:RS_PLUGIN_INVALID_ARGUMENT;}
int32_t RS_PLUGIN_CALL Register(const char* owner,const char* id,RuneSchemaServiceFn call,void* context){if(Capability(owner,id))return RS_PLUGIN_INVALID_ARGUMENT;Service=call;Context=context;return RS_PLUGIN_OK;}
int32_t RS_PLUGIN_CALL Call(const char*,const char* name,const char*,char* output,uint32_t capacity,uint32_t* size){++Calls;assert(std::string_view(name)=="helpy.host");const auto text=Json{{"Process",WrongHost?"Game":"Helpy"}}.dump();*size=uint32_t(text.size()+1);assert(capacity>=*size);std::memcpy(output,text.c_str(),*size);return RS_PLUGIN_OK;}
Json Exchange(const Json& request){const auto text=request.dump();char output[4096]{};uint32_t size=0;assert(Service(Context,text.c_str(),output,sizeof(output),&size)==RS_PLUGIN_OK);return Json::parse(output);}
}
int main(int argc,char** argv){
 assert(argc==3);namespace fs=std::filesystem;
 assert(PS::CacheBundleIO::Hash(PS::DeveloperToolsKey::Normalize(PS::CacheBundleIO::Read(fs::u8path(argv[2]),8192)))==PS::DeveloperToolsKey::LicenseHash);
 assert(PS::DeveloperToolsKey::ValidText(std::string("\xef\xbb\xbf")+PS::DeveloperToolsKey::LicenseHash+"\r\n"));
 assert(!PS::DeveloperToolsKey::ValidText("wrong"));
 const auto root=fs::temp_directory_path()/("RS-developer-plugin-"+std::to_string(GetCurrentProcessId()));assert(!fs::exists(root));
 struct Cleanup{fs::path Root;~Cleanup(){std::error_code error;fs::remove_all(Root,error);}} cleanup{root};
 fs::create_directories(root/"dll");const auto dll=root/"dll/RuneSchema.DeveloperTools.dll";fs::copy_file(fs::u8path(argv[1]),dll);
 const auto module=LoadLibraryExW(dll.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);assert(module);
 const auto start=reinterpret_cast<RuneSchemaPluginInitializeFn>(GetProcAddress(module,"RuneSchemaPlugin_Initialize"));const auto stop=reinterpret_cast<RuneSchemaPluginShutdownFn>(GetProcAddress(module,"RuneSchemaPlugin_Shutdown"));const auto query=reinterpret_cast<RuneSchemaPluginQueryFn>(GetProcAddress(module,"RuneSchemaPlugin_Query"));assert(start&&stop&&query&&std::string_view(query()->Id)=="RuneSchema.DeveloperTools");
 RuneSchemaHostApi host{sizeof(RuneSchemaHostApi),RUNESCHEMA_PLUGIN_API_VERSION,Log,Capability,Register,Call};void* instance=nullptr;
 assert(start(&host,&instance)==RS_PLUGIN_NOT_FOUND&&!instance&&!Service&&!Calls);
 const auto key=root/"devmenu.key";
 {std::ofstream(key)<<"invalid";}assert(start(&host,&instance)==RS_PLUGIN_NOT_FOUND&&!instance&&!Service&&!Calls);
 {std::ofstream(key)<<std::string(300,'x');}assert(!PS::DeveloperToolsKey::Present(root));
 {std::ofstream(key)<<PS::DeveloperToolsKey::LicenseHash<<"\n";}
 WrongHost=true;assert(start(&host,&instance)==RS_PLUGIN_INCOMPATIBLE_API&&!instance&&!Service);WrongHost=false;assert(start(&host,&instance)==RS_PLUGIN_OK&&instance&&Service);
 const auto metadata=Exchange({{"Op","read"}});assert(metadata.at("IndependentImplementation")&&metadata.at("Name")=="Advanced Tooling"&&metadata.at("Groups").back()=="Compatibility");assert(Exchange({{"Op","prepare"},{"Command","inspect"}}).at("Request").at("Action")=="InspectDeveloperContracts");assert(Exchange({{"Op","prepare"},{"Command","players"}}).at("Request").at("Action")=="QuickPlayers");
 uint32_t size=0;assert(Service(Context,R"({"Op":"prepare","Command":"execute arbitrary function"})",nullptr,0,&size)==RS_PLUGIN_INVALID_ARGUMENT);assert(Calls==2);fs::remove(key);assert(Service(Context,R"({"Op":"read"})",nullptr,0,&size)==RS_PLUGIN_NOT_FOUND);stop(instance);assert(FreeLibrary(module));
 std::cout<<"Developer plugin: native identity, Helpy-only host, license-hash key required, revocation, pure discovery, whitelisted requests and no automatic game calls passed.\n";
}
