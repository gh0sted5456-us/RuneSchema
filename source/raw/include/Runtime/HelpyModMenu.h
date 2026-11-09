#pragma once
#include "Core/ModMenuGate.h"
#include "Core/UE4SSPackages.h"
#include "Core/RuneSchemaDataRoot.h"
#include "Runtime/NetworkStatus.h"
#include "Generator/ToolRequest.h"
#include "Utility/DiagnosticLog.h"
#include "LuaMadeSimple/LuaMadeSimple.hpp"
#include <deque>
#include <mutex>

namespace PS::HelpyModMenu {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline std::mutex Gate;inline fs::path Root,ActiveRoot,UE4SSMods;inline bool Installed=false,Active=false;
inline uint64_t Revision=0,Heartbeat=0;inline Json Snapshot=Json::object();inline std::deque<Json> Commands;
inline std::map<std::string,fs::path> ReadRoots,WriteFiles;
inline std::string FoldPath(const fs::path& path){const auto key=fs::absolute(path).lexically_normal().generic_u8string();return ModManager::Order::Fold(std::string(key.begin(),key.end()));}
inline Json Catalog(){
    Json pages=Json::array();ReadRoots.clear();WriteFiles.clear();const auto add=[&](const fs::path& folder,const std::string& name){
        auto page=Json{{"Folder",name},{"Path",RuntimeModPath::Text(folder)}};
        try{
            fs::path schema;
            for(const auto* leaf:{"modmenu.json","modmenu.txt"}){const auto file=folder/leaf;if(fs::is_regular_file(file)){schema=file;break;}}
            if(schema.empty())return;
            ModManager::Safe(schema);
            const auto doc=Json::parse(ConfigFiles::Read(schema,512*1024),nullptr,true,true);const auto config=doc.value("config",Json::object());auto relative=config.value("file",config.value("format",std::string{})=="lua"?std::string("Scripts/main.lua"):std::string("config.txt"));
            if(relative.empty()||relative.find("..")!=relative.npos||relative.find(':')!=relative.npos||fs::u8path(relative).has_root_path()||std::any_of(relative.begin(),relative.end(),[](unsigned char c){return c<32||c==127;}))throw std::runtime_error("Config path leaves its mod folder.");const auto file=folder/fs::u8path(relative);ModManager::Safe(file);ReadRoots[FoldPath(folder)]=folder;WriteFiles[FoldPath(file)]=file;
        }catch(const std::exception& error){page["Error"]=error.what();}
        pages.push_back(std::move(page));
    };
    std::string warning;
    for(const auto& entry:fs::directory_iterator(UE4SSMods)){
        std::error_code error;
        if(!entry.is_directory(error))continue;
        const auto name=RuntimeModPath::Text(entry.path().filename());
        if(ModMenuGate::Identity(name)!="runeschema")add(entry.path(),name);
    }
    try{
        const auto parents=ModManager::Read(Root,true);
        for(const auto& row:parents.at("Rows"))if(row.value("Enabled",false)&&row.value("Present",false))try{
            for(const auto& package:UE4SSPackages::Discover(fs::u8path(row.at("Folder").get<std::string>()),row.at("Name").get<std::string>()))
                if(package.Enabled&&package.Native.empty())add(package.Root,package.Name);
        }catch(const std::exception& error){warning=error.what();}
    }catch(const std::exception& error){warning=error.what();}
    return {{"Pages",std::move(pages)},{"Warning",std::move(warning)}};
}
inline fs::path Allowed(const Json& request,bool writing){const auto path=fs::absolute(fs::u8path(request.at("Path").get<std::string>())).lexically_normal();const auto key=FoldPath(path);bool allowed=writing?WriteFiles.contains(key):key==FoldPath(UE4SSMods/"mods.txt")||std::any_of(ReadRoots.begin(),ReadRoots.end(),[&](const auto& entry){return key.starts_with(entry.first+"/");});if(!allowed)throw std::runtime_error("Mod Menu file is outside its registered mod scope.");ModManager::Safe(path);return path;}
inline int Exchange(const RC::LuaMadeSimple::Lua& lua){
    try{if(!lua.is_string())throw std::runtime_error("Invalid Mod Menu bridge request.");const auto text=lua.get_string();if(text.size()>4*1024*1024)throw std::runtime_error("Mod Menu bridge message is too large.");const auto request=Json::parse(text,[](int depth,Json::parse_event_t,Json&){if(depth>32)throw std::runtime_error("Mod Menu bridge nesting is too deep.");return true;});const auto op=request.at("Op").get<std::string>();std::lock_guard lock(Gate);Json result;
        if(!Installed)result={{"Active",false}};
        else if(op=="catalog")result=Catalog();
        else if(op=="next"){Heartbeat=GetTickCount64();result={{"Active",true}};const auto session=NetworkStatus::Current.Snapshot(SpawnToolRequests::Generation.load(),SpawnToolRequests::Available.load(),false);const auto role=session.value("Role",std::string{});result["HostAuthority"]=session.value("Fresh",false)&&(role=="standalone"||role=="listen-server"||role=="dedicated-server");if(!Commands.empty()){const auto blocked=ModMenuGate::Blocked(ActiveRoot);if(!blocked.empty()){Installed=Active=false;Commands.clear();result={{"Active",false},{"Reason",blocked}};}else{result["Command"]=std::move(Commands.front());Commands.pop_front();}}}
        else if(op=="log"){auto message=request.at("Text").get<std::string>();message.resize(std::min<size_t>(message.size(),2048));std::replace(message.begin(),message.end(),'\n',' ');std::replace(message.begin(),message.end(),'\r',' ');DiagnosticLog::WriteUtf8("[RuneSchema][Mod Menu 2] "+message);result={{"Accepted",true}};}
        else if(op=="publish"){auto value=request.at("Snapshot");if(!value.is_object()||!value.at("Pages").is_array()||value.at("Pages").size()>2048)throw std::runtime_error("Invalid Mod Menu snapshot.");Snapshot=std::move(value);Active=true;Heartbeat=GetTickCount64();++Revision;result={{"Accepted",true}};}
        else if(op=="stop"){Active=false;Installed=false;Commands.clear();++Revision;Snapshot={{"Status",request.value("Reason",std::string("Mod Menu stopped."))}};result={{"Accepted",true}};}
        else if(op=="read"){const auto file=Allowed(request,false);const bool exists=fs::exists(file);result={{"Exists",exists},{"Text",exists?ConfigFiles::Read(file,512*1024):std::string{}}};}
        else if(op=="write"){const auto file=Allowed(request,true);const auto after=request.at("Text").get<std::string>();if(after.size()>512*1024)throw std::runtime_error("Mod config is too large.");const DragonWilds::ModOrderState::Snapshot before{request.at("Exists").get<bool>(),request.at("Expected").get<std::string>()};if(before.Exists){const auto backup=RuneSchemaData::SharedDirectory("HELPY")/"mod-menu-backups"/(CacheBundleIO::Hash(FoldPath(file)+before.Contents)+".txt");CacheBundleIO::Safe(backup);if(!fs::exists(backup))ConfigFiles::Write(backup,before.Contents);}result={{"Saved",ModManager::Order::WriteExpected(file,before,after)}};}
        else throw std::runtime_error("Unknown Mod Menu bridge operation.");lua.set_string(result.dump(-1,' ',false,Json::error_handler_t::replace));return 1;
    }catch(const std::exception& error){lua.set_string(Json{{"Error",error.what()}}.dump(-1,' ',false,Json::error_handler_t::replace));return 1;}
}
inline void Install(const fs::path& root,const RC::LuaMadeSimple::Lua& lua){
    std::lock_guard lock(Gate);if(Installed)return;ActiveRoot=root;Root=RuntimeModPath::Resolve(root);const auto blocked=ModMenuGate::Blocked(ActiveRoot);if(!blocked.empty()){Snapshot={{"Status",blocked}};return;}UE4SSMods=RuntimeModPath::Resolve(root.parent_path());lua.register_function("RuneSchemaModMenuExchange",Exchange);Installed=true;
}
inline Json Read(uint64_t known){std::lock_guard lock(Gate);const bool active=Installed&&Active&&GetTickCount64()-Heartbeat<5000;Json result={{"Available",active},{"Revision",Revision},{"Status",active?"Integrated Mod Menu is active.":Installed?"Waiting for Mod Menu's Lua adapter.":Snapshot.value("Status",std::string("Mod Menu is inactive; changes require a verified restart."))}};if(known!=Revision){result["Snapshot"]=Snapshot;}return result;}
inline Json Queue(const Json& request){std::lock_guard lock(Gate);const auto op=request.at("Op").get<std::string>();if(op=="stop"){Installed=Active=false;Commands.clear();++Revision;return {{"Accepted",true}};}if(!Installed||!Active||GetTickCount64()-Heartbeat>=5000)return {{"Accepted",false},{"Status","Mod Menu is inactive. Restart after disabling the separate mod."}};if(op!="refresh"&&op!="set"&&op!="button"&&op!="reset"&&op!="page"&&op!="template_apply")throw std::runtime_error("Unknown Mod Menu command.");if(Commands.size()>=32||request.dump().size()>(op=="template_apply"?512*1024:16384))throw std::runtime_error("Mod Menu command queue is full.");Commands.push_back(request);return {{"Accepted",true}};}
}
