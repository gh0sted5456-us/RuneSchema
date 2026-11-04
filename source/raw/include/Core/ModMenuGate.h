#pragma once
#include "../../../shared/ModMenu2Key.h"
#include "../../../shared/ModMenu2Settings.h"
#include "Core/ModManager.h"
#include "Core/RuntimeModPath.h"
#include <sstream>

namespace PS::ModMenuGate {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline std::string Identity(std::string name){name=ModManager::Order::Fold(name);std::erase_if(name,[](char c){return c==' '||c=='_'||c=='-';});return name;}
inline bool ExternalEnabled(const fs::path& root){
    const auto mods=root.parent_path();ModManager::Safe(mods);if(!fs::is_directory(mods))throw std::runtime_error("UE4SS mod directory is unavailable.");
    std::map<std::string,bool> flags;const auto list=mods/"mods.txt";ModManager::Safe(list);
    if(fs::is_regular_file(list)){std::istringstream input(ConfigFiles::Read(list,1024*1024));std::string line;while(std::getline(input,line)){line=line.substr(0,line.find_first_of(";#"));const auto split=line.find(':');if(split==line.npos)continue;auto name=ModIdentity::Trim(line.substr(0,split));if(name.starts_with("\xef\xbb\xbf"))name.erase(0,3);const auto value=ModIdentity::Trim(line.substr(split+1));if(value=="0"||value=="1")flags[ModManager::Order::Fold(name)]=value=="1";}}
    size_t count=0;for(const auto& entry:fs::directory_iterator(mods)){if(++count>4096)throw std::runtime_error("UE4SS mod directory needs inspection.");ModManager::Safe(entry.path());if(!entry.is_directory())continue;const auto folder=entry.path().filename().string();auto id=Identity(folder);bool menu=id=="modmenu"||id=="modmenu2";
        if(!menu)for(const auto* name:{"modmenu.json","modmenu.txt"}){const auto file=entry.path()/name;ModManager::Safe(file);if(!fs::is_regular_file(file))continue;try{const auto doc=Json::parse(ConfigFiles::Read(file,512*1024),nullptr,true,true);id=Identity(doc.value("id",std::string{}));menu=id=="modmenu"||id=="modmenu2"||Identity(doc.value("name",std::string{}))=="modmenu";}catch(...){}break;}
        if(menu&&(fs::exists(entry.path()/"enabled.txt")||flags[ModManager::Order::Fold(folder)]))return true;
    }return false;
}
inline std::string Blocked(const fs::path& activeRoot){try{const auto root=RuntimeModPath::Resolve(activeRoot);if(!ModMenu2Key::Present(root))return "Mod Menu 2 is locked; its activation key is absent or invalid.";if(!ModMenu2Settings::Read(root).Enabled)return "Integrated Mod Menu is disabled.";if(ExternalEnabled(root))return "Separate Mod Menu is enabled. Disable or remove it, then restart the game.";return {};}catch(const std::exception& error){return std::string("Mod Menu activation could not be verified: ")+error.what();}}
}
