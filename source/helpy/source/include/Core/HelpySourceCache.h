#pragma once
#include "Core/HelpyCacheSources.h"
#include "Core/ContentSource.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <stdexcept>
#include <string_view>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <set>
#include <string>
#include <windows.h>
namespace PS::HelpySourceCache {
struct Snapshot {
    std::string Key;std::set<std::string> Files;
    bool Authoritative=false;uint64_t PlanRevision=0;
    std::map<std::string,std::string> SourceKeys;
    std::map<std::string,std::set<std::string>> Owners;
    std::map<std::string,std::filesystem::path> FilePaths; // Worker-local; never sent to a client or exported.
};
inline uint64_t Hash(std::string_view value,uint64_t seed=1469598103934665603ull){for(unsigned char c:value){seed^=c;seed*=1099511628211ull;}return seed;}
inline std::string Hex(uint64_t value){char text[17]{};std::snprintf(text,sizeof(text),"%016llx",static_cast<unsigned long long>(value));return text;}
inline std::filesystem::path NativePath(const std::filesystem::path& path){
    auto value=std::filesystem::absolute(path).lexically_normal().wstring();
    if(value.starts_with(L"\\\\?\\"))return value;
    if(value.starts_with(L"\\\\"))return L"\\\\?\\UNC\\"+value.substr(2);
    return L"\\\\?\\"+value;
}
inline Snapshot Inspect(const std::filesystem::path& root,const HelpyCacheSources::Plan& plan={}){
    const auto source=NativePath(root);std::map<std::string,std::string> records;size_t count=0;
    std::map<std::string,std::filesystem::path> filePaths;
    const auto activeOwners=HelpyCacheSources::ActiveOwners(plan);
    const auto binary=source.parent_path().parent_path().parent_path();
    const auto game=binary.parent_path().parent_path();
    const auto add=[&](const std::filesystem::directory_entry& entry){
        if(!entry.is_regular_file()||entry.is_symlink())return;
        const auto attr=GetFileAttributesW(entry.path().c_str());if(attr==INVALID_FILE_ATTRIBUTES||(attr&FILE_ATTRIBUTE_REPARSE_POINT))return;
        auto ext=entry.path().extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        const auto filename=entry.path().filename().string();
        const bool modList=filename=="enabled.txt"||filename=="mods.txt"||filename=="runeschema.txt";
        if(ext!=".json"&&ext!=".jsonc"&&ext!=".ini"&&ext!=".pak"&&ext!=".utoc"&&ext!=".ucas"&&ext!=".uplugin"&&ext!=".dll"&&ext!=".exe"&&ext!=".lua"&&!modList)return;
        // Game Pass can expose the same install via XboxGames and WindowsApps.
        // Compare logical game-relative names, not the caller's absolute alias.
        // The Win64/WinGDK lane remains part of every binary/mod record.
        auto name=entry.path().lexically_normal().lexically_relative(game).generic_string();
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        const auto owner=HelpyCacheSources::FileOwner(name);
        if(plan.Authoritative&&!owner.empty()&&!activeOwners.contains(owner))return;
        if(++count>100000)throw std::runtime_error("Source cache manifest exceeds its file bound");
        auto record=std::to_string(entry.file_size())+":"+std::to_string(entry.last_write_time().time_since_epoch().count());
        // Small authored files include content, so same-size edits cannot reuse metadata.
        if((ext==".json"||ext==".jsonc"||ext==".ini"||ext==".lua"||ext==".uplugin"||modList)&&entry.file_size()<=4*1024*1024){std::ifstream input(entry.path(),std::ios::binary);if(!input)throw std::runtime_error("Source file is not readable");uint64_t hash=1469598103934665603ull;char data[8192];while(input){input.read(data,sizeof(data));hash=Hash(std::string_view(data,static_cast<size_t>(input.gcount())),hash);}record+=":"+Hex(hash);}
        records[name]=record;
        filePaths[name]=entry.path();
    };
    const auto tree=[&](const std::filesystem::path& folder){if(!std::filesystem::exists(folder))return;
        for(auto it=std::filesystem::recursive_directory_iterator(folder);it!=std::filesystem::recursive_directory_iterator();++it){const auto& entry=*it;
            const auto attr=GetFileAttributesW(entry.path().c_str());if(attr==INVALID_FILE_ATTRIBUTES)throw std::runtime_error("Source path is unavailable: "+entry.path().lexically_relative(game).generic_string()+" (Windows error "+std::to_string(GetLastError())+")");
            if(entry.is_directory()){const auto name=entry.path().filename().string();
                const auto owner=HelpyCacheSources::FileOwner(entry.path().lexically_relative(game).generic_string()+"/");
                if((attr&FILE_ATTRIBUTE_REPARSE_POINT)||name=="runtime"||name=="cache"||name=="saved"||name=="logs"||name=="exports"||name=="settings"
                    ||(plan.Authoritative&&!owner.empty()&&!activeOwners.contains(owner)))it.disable_recursion_pending();continue;}add(entry);
        }
    };
    tree(source.parent_path()); // Enabled UE4SS mods, including RuneSchema content and plugins.
    for(const auto* name:{"settings.ini","settings.jsonc"})if(std::filesystem::is_regular_file(source/"settings"/name))add(std::filesystem::directory_entry(source/"settings"/name));
    if(std::filesystem::is_directory(binary))for(const auto& entry:std::filesystem::directory_iterator(binary))add(entry);
    tree(binary.parent_path().parent_path()/"Content"/"Paks");
    tree(game/"Plugins");tree(game/"GameFeatures");tree(game.parent_path()/"Engine"/"Plugins");
    Snapshot result;result.Authoritative=plan.Authoritative;result.PlanRevision=plan.Revision;
    result.Owners=plan.Owners;result.FilePaths=std::move(filePaths);
    uint64_t hash=Hash("helpy-cache-v4|"+binary.filename().generic_string());
    for(const auto& [name,record]:records){result.Files.insert(name);hash=Hash(name+"|"+record+"\n",hash);}result.Key=Hex(hash);
    if(plan.Authoritative){
        uint64_t base=Hash("helpy-icons-v4|"+binary.filename().generic_string());
        std::map<std::string,uint64_t> ownerHashes;
        for(const auto& [name,record]:records){const auto owner=HelpyCacheSources::FileOwner(name);
            if(!owner.empty())ownerHashes[owner]=Hash(name+"|"+record+"\n",ownerHashes.contains(owner)?ownerHashes[owner]:Hash(owner));
            else if(ContentSource::NativeContentFile(name)||name.ends_with(".exe"))base=Hash(name+"|"+record+"\n",base);
        }
        auto registered=plan.Registered;if(registered.contains("game"))registered.insert("engine");
        for(const auto& group:registered){uint64_t key=Hash(group,base);const auto owners=plan.Owners.find(group);
            if(owners!=plan.Owners.end())for(const auto& owner:owners->second){
                const auto found=ownerHashes.find(owner);key=Hash(owner+"|"+(found==ownerHashes.end()?"missing":Hex(found->second)),key);
            }
            result.SourceKeys.emplace(group,Hex(key));
        }
        for(const auto& [group,key]:result.SourceKeys)hash=Hash(group+"|"+key,hash);result.Key=Hex(hash);
    }
    return result;
}
inline Snapshot Current;
inline std::future<Snapshot> Worker;
inline std::chrono::steady_clock::time_point Next{};
inline uint64_t Revision=0,RemovalRevision=0;
inline std::string Error;
inline std::function<bool(nlohmann::json&)> RemoteSnapshot;
inline std::mutex PublishedMutex;
inline nlohmann::json Published={{"Ready",false}};
inline std::atomic<bool> Requested{false};
inline uint64_t InspectingRevision=0;
inline bool HexKey(const std::string& key){return key.size()==16&&key.find_first_not_of("0123456789abcdef")==key.npos;}
inline nlohmann::json ForClient(){Requested=true;std::lock_guard lock(PublishedMutex);auto result=Published;
    if(result.value("PlanRevision",uint64_t{})!=HelpyCacheSources::Snapshot().Revision)result["Ready"]=false;return result;}
inline nlohmann::json Document(const Snapshot& value){return {{"Version",4},{"Ready",!value.Key.empty()&&(!value.Authoritative||!value.SourceKeys.empty())},
    {"Authoritative",value.Authoritative},{"PlanRevision",value.PlanRevision},{"Key",value.Key},{"Sources",value.SourceKeys},{"Owners",value.Owners}};}
inline bool AcceptRemote(const nlohmann::json& value){
    if(!value.is_object()||!value.value("Ready",false))return false;
    if(value.value("Version",0)!=4||!value.value("Authoritative",false))throw std::runtime_error("Icon source manifest requires a matching RuneSchema core");
    Snapshot next;next.Key=value.at("Key").get<std::string>();next.Authoritative=true;next.PlanRevision=value.at("PlanRevision").get<uint64_t>();
    if(!HexKey(next.Key)||!value.at("Sources").is_object()||value.at("Sources").size()>512)throw std::runtime_error("Icon source manifest is invalid");
    for(const auto& [group,key]:value.at("Sources").items()){
        if(HelpyCacheSources::Source("/"+group+"/asset.asset")!=group||!key.is_string()||!HexKey(key.get<std::string>()))throw std::runtime_error("Invalid registered icon source");
        next.SourceKeys.emplace(group,key.get<std::string>());
    }
    if(value.contains("Owners")){
        if(!value["Owners"].is_object()||value["Owners"].size()>512)throw std::runtime_error("Invalid icon source ownership");
        for(const auto& [group,owners]:value["Owners"].items()){
            if(HelpyCacheSources::Source("/"+group+"/asset.asset")!=group||!owners.is_array()||owners.size()>256)throw std::runtime_error("Invalid icon source ownership");
            for(const auto& owner:owners){if(!owner.is_string())throw std::runtime_error("Invalid icon source owner");const auto name=owner.get<std::string>();
                if(name.empty()||name.size()>128||name=="."||name==".."||name.find_first_of("/\\\r\n\t:")!=name.npos)throw std::runtime_error("Invalid icon source owner");next.Owners[group].insert(name);}
        }
    }
    if(next.Key==Current.Key)return false;
    bool removed=false;for(const auto& [group,key]:Current.SourceKeys)if(!next.SourceKeys.contains(group)){removed=true;break;}
    Current=std::move(next);++Revision;if(removed)RemovalRevision=Revision;return true;
}
inline std::filesystem::path Directory(bool liveOnly);
inline void Persist(const Snapshot& value) noexcept {
    try {if(!value.Authoritative)return;const auto folder=Directory(false);std::filesystem::create_directories(folder);
        const auto target=folder/"sources.json",temporary=folder/("sources-"+std::to_string(GetCurrentProcessId())+".tmp");
        {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);file<<Document(value).dump(2);file.close();if(!file)throw std::runtime_error("Icon source manifest write failed");}
        if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::error_code ec;std::filesystem::remove(temporary,ec);}
    }catch(...) {/* Disk cache failures must not fail the game or register assets. */}
}
// Called only by the owning UI/game thread. The worker only reads files and
// returns a value; it never accesses Unreal, ImGui, or mutable cache state.
inline bool Poll(const std::filesystem::path& root){
    bool changed=false;const auto now=std::chrono::steady_clock::now();
    if(RemoteSnapshot){if(now<Next)return false;Next=now+std::chrono::seconds(2);try{nlohmann::json value;
        if(!RemoteSnapshot(value))return false;changed=AcceptRemote(value);Error.clear();return changed;
    }catch(const std::exception& error){Error=error.what();return false;}}
    const auto plan=HelpyCacheSources::Snapshot();
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{
        auto result=Worker.get();if(result.PlanRevision!=plan.Revision){Next={};return false;}
        if(result.Key!=Current.Key){bool removed=false;for(const auto& path:Current.Files)if(!result.Files.contains(path)){removed=true;break;}
            Current=std::move(result);++Revision;if(removed)RemovalRevision=Revision;changed=true;}Error.clear();
        {std::lock_guard lock(PublishedMutex);Published=Document(Current);}
    }catch(const std::exception& error){Error=error.what();}Next=now+std::chrono::seconds(5);}
    if(!Worker.valid()&&(now>=Next||InspectingRevision!=plan.Revision)){InspectingRevision=plan.Revision;
        Worker=std::async(std::launch::async,[root,plan]{auto value=Inspect(root,plan);Persist(value);return value;});}return changed;
}
inline bool Ready(){return !Current.Key.empty()&&Error.empty()&&(!Current.Authoritative||!Current.SourceKeys.empty());}
// Call after the UI thread stops, before unloading the DLL. A std::async
// future must not outlive plugin shutdown and wait from a static destructor
// under Windows' loader lock.
inline void Shutdown() noexcept {
    if(Worker.valid())try{(void)Worker.get();}catch(...){}
    Next={};
}
inline std::string Fingerprint(const std::string& icon){if(!Current.Authoritative)return Current.Key;
    const auto found=Current.SourceKeys.find(HelpyCacheSources::Source(icon));return found==Current.SourceKeys.end()?std::string{}:found->second;}
inline std::string Key(const std::string& icon){return Hex(Hash(Fingerprint(icon)+"|"+icon));}
inline std::filesystem::path Directory(bool liveOnly){
    wchar_t appdata[32768];const auto size=GetEnvironmentVariableW(L"LOCALAPPDATA",appdata,32768);if(!size||size>=32768)throw std::runtime_error("LOCALAPPDATA is unavailable");
    return std::filesystem::path(appdata)/"RuneSchema"/"HELPY"/"runtime"/"saved"/"cache"/(liveOnly?"item-icons-live":"item-icons");
}
inline std::filesystem::path IconDirectory(const std::string& icon,bool liveOnly){
    const auto fingerprint=Fingerprint(icon);if(fingerprint.empty())return {};
    if(!Current.Authoritative)return Directory(liveOnly);
    const auto source=HelpyCacheSources::Source(icon);return Directory(liveOnly)/(HelpyCacheSources::Folder(source)+"-"+Hex(Hash(source)))/fingerprint;
}
}
