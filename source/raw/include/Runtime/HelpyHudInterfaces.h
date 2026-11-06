#pragma once
#include "../../../shared/HudInterfaces.h"
#include "Core/UE4SSPackages.h"
#include "Core/ModManager.h"
#include "Core/ConfigFiles.h"
#include "Utility/DiagnosticLog.h"
#include "LuaMadeSimple/LuaMadeSimple.hpp"
#include <mutex>
#include <sstream>

namespace PS::HelpyHudInterfaces {
inline std::mutex Gate;inline HudInterfaces::Store State;inline bool Installed=false;
inline std::string Failure;
inline HudInterfaces::Json Read(uint64_t revision){std::lock_guard lock(Gate);HudInterfaces::Json result={{"Available",Installed},{"Revision",State.Revision}};if(revision!=State.Revision)result["Interfaces"]=State.Rows;if(!Failure.empty())result["Status"]=Failure;return result;}
inline HudInterfaces::Json Queue(const HudInterfaces::Json& request){std::lock_guard lock(Gate);try{State.Queue(request);return {{"Accepted",true}};}catch(const std::exception& error){return {{"Accepted",false},{"Status",error.what()}};}}
inline std::string Scoped(const RC::LuaMadeSimple::Lua& lua){const auto owner=std::string(lua.get_string(1)),id=std::string(lua.get_string(2));if(owner.size()>128||id.size()>64)throw std::runtime_error("Invalid HUD identity.");return owner+"/"+id;}
inline int Register(const RC::LuaMadeSimple::Lua& lua){
    try{const auto id=Scoped(lua),text=std::string(lua.get_string(3));if(text.size()>8192)throw std::runtime_error("Too many HUD callbacks.");std::set<std::string> controls;std::istringstream input(text);for(std::string field;std::getline(input,field);)if(!field.empty())controls.insert(field);std::lock_guard lock(Gate);State.Register(id,controls);lua.set_string("");}
    catch(const std::exception& error){lua.set_string(error.what());}return 1;
}
inline int Unregister(const RC::LuaMadeSimple::Lua& lua){try{const auto id=Scoped(lua);std::lock_guard lock(Gate);State.Unregister(id);lua.set_string("");}catch(const std::exception& error){lua.set_string(error.what());}return 1;}
inline int Set(const RC::LuaMadeSimple::Lua& lua){
    try{const auto id=Scoped(lua),field=std::string(lua.get_string(3)),text=std::string(lua.get_string(4));if(text.size()>2048)throw std::runtime_error("HUD value is too long.");std::lock_guard lock(Gate);auto& row=State.Find(id);const auto type=State.Control(row,field).at("Type").get<std::string>();
        const auto value=type=="slider"||type=="toggle"?HudInterfaces::Json::parse(text):type=="button"?HudInterfaces::Json{}:HudInterfaces::Json(text);State.Set(id,field,value);lua.set_string("");}
    catch(const std::exception& error){lua.set_string(error.what());}return 1;
}
inline int Next(const RC::LuaMadeSimple::Lua& lua){
    std::lock_guard lock(Gate);const auto command=State.Next();
    if(command.is_null()){for(int i=0;i<6;++i)lua.set_string("");return 6;}
    for(const auto* key:{"Owner","LocalId","Control","Type"})lua.set_string(command.at(key).get<std::string>());
    const auto& value=command.at("Value");lua.set_string(value.is_string()?value.get<std::string>():value.dump());lua.set_string(command.at("Id").get<std::string>());return 6;
}
inline int Error(const RC::LuaMadeSimple::Lua& lua){
    try{const auto id=Scoped(lua),message=std::string(lua.get_string(3).substr(0,512));{std::lock_guard lock(Gate);State.Error(id,message);}
        DiagnosticLog::WriteUtf8("[RuneSchema][warning] [RS-UI-001] HUD callback "+id+": "+message+"\n");lua.set_string("");}
    catch(const std::exception& error){lua.set_string(error.what());}return 1;
}
inline void Install(const std::filesystem::path& root,const RC::LuaMadeSimple::Lua& lua){
    std::lock_guard lock(Gate);if(Installed)return;
    try{const auto parents=ModManager::Read(root,true);for(const auto& row:parents.at("Rows"))if(row.value("Enabled",false)&&row.value("Present",false)){
        try{for(const auto& folder:UE4SSPackages::HudRoots(std::filesystem::u8path(row.at("Folder").get<std::string>()),row.at("Name").get<std::string>())){
            const auto file=folder/"hud.json";ModManager::Safe(file);if(!std::filesystem::is_regular_file(file))continue;
            try{const auto doc=HudInterfaces::Json::parse(ConfigFiles::Read(file,256*1024),[](int depth,HudInterfaces::Json::parse_event_t,HudInterfaces::Json&){if(depth>16)throw std::runtime_error("HUD declaration nesting is too deep.");return true;},true,true);State.Add(HudInterfaces::Parse(row.at("Name").get<std::string>(),doc));}
            catch(const std::exception& error){Failure="Some mod HUD declarations could not be loaded. Check the RuneSchema log.";DiagnosticLog::WriteUtf8("[RuneSchema][warning] [RS-UI-001] HUD declaration "+file.string()+": "+error.what()+"\n");}
        }}catch(const std::exception& error){Failure=error.what();}
    }}catch(const std::exception& error){Failure=error.what();}
    lua.register_function("RuneSchemaHUDRegister",Register);lua.register_function("RuneSchemaHUDUnregister",Unregister);lua.register_function("RuneSchemaHUDSet",Set);lua.register_function("RuneSchemaHUDNext",Next);lua.register_function("RuneSchemaHUDError",Error);Installed=true;
}
}
