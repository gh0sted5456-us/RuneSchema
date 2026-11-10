#pragma once
#include "Core/ModMenuGate.h"
#include "Core/ModMenuFiles.h"
#include "Runtime/NetworkStatus.h"
#include "Generator/ToolRequest.h"
#include "Utility/DiagnosticLog.h"
#include "LuaMadeSimple/LuaMadeSimple.hpp"
#include <deque>
#include <mutex>

namespace PS::HelpyModMenu {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline std::mutex Gate;inline fs::path ActiveRoot;inline ModMenuFiles::Scope Files;inline bool Installed=false,Active=false;
inline uint64_t Revision=0,Heartbeat=0;inline Json Snapshot=Json::object();inline std::deque<Json> Commands;
inline int Exchange(const RC::LuaMadeSimple::Lua& lua){
    try{if(!lua.is_string())throw std::runtime_error("Invalid Mod Menu bridge request.");const auto text=lua.get_string();if(text.size()>4*1024*1024)throw std::runtime_error("Mod Menu bridge message is too large.");const auto request=Json::parse(text,[](int depth,Json::parse_event_t,Json&){if(depth>32)throw std::runtime_error("Mod Menu bridge nesting is too deep.");return true;});const auto op=request.at("Op").get<std::string>();std::lock_guard lock(Gate);Json result;
        if(!Installed)result={{"Active",false}};
        else if(op=="catalog")result=Files.Catalog();
        else if(op=="next"){Heartbeat=GetTickCount64();result={{"Active",true}};const auto session=NetworkStatus::Current.Snapshot(SpawnToolRequests::Generation.load(),SpawnToolRequests::Available.load(),false);const auto role=session.value("Role",std::string{});result["HostAuthority"]=session.value("Fresh",false)&&(role=="standalone"||role=="listen-server"||role=="dedicated-server");if(!Commands.empty()){const auto blocked=ModMenuGate::Blocked(ActiveRoot);if(!blocked.empty()){Installed=Active=false;Commands.clear();result={{"Active",false},{"Reason",blocked}};}else{result["Command"]=std::move(Commands.front());Commands.pop_front();}}}
        else if(op=="log"){auto message=request.at("Text").get<std::string>();message.resize(std::min<size_t>(message.size(),2048));std::replace(message.begin(),message.end(),'\n',' ');std::replace(message.begin(),message.end(),'\r',' ');DiagnosticLog::WriteUtf8("[RuneSchema][Mod Menu 2] "+message);result={{"Accepted",true}};}
        else if(op=="publish"){auto value=request.at("Snapshot");if(!value.is_object()||!value.at("Pages").is_array()||value.at("Pages").size()>2048)throw std::runtime_error("Invalid Mod Menu snapshot.");Snapshot=std::move(value);Active=true;Heartbeat=GetTickCount64();++Revision;result={{"Accepted",true}};}
        else if(op=="stop"){Active=false;Installed=false;Commands.clear();++Revision;Snapshot={{"Status",request.value("Reason",std::string("Mod Menu stopped."))}};result={{"Accepted",true}};}
        else if(op=="read")result=Files.Read(request);
        else if(op=="write")result=Files.Write(request);
        else throw std::runtime_error("Unknown Mod Menu bridge operation.");lua.set_string(result.dump(-1,' ',false,Json::error_handler_t::replace));return 1;
    }catch(const std::exception& error){lua.set_string(Json{{"Error",error.what()}}.dump(-1,' ',false,Json::error_handler_t::replace));return 1;}
}
inline void Install(const fs::path& root,const RC::LuaMadeSimple::Lua& lua){
    std::lock_guard lock(Gate);if(Installed)return;ActiveRoot=root;const auto blocked=ModMenuGate::Blocked(ActiveRoot);if(!blocked.empty()){Snapshot={{"Status",blocked}};return;}Files.Open(root);lua.register_function("RuneSchemaModMenuExchange",Exchange);Installed=true;
}
inline Json Read(uint64_t known){std::lock_guard lock(Gate);const bool active=Installed&&Active&&GetTickCount64()-Heartbeat<5000;Json result={{"Available",active},{"Revision",Revision},{"Status",active?"Integrated Mod Menu is active.":Installed?"Waiting for Mod Menu's Lua adapter.":Snapshot.value("Status",std::string("Mod Menu is inactive; changes require a verified restart."))}};if(known!=Revision){result["Snapshot"]=Snapshot;}return result;}
inline Json Queue(const Json& request){std::lock_guard lock(Gate);const auto op=request.at("Op").get<std::string>();if(op=="stop"){Installed=Active=false;Commands.clear();++Revision;return {{"Accepted",true}};}if(!Installed||!Active||GetTickCount64()-Heartbeat>=5000)return {{"Accepted",false},{"Status","Mod Menu is inactive. Restart after disabling the separate mod."}};if(op!="refresh"&&op!="set"&&op!="button"&&op!="reset"&&op!="page"&&op!="template_apply")throw std::runtime_error("Unknown Mod Menu command.");if(Commands.size()>=32||request.dump().size()>(op=="template_apply"?512*1024:16384))throw std::runtime_error("Mod Menu command queue is full.");Commands.push_back(request);return {{"Accepted",true}};}
}
