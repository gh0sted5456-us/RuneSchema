#pragma once
#include "Core/ModManager.h"
#include "Core/UE4SSPackages.h"

// Read-only inventory. File configuration is not evidence that a mod loaded.
namespace PS::UE4SSInventory {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline Json Metadata(const fs::path& root){
    Json result=Json::object();
    if(fs::is_regular_file(root/"ID.txt"))try{result=ModManager::Metadata(root);}catch(const std::exception& error){result["issue"]=std::string(error.what()).substr(0,512);}
    for(const auto* filename:{"modmenu.json","modmenu.txt","mod.json"}){
        const auto file=root/filename;if(!fs::is_regular_file(file))continue;
        try{ModManager::Safe(file);const auto doc=Json::parse(ConfigFiles::Read(file,512*1024),[](int depth,Json::parse_event_t,Json&){if(depth>32)throw std::runtime_error("UE4SS metadata nesting exceeds its limit");return true;},true,true);
            if(!doc.is_object())throw std::runtime_error("UE4SS metadata must be a JSON object");
            for(const auto* key:{"name","author","description","version","website"}){
                const auto field=doc.find(key);if(field==doc.end())continue;
                if(field->is_string())result[key]=field->get<std::string>().substr(0,std::string_view(key)=="description"?1024:256);
                else if(std::string_view(key)=="version"&&field->is_number())result[key]=field->dump().substr(0,64);
            }
            result["source"]=filename;break;
        }catch(const std::exception& error){result["issue"]=std::string(error.what()).substr(0,512);}
    }return result;
}
inline Json Read(const fs::path& root){
    ModManager::Safe(root);
    if(!ModFolderLayout::EqualsInsensitive(root.filename().string(),"RuneSchema")||!ModFolderLayout::EqualsInsensitive(root.parent_path().filename().string(),"Mods"))throw std::runtime_error("Confirm the assigned RuneSchema installation before viewing UE4SS mods");
    const auto mods=root.parent_path();ModManager::Safe(mods);if(!fs::is_directory(mods))throw std::runtime_error("This runtime's UE4SS Mods directory is unavailable");
    Json rows=Json::array();std::map<std::string,bool> configured;
    const auto order=mods/"mods.txt";
    if(fs::is_regular_file(order)){ModManager::Safe(order);std::istringstream input(ConfigFiles::Read(order,1024*1024));std::string line;
        while(std::getline(input,line)){line=line.substr(0,line.find(';'));line=line.substr(0,line.find('#'));const auto split=line.find(':');if(split==line.npos)continue;
            const auto name=ModIdentity::Trim(line.substr(0,split)),value=ModIdentity::Trim(line.substr(split+1));if(UE4SSPackages::NameSafe(name)&&(value=="0"||value=="1"))configured[ModFolderLayout::FoldAscii(name)]=value=="1";}
    }
    const auto append=[&](const UE4SSPackages::Package& package,bool managed,bool enabled,const Json& parent){
        if(package.Main.empty()&&package.Native.empty())return;
        if(rows.size()>=2048)throw std::runtime_error("UE4SS inventory exceeds 2048 entries");
        auto metadata=Metadata(package.Root);
        if(managed&&metadata.value("version",std::string{}).empty()&&!parent.value("version",std::string{}).empty()){metadata["version"]=parent.at("version");metadata["source"]="RuneSchema parent metadata";}
        const auto version=metadata.value("version",std::string{});
        rows.push_back({{"Name",package.Name},{"Owner",managed?package.Owner:std::string{}},{"Folder",package.Root.string()},{"Managed",managed},{"ConfiguredEnabled",enabled},
            {"NativeUnsupported",managed&&!package.Native.empty()},{"Kind",package.Native.empty()?"Lua":package.Main.empty()?"Native":"Lua + native"},{"Version",version},{"Metadata",std::move(metadata)}});
    };
    size_t entries=0;
    for(const auto& entry:fs::directory_iterator(mods)){
        if(++entries>1024)throw std::runtime_error("UE4SS Mods directory exceeds 1024 entries");
        if(!entry.is_directory()||ModFolderLayout::EqualsInsensitive(entry.path().filename().string(),"RuneSchema"))continue;
        try{const auto name=entry.path().filename().string();const auto package=UE4SSPackages::Inspect(entry.path(),name,name);
            const auto flag=entry.path()/"enabled.txt";ModManager::Safe(flag);const bool enabled=fs::is_regular_file(flag)||configured[ModFolderLayout::FoldAscii(name)];append(package,false,enabled,Json::object());
        }catch(const std::exception& error){rows.push_back({{"Name",entry.path().filename().string()},{"Owner",""},{"Folder",entry.path().string()},{"Managed",false},{"ConfiguredEnabled",false},{"ConfiguredKnown",false},{"NativeUnsupported",false},{"Kind","Not inspected"},{"Version",""},{"Metadata",{{"issue",std::string(error.what()).substr(0,512)}}}});}
    }
    Json owners;{std::lock_guard lock(ModManager::Gate);owners=ModManager::Read(root,true);}
    for(const auto& owner:owners.at("Rows"))if(owner.value("Present",false)){
        try{for(const auto& package:UE4SSPackages::Discover(fs::path(owner.at("Folder").get<std::string>()),owner.at("Name").get<std::string>()))append(package,true,owner.value("Enabled",false),owner.at("Metadata"));}
        catch(const std::exception& error){if(rows.size()>=2048)throw;rows.push_back({{"Name",owner.at("Name")},{"Owner",owner.at("Name")},{"Folder",owner.at("Folder")},{"Managed",true},{"ConfiguredEnabled",owner.at("Enabled")},{"NativeUnsupported",false},{"Kind","Layout unavailable"},{"Version",""},{"Metadata",{{"issue",std::string(error.what()).substr(0,512)}}}});}
    }
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return std::pair{a.at("Managed").template get<bool>(),ModFolderLayout::FoldAscii(a.at("Name").template get<std::string>())}<std::pair{b.at("Managed").template get<bool>(),ModFolderLayout::FoldAscii(b.at("Name").template get<std::string>())};});
    return {{"Rows",std::move(rows)},{"Root",root.string()}};
}
}
