#include "Runtime/HelpyPluginHost.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <fstream>
#include <map>
#include <stdexcept>
#include <thread>
#include <iostream>
using Json=nlohmann::json;
namespace {
PS::HelpyPluginHost Plugins;
struct Service{RuneSchemaServiceFn Call;void* Context;std::string Owner;};std::map<std::string,Service> Services;
std::mutex Data;std::atomic<unsigned> Reads=0,Writes=0;Json LastWrite;bool WrongHost=false;
void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
unsigned DeveloperLocked=0,DeveloperLoadFailures=0;
void RS_PLUGIN_CALL Log(uint32_t,const char* owner,const char* text){
 if(std::string_view(owner)!="RuneSchema.DeveloperTools")return;
 if(std::string_view(text).starts_with("Advanced Tooling locked:"))++DeveloperLocked;
 if(std::string_view(text).starts_with("[RS-PLUG-003]"))++DeveloperLoadFailures;
}
int32_t RS_PLUGIN_CALL Capability(const char* owner,const char*){return Plugins.CanRegister(owner)?RS_PLUGIN_OK:RS_PLUGIN_INVALID_ARGUMENT;}
int32_t RS_PLUGIN_CALL Register(const char* owner,const char* name,RuneSchemaServiceFn call,void* context){if(!Plugins.CanRegister(owner))return RS_PLUGIN_INVALID_ARGUMENT;return Services.emplace(name,Service{call,context,owner}).second?RS_PLUGIN_OK:RS_PLUGIN_DUPLICATE;}
int32_t Reply(const Json& result,char* output,uint32_t capacity,uint32_t* size){const auto text=result.dump();*size=uint32_t(text.size()+1);if(!output||capacity<*size)return RS_PLUGIN_BUFFER_TOO_SMALL;std::memcpy(output,text.c_str(),*size);return RS_PLUGIN_OK;}
int32_t RS_PLUGIN_CALL Call(const char* owner,const char* name,const char* text,char* output,uint32_t capacity,uint32_t* size){
 if(!Plugins.Authorized(owner))return RS_PLUGIN_INVALID_ARGUMENT;
 if(std::string_view(name)=="helpy.host")return Reply({{"Process",WrongHost?"Game":"Helpy"}},output,capacity,size);
 if(std::string_view(name)!="runeschema.tools")return RS_PLUGIN_NOT_FOUND;
 const auto request=Json::parse(text);std::lock_guard lock(Data);
 if(request.at("Action")=="HelpyModMenuWrite"){
  if(capacity<4096){*size=4096;return RS_PLUGIN_BUFFER_TOO_SMALL;}++Writes;LastWrite=request.at("Payload");return Reply({{"Accepted",true}},output,capacity,size);
 }
 Check(request.at("Action")=="HelpyModMenuRead","Plugin called unrelated game tools");++Reads;
 return Reply({{"Available",true},{"Revision",1},{"Status","Ready"},{"Snapshot",{{"Pages",Json::array()}}}},output,capacity,size);
}
Json Exchange(const Json& request){const auto text=request.dump();std::vector<char> output(8192);uint32_t size=0;const auto service=Services.at("helpy.mod-menu2");auto code=service.Call(service.Context,text.c_str(),output.data(),uint32_t(output.size()),&size);if(code==RS_PLUGIN_BUFFER_TOO_SMALL){output.resize(size);code=service.Call(service.Context,text.c_str(),output.data(),uint32_t(output.size()),&size);}Check(code==RS_PLUGIN_OK,"Native service failed");return Json::parse(output.data());}
void Tick(uint32_t pid,bool unlocked=true){Check(Exchange({{"Op","tick"},{"Pid",pid},{"Unlocked",unlocked},{"Enabled",true},{"Visible",true}}).at("Accepted"),"Native tick rejected");}
void Write(const std::filesystem::path& path,std::string text){std::filesystem::create_directories(path.parent_path());std::ofstream(path,std::ios::binary)<<text;}
}
int main(int argc,char** argv)try{
 Check(argc==2,"Expected native plugin DLL");namespace fs=std::filesystem;
 const auto root=fs::temp_directory_path()/("RS-modmenu-plugin-"+std::to_string(GetCurrentProcessId()));Check(!fs::exists(root),"Fixture collision");
 struct Cleanup{fs::path Root;~Cleanup(){Plugins.Shutdown();std::error_code error;fs::remove_all(Root,error);}} cleanup{root};
 const auto plugin=root/"Helpy/plugins/RuneSchema.ModMenu2";Write(plugin/"plugin.json",R"({"Id":"RuneSchema.ModMenu2","Version":"0.8.1.0","Host":"helpy","EntryPoint":"RuneSchema.ModMenu2.dll"})");fs::create_directories(plugin/"dll");fs::copy_file(fs::u8path(argv[1]),plugin/"dll/RuneSchema.ModMenu2.dll");
 RuneSchemaHostApi host{sizeof(RuneSchemaHostApi),RUNESCHEMA_PLUGIN_API_VERSION,Log,Capability,Register,Call};
 const auto remove=[](const std::string& owner){std::erase_if(Services,[&](const auto& item){return item.second.Owner==owner;});};
 Plugins.Load(root,&host,remove);Check(Services.contains("helpy.mod-menu2")&&Plugins.Authorized("RuneSchema.ModMenu2"),"Helpy frontend did not load");Check(!Plugins.CanRegister("RuneSchema.ModMenu2"),"Registration stayed open after initialization");
 Tick(1,false);Check(!Reads&&!Writes,"Locked plugin called the game");
 for(unsigned i=0;i<200&&!Exchange({{"Op","read"},{"Revision",0}}).value("Available",false);++i){Tick(1);Sleep(2);}Check(Reads>0&&Exchange({{"Op","read"},{"Revision",0}}).value("Available",false),"Plugin did not publish native session state");
 const Json command={{"Op","set"},{"Id","Example"},{"Key","Enabled"},{"Value",true},{"Expected",false}};
 const auto service=Services.at("helpy.mod-menu2");const auto queue=Json{{"Op","queue"},{"Command",command}}.dump();uint32_t size=0;Check(service.Call(service.Context,queue.c_str(),nullptr,0,&size)==RS_PLUGIN_BUFFER_TOO_SMALL&&size==4096,"Mutation capacity probe mutated state");
 Tick(1);Sleep(5);Check(!Writes,"Queue capacity probe sent a command");Check(Exchange({{"Op","queue"},{"Command",command}}).at("Accepted"),"Command enqueue failed");
 for(unsigned i=0;i<200&&!Writes;++i){Tick(1);Sleep(2);}Check(Writes==1,"Command lost or duplicated");{std::lock_guard lock(Data);Check(LastWrite==command,"Native plugin changed the command payload");}
 for(unsigned i=0;i<20;++i){Tick(1);Sleep(2);}Check(Writes==1,"Read/poll retried a mutation");
 Tick(0);Check(!Exchange({{"Op","read"},{"Revision",0}}).value("Available",true),"Disconnected session retained availability");
 Plugins.Shutdown();Check(Services.empty()&&!Plugins.Authorized("RuneSchema.ModMenu2"),"Shutdown retained callbacks or ownership");
 Write(root/"settings/plugins.txt","RuneSchema.ModMenu2:0");Plugins.Load(root,&host,remove);Check(Services.empty(),"Disabled frontend initialized");
 Write(root/"settings/plugins.txt","RuneSchema.ModMenu2:1");WrongHost=true;Plugins.Load(root,&host,remove);Check(Services.empty(),"Mod Menu initialized in the wrong host");WrongHost=false;
 fs::remove(plugin/"dll/RuneSchema.ModMenu2.dll");Plugins.Load(root,&host,remove);Check(Services.empty(),"Missing optional plugin broke host startup");
 const auto developer=root/"Helpy/plugins/RuneSchema.DeveloperTools";
 Write(developer/"plugin.json",R"({"Id":"RuneSchema.DeveloperTools","Version":"0.8.1.0","Host":"helpy","EntryPoint":"RuneSchema.DeveloperTools.dll"})");
 Write(developer/"dll/RuneSchema.DeveloperTools.dll","invalid DLL fixture");
 Plugins.Load(root,&host,remove);Check(DeveloperLocked==1&&!DeveloperLoadFailures&&!Plugins.Authorized("RuneSchema.DeveloperTools"),"Host tried loading developer DLL without its key");
 Write(developer/"devmenu.key","wrong");Plugins.Load(root,&host,remove);Check(DeveloperLocked==2&&!DeveloperLoadFailures,"Invalid key passed host gate");
 Write(developer/"devmenu.key",PS::DeveloperToolsKey::LicenseHash);Plugins.Load(root,&host,remove);Check(DeveloperLocked==2&&DeveloperLoadFailures==1,"Valid key did not reach normal DLL validation");
 std::cout<<"Native Helpy Mod Menu plugin: host ownership, activation, payloads, capacity probes, no mutation retries, disconnect, disable and unload passed.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
