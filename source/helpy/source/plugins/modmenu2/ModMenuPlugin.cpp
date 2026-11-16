#include "Core/HelpyBranding.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include <windows.h>
#include "OfflineModMenu.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstring>
#include <deque>
#include <future>
#include <mutex>
#include <string>
#include <vector>

namespace {
using Json=nlohmann::json;using Clock=std::chrono::steady_clock;
constexpr RuneSchemaPluginDescriptor Descriptor{sizeof(RuneSchemaPluginDescriptor),RUNESCHEMA_PLUGIN_API_VERSION,"RuneSchema.ModMenu2","Mod Menu 2",PS::HelpyBranding::Version};
constexpr uint32_t MaxSnapshot=RUNESCHEMA_PLUGIN_MAX_MESSAGE;
struct Session {
 const RuneSchemaHostApi* Host;std::mutex Gate;std::future<Json> Worker;
 std::deque<Json> Commands;Json Snapshot=Json::object();uint64_t Revision=~uint64_t{};uint32_t Pid=0;bool Available=false;
 Clock::time_point Next{};std::string Status;
 std::filesystem::path Root;std::unique_ptr<PS::OfflineModMenu> Offline;
 Json WorkOffline(const Json& command){
  if(Root.empty())return {{"Available",false},{"Status","Update Helpy to use offline Mod Menu settings."}};
  if(!PS::HelpyGameProcess::GameStopped()){
   Offline.reset();
   return {{"Available",false},{"Status","Dragonwilds is running. Waiting for the live Mod Menu bridge."}};
  }
  Json reply={{"Available",true},{"Status","Offline settings. Saved values are read when Dragonwilds starts."}};
  if(!Offline){
   Offline=std::make_unique<PS::OfflineModMenu>(Root);
   reply["Snapshot"]=Offline->TakeSnapshot();
  }
  if(!command.is_null()){
   const auto op=command.at("Op").get<std::string>();
   if(op=="stop"){Offline.reset();return {{"Available",false}};}
   if(op!="page")reply["Snapshot"]=Offline->Run(command);
  }
  return reply;
 }
 bool Remote(const Json& request,Json& result){
  const auto input=request.dump();std::vector<char> data(8192);uint32_t size=0;
  auto code=Host->CallService(Descriptor.Id,"runeschema.tools",input.c_str(),data.data(),uint32_t(data.size()),&size);
  if(code==RS_PLUGIN_BUFFER_TOO_SMALL&&size>data.size()&&size<=MaxSnapshot){data.resize(size);code=Host->CallService(Descriptor.Id,"runeschema.tools",input.c_str(),data.data(),uint32_t(data.size()),&size);}
  if(code!=RS_PLUGIN_OK||!size||size>data.size())return false;if(data[size-1]=='\0')--size;
  result=Json::parse(data.data(),data.data()+size);return true;
 }
 void Tick(uint32_t pid,bool unlocked,bool enabled,bool visible){
  if(pid!=Pid){Pid=pid;Revision=~uint64_t{};Snapshot=Json::object();Available=false;Commands.clear();Next={};}
  if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){
   try{auto reply=Worker.get();if(reply.value("_Pid",uint32_t{})==Pid){if(reply.contains("Available"))Available=reply.at("Available").get<bool>();Status=reply.value("Status",Status);if(reply.contains("Snapshot")){Snapshot=std::move(reply["Snapshot"]);Revision=Pid?reply.at("Revision").get<uint64_t>():Revision+1;}}}catch(const std::exception& error){Status=error.what();Available=false;}
  }
  if(!unlocked){Available=false;Commands.clear();return;}
  if((!enabled&&Commands.empty())||Worker.valid())return;
  if(!Pid&&!visible&&Commands.empty())return;
  const auto now=Clock::now();if(Commands.empty()&&now<Next)return;
  Next=now+std::chrono::milliseconds(pid&&visible?200:1000);
  if(!Pid){
   Json command;
   if(!Commands.empty()){command=std::move(Commands.front());Commands.pop_front();}
   Worker=std::async(std::launch::async,[this,command=std::move(command)]{
    auto reply=WorkOffline(command);reply["_Pid"]=0u;return reply;
   });
   return;
  }
  Offline.reset();
  Json request={{"Action","HelpyModMenuRead"},{"Revision",Revision}};
  if(!Commands.empty()){request={{"Action","HelpyModMenuWrite"},{"Payload",std::move(Commands.front())}};Commands.pop_front();}
  Worker=std::async(std::launch::async,[this,pid,request]{Json reply;if(!Remote(request,reply))reply={{"Available",false},{"Status","Mod Menu 2 is unavailable through this connection. Check the matching RuneSchema bridge."}};reply["_Pid"]=pid;return reply;});
 }
 void Queue(const Json& command){
  const auto op=command.at("Op").get<std::string>();if(op!="page"&&op!="set"&&op!="button"&&op!="reset"&&op!="refresh"&&op!="stop"&&op!="template_apply")throw std::runtime_error("Unknown Mod Menu command");
  if(!Pid&&op=="button")throw std::runtime_error("This action requires a running game.");
  if(command.dump().size()>(op=="template_apply"?512*1024:16384))throw std::runtime_error("Mod Menu command is too large");
  if(op=="page")for(auto& item:Commands)if(item.value("Op",std::string{})=="page"){item=command;Next={};return;}
  if(Commands.size()>=32)throw std::runtime_error("Wait for queued Mod Menu changes to finish.");Commands.push_back(command);Next={};
 }
 void Close(){
  Commands.clear();if(Worker.valid())try{Worker.get();}catch(...){}
  if(Pid&&Available)try{Json ignored;Remote({{"Action","HelpyModMenuWrite"},{"Payload",{{"Op","page"},{"Id",""},{"Rect",""}}}},ignored);}catch(...){}
  Pid=0;Available=false;Offline.reset();Snapshot=Json::object();Revision=~uint64_t{};Next={};
 }
};
int32_t Reply(const Json& value,char* output,uint32_t capacity,uint32_t* size){const auto text=value.dump();if(text.size()+1>MaxSnapshot)return RS_PLUGIN_FAILED;*size=uint32_t(text.size()+1);if(!output||capacity<*size)return RS_PLUGIN_BUFFER_TOO_SMALL;std::memcpy(output,text.c_str(),*size);return RS_PLUGIN_OK;}
int32_t RS_PLUGIN_CALL Exchange(void* context,const char* input,char* output,uint32_t capacity,uint32_t* size){
 if(!context||!input||!size||(!output&&capacity)||strnlen_s(input,524289)>524288)return RS_PLUGIN_INVALID_ARGUMENT;
 try{
  const auto request=Json::parse(input,[](int depth,Json::parse_event_t,Json&){if(depth>32)throw std::runtime_error("Mod Menu request is too deeply nested");return true;});
  const auto op=request.at("Op").get<std::string>();auto& state=*static_cast<Session*>(context);std::lock_guard lock(state.Gate);
  if(op=="read"){Json reply={{"Loaded",true},{"Available",state.Available},{"Offline",state.Available&&!state.Pid},{"Revision",state.Revision},{"Status",state.Status}};if(!request.value("MetadataOnly",false)&&request.value("Revision",~uint64_t{})!=state.Revision)reply["Snapshot"]=state.Snapshot;return Reply(reply,output,capacity,size);}
  if(op!="tick"&&op!="queue"&&op!="close")return RS_PLUGIN_INVALID_ARGUMENT;
  if(!output||capacity<4096){*size=4096;return RS_PLUGIN_BUFFER_TOO_SMALL;}
  if(op=="tick")state.Tick(request.at("Pid").get<uint32_t>(),request.at("Unlocked").get<bool>(),request.at("Enabled").get<bool>(),request.at("Visible").get<bool>());
  else if(op=="queue")state.Queue(request.at("Command"));else state.Close();
  return Reply({{"Accepted",true}},output,capacity,size);
 }catch(const std::exception& error){return Reply({{"Accepted",false},{"Status",error.what()}},output,capacity,size);}
}
}
extern "C" __declspec(dllexport) const RuneSchemaPluginDescriptor* RS_PLUGIN_CALL RuneSchemaPlugin_Query()noexcept{return &Descriptor;}
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_Initialize(const RuneSchemaHostApi* host,void** instance)noexcept{
 if(!host||!instance||host->StructSize<offsetof(RuneSchemaHostApi,FindObject)||host->ApiVersion!=RUNESCHEMA_PLUGIN_API_VERSION||!host->RegisterCapability||!host->RegisterService||!host->CallService)return RS_PLUGIN_INCOMPATIBLE_API;
 *instance=nullptr;try{std::vector<char> identity(4096);uint32_t size=0;
 if(host->CallService(Descriptor.Id,"helpy.host",R"({"ModMenuRuntime":true})",identity.data(),uint32_t(identity.size()),&size)!=RS_PLUGIN_OK||!size||size>identity.size())return RS_PLUGIN_INCOMPATIBLE_API;
 const auto details=Json::parse(identity.data(),identity.data()+size-1);
 if(details.value("Process",std::string{})!="Helpy")return RS_PLUGIN_INCOMPATIBLE_API;
 auto state=std::make_unique<Session>();state->Host=host;
 state->Root=std::filesystem::u8path(details.value("RuntimeRoot",std::string{}));if(host->RegisterCapability(Descriptor.Id,"helpy.mod-menu2")!=RS_PLUGIN_OK||host->RegisterService(Descriptor.Id,"helpy.mod-menu2",Exchange,state.get())!=RS_PLUGIN_OK)return RS_PLUGIN_DUPLICATE;*instance=state.release();return RS_PLUGIN_OK;}catch(...){return RS_PLUGIN_FAILED;}
}
extern "C" __declspec(dllexport) void RS_PLUGIN_CALL RuneSchemaPlugin_Shutdown(void* context)noexcept{if(auto* state=static_cast<Session*>(context)){try{std::lock_guard lock(state->Gate);state->Close();}catch(...){}delete state;}}
