#pragma once
#include "Core/CacheBundleIO.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <functional>
#include <mutex>
#include <optional>
#include <map>
#include <cstdio>

namespace PS::SaveBackups {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline constexpr size_t MaximumBytes=32*1024*1024,MaximumEntries=4096;
inline std::mutex ModsMutex;inline Json SessionMods=Json::object();
inline std::string Guid(std::string value){
    if(value.size()!=32||value.find_first_not_of("0123456789abcdefABCDEF")!=value.npos||value.find_first_not_of('0')==value.npos)return {};
    for(auto& c:value)if(c>='a'&&c<='f')c=char(c-'a'+'A');return value;
}
inline std::string Runtime(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=char(c-'A'+'a');if(value!="steam"&&value!="epic"&&value!="gamepass"&&value!="server")throw std::runtime_error("Unknown backup runtime");return value;}
inline std::string Now(){SYSTEMTIME time{};GetSystemTime(&time);char text[32]{};std::snprintf(text,sizeof(text),"%04u-%02u-%02uT%02u:%02u:%02uZ",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond);return text;}
inline Json Decode(const std::string& bytes){
    if(bytes.empty()||bytes.size()>MaximumBytes)throw std::runtime_error("Save backup exceeds its byte limit");
    if(bytes.size()>=2&&static_cast<unsigned char>(bytes[0])==0xff&&static_cast<unsigned char>(bytes[1])==0xfe){
        if(bytes.size()%2)throw std::runtime_error("Save backup has an invalid UTF-16 length");
        std::u16string value((bytes.size()-2)/2,u'\0');std::memcpy(value.data(),bytes.data()+2,bytes.size()-2);return Json::parse(value);
    }return Json::parse(bytes);
}
inline std::string Identity(const Json& value){
    if(!value.is_object()||!value.contains("GameProgress")||!value["GameProgress"].is_object())return {};
    std::string id;
    if(const auto meta=value.find("meta_data");meta!=value.end()&&meta->is_object()&&meta->contains("char_guid")){
        if(!(*meta)["char_guid"].is_string())return {};id=Guid((*meta)["char_guid"].get<std::string>());if(id.empty())return {};
    }
    if(value.contains("GUID")){if(!value["GUID"].is_string())return {};const auto legacy=Guid(value["GUID"].get<std::string>());if(!id.empty()&&legacy!=id)return {};id=legacy;}
    return id;
}
inline fs::path Directory(const fs::path& root,const std::string& runtime){return root/"core/backups"/Runtime(runtime)/"characters";}
inline fs::path File(const fs::path& directory,const std::string& guid,const std::string& hash){
    if(Guid(guid)!=guid||!CacheBundleIO::Digest(hash))throw std::runtime_error("Invalid save backup identity");
    const auto file=directory/guid/(hash+".json.bak");CacheBundleIO::Safe(file);return file;
}
inline fs::path Metadata(const fs::path& file){return fs::path(file.wstring()+L".metadata.json");}
struct Lock {
    HANDLE Handle=INVALID_HANDLE_VALUE;
    explicit Lock(const fs::path& directory){CacheBundleIO::Safe(directory);fs::create_directories(directory);const auto path=CacheBundleIO::NativePath(directory/"backup.lock");CacheBundleIO::Safe(path);
        Handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(Handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Save backups are busy; try again after the current operation");}
    ~Lock(){if(Handle!=INVALID_HANDLE_VALUE)CloseHandle(Handle);}Lock(const Lock&)=delete;Lock& operator=(const Lock&)=delete;
};
inline void ObserveMod(const std::string& id,const std::string& name,const std::string& version){
    if(id.empty()||id.size()>256||name.size()>1024||version.size()>256)return;std::lock_guard lock(ModsMutex);
    if(SessionMods.size()<MaximumEntries||SessionMods.contains(id))SessionMods[id]={{"Id",id},{"Name",name},{"Version",version}};
}
inline Json ActiveMods(){std::lock_guard lock(ModsMutex);Json rows=Json::array();for(const auto& row:SessionMods)rows.push_back(row);return rows;}
inline Json Context(Json context){
    if(!context.is_object())throw std::runtime_error("Invalid backup context");
    Json result=Json::object();
    if(context.contains("LastPlayedUtc")&&context["LastPlayedUtc"].is_string()&&context["LastPlayedUtc"].get_ref<const std::string&>().size()<=32)result["LastPlayedUtc"]=context["LastPlayedUtc"];
    if(context.contains("Mods")){
        const auto& mods=context["Mods"];bool valid=mods.is_array()&&mods.size()<=MaximumEntries;
        if(valid)for(const auto& mod:mods){if(!mod.is_object()){valid=false;break;}for(const auto* key:{"Id","Name","Version"})if(!mod.contains(key)||!mod[key].is_string()||mod[key].get_ref<const std::string&>().size()>1024){valid=false;break;}if(!valid)break;}
        // Optional annotations must never prevent a verified pre-cleanup backup.
        result["Mods"]=valid&&mods.dump().size()<=400*1024?mods:Json();
    }return result;
}
inline void Verify(const fs::path& file,std::string_view expected){
    CacheBundleIO::Safe(file);if(fs::file_size(file)!=expected.size())throw std::runtime_error("Whole-save file size changed");
    std::ifstream input(CacheBundleIO::NativePath(file),std::ios::binary);std::array<char,64*1024> block{};
    for(size_t offset=0;offset<expected.size();){const auto count=(std::min)(block.size(),expected.size()-offset);input.read(block.data(),count);
        if(!input||std::memcmp(block.data(),expected.data()+offset,count)!=0)throw std::runtime_error("Whole-save verification failed");offset+=count;}
    if(!input||input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Whole-save file changed during verification");
}
inline fs::path Store(const fs::path& directory,const std::string& bytes,const std::string& expectedGuid,const std::string& reason,Json context=Json::object()){
    const auto document=Decode(bytes);const auto guid=Identity(document);if(guid.empty()||guid!=Guid(expectedGuid))throw std::runtime_error("Backup is not the exact gameplay character");
    if(reason!="Before cleanup"&&reason!="Automatic"&&reason!="Before restore"&&reason!="Manual")throw std::runtime_error("Unknown backup reason");
    context=Context(std::move(context));
    Lock lock(directory);const auto hash=CacheBundleIO::Hash(bytes);const auto file=File(directory,guid,hash);
    if(!fs::exists(file))CacheBundleIO::Publish(file,bytes);
    Verify(file,bytes);
    Json metadata={{"Format","RuneSchemaSaveBackup"},{"Version",1},{"Guid",guid},{"SHA256",hash},{"Bytes",bytes.size()},{"CreatedUtc",Now()},{"Reasons",Json::array()}};
    if(fs::exists(Metadata(file))){const auto old=Json::parse(CacheBundleIO::Read(Metadata(file),512*1024));if(old.value("Guid",std::string{})!=guid||old.value("SHA256",std::string{})!=hash)throw std::runtime_error("Backup metadata identity changed");metadata=old;}
    auto& reasons=metadata["Reasons"];if(!reasons.is_array()||reasons.size()>8)throw std::runtime_error("Invalid backup reasons");
    if(std::find(reasons.begin(),reasons.end(),reason)==reasons.end())reasons.push_back(reason);
    for(const auto& [key,value]:context.items())if(key=="LastPlayedUtc"||(key=="Mods"&&!metadata.contains("Mods")))metadata[key]=value;
    if(const auto meta=document.find("meta_data");meta!=document.end()&&meta->is_object())if(const auto name=meta->find("char_name");name!=meta->end()&&name->is_string())metadata["Name"]=name->get<std::string>().substr(0,512);
    metadata["LastObservedUtc"]=Now();CacheBundleIO::Publish(Metadata(file),metadata.dump());return file;
}
inline std::string Read(const fs::path& directory,const std::string& guid,const std::string& hash){const auto bytes=CacheBundleIO::Read(File(directory,guid,hash),MaximumBytes);
    if(CacheBundleIO::Hash(bytes)!=hash||Identity(Decode(bytes))!=guid)throw std::runtime_error("Backup hash or character identity does not match; restore refused");return bytes;}
struct Entry {std::string Guid,Hash,Name="Unknown character",CreatedUtc,LastPlayedUtc;Json Mods=nullptr,Reasons=Json::array();std::uintmax_t Bytes=0;bool ValidMetadata=false;};
inline std::vector<Entry> List(const fs::path& directory){
    std::vector<Entry> rows;CacheBundleIO::Safe(directory);if(!fs::exists(directory))return rows;size_t examined=0,legacyBytes=0;
    for(const auto& owner:fs::directory_iterator(directory)){if(++examined>MaximumEntries*2)throw std::runtime_error("Save backup listing exceeds its entry limit");const auto guid=owner.path().filename().string();if(Guid(guid)!=guid)continue;CacheBundleIO::Safe(owner.path());if(!owner.is_directory())continue;
        for(const auto& file:fs::directory_iterator(owner.path())){if(++examined>MaximumEntries*4||rows.size()>=MaximumEntries)throw std::runtime_error("Save backup listing exceeds its entry limit");const auto name=file.path().filename().string();if(name.size()!=73||!name.ends_with(".json.bak"))continue;const auto hash=name.substr(0,64);if(!CacheBundleIO::Digest(hash))continue;
            CacheBundleIO::Safe(file.path());if(!file.is_regular_file())continue;Entry row;row.Guid=guid;row.Hash=hash;row.Bytes=file.file_size();if(!row.Bytes||row.Bytes>MaximumBytes)continue;
            try{if(fs::exists(Metadata(file.path()))){const auto meta=Json::parse(CacheBundleIO::Read(Metadata(file.path()),512*1024));if(meta.value("Guid",std::string{})!=guid||meta.value("SHA256",std::string{})!=hash)throw std::runtime_error("Backup metadata mismatch");
                row.Name=meta.value("Name",row.Name);row.CreatedUtc=meta.value("CreatedUtc",std::string{});row.LastPlayedUtc=meta.value("LastPlayedUtc",std::string{});row.Mods=meta.value("Mods",Json());row.Reasons=meta.value("Reasons",Json::array());row.ValidMetadata=true;
            }else if(legacyBytes+row.Bytes<=64*1024*1024){legacyBytes+=row.Bytes;const auto document=Decode(Read(directory,guid,hash));if(document.contains("meta_data"))row.Name=document["meta_data"].value("char_name",row.Name);}
            }catch(...){row.ValidMetadata=false;row.Mods=nullptr;row.Reasons=Json::array();}
            rows.push_back(std::move(row));
        }
    }std::stable_sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.CreatedUtc>b.CreatedUtc;});return rows;
}
inline Json Options(const fs::path& directory){const auto file=directory/"settings.json";CacheBundleIO::Safe(file);if(!fs::exists(file))return Json::object();auto value=Json::parse(CacheBundleIO::Read(file,4096));if(!value.is_object())throw std::runtime_error("Invalid save recovery settings");return value;}
inline bool Automatic(const fs::path& directory){return Options(directory).value("Automatic",false);}
inline void SetAutomatic(const fs::path& directory,bool enabled){Lock lock(directory);auto value=Options(directory);value["Automatic"]=enabled;CacheBundleIO::Publish(directory/"settings.json",value.dump());}
inline bool ExperimentalWorldRecovery(const fs::path& directory){return Options(directory).value("ExperimentalWorldRecovery",false);}
inline void SetWorldRecovery(const fs::path& directory,bool enabled){Lock lock(directory);auto value=Options(directory);value["ExperimentalWorldRecovery"]=enabled;CacheBundleIO::Publish(directory/"settings.json",value.dump());}
inline fs::path Pending(const fs::path& directory,const std::string& guid){if(Guid(guid)!=guid)throw std::runtime_error("Invalid restore character");return directory/"requests"/(guid+".pending.json");}
inline void CancelRequest(const fs::path& directory,const std::string& guid,const std::function<bool()>& stopped){
    if(!stopped||!stopped())throw std::runtime_error("Close Dragonwilds before changing a restore request");Lock lock(directory);
    const auto pending=Pending(directory,guid);const auto processing=fs::path(pending.wstring()+L".processing");
    const auto source=fs::exists(pending)?pending:processing;CacheBundleIO::Safe(source);if(!fs::exists(source))return;
    const auto cancelled=fs::path(source.wstring()+L".cancelled-"+std::to_wstring(GetTickCount64()));CacheBundleIO::Safe(cancelled);
    if(!stopped()||!MoveFileExW(source.c_str(),cancelled.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Restore request changed or could not be cancelled");
}
inline void QueueRestore(const fs::path& directory,const Entry& entry,const std::function<bool()>& stopped){
    if(!stopped||!stopped())throw std::runtime_error("Close Dragonwilds before scheduling a restore");(void)Read(directory,entry.Guid,entry.Hash);Lock lock(directory);
    if(!stopped())throw std::runtime_error("Dragonwilds started; restore was not scheduled");
    const auto pending=Pending(directory,entry.Guid);if(fs::exists(pending)||fs::exists(fs::path(pending.wstring()+L".processing")))throw std::runtime_error("This character already has a pending or review-required restore; cancel it first");
    CacheBundleIO::Publish(pending,Json{{"Format","RuneSchemaSaveRestore"},{"Guid",entry.Guid},{"SHA256",entry.Hash},{"RequestedUtc",Now()}}.dump());
}
inline bool ApplyQueued(const fs::path& directory,const std::string& current,const std::function<void(const std::string&)>& apply){
    const auto guid=Identity(Decode(current));if(guid.empty()||!fs::exists(Pending(directory,guid)))return false;
    const auto requested=CacheBundleIO::Read(Pending(directory,guid),4096);const auto request=Json::parse(requested);if(request.value("Format",std::string{})!="RuneSchemaSaveRestore"||request.value("Guid",std::string{})!=guid)throw std::runtime_error("Restore request identity does not match");
    const auto hash=request.at("SHA256").get<std::string>(),bytes=Read(directory,guid,hash);Store(directory,current,guid,"Before restore",{{"Mods",ActiveMods()}});
    Lock lock(directory);const auto pending=Pending(directory,guid),processing=fs::path(pending.wstring()+L".processing");CacheBundleIO::Safe(pending);CacheBundleIO::Safe(processing);
    if(CacheBundleIO::Read(pending,4096)!=requested)throw std::runtime_error("Restore request changed; no save was replaced");
    if(!MoveFileExW(pending.c_str(),processing.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Restore request is busy or needs review; no automatic retry");
    // A failed/crashed native application leaves a review marker, never a replayable restore.
    apply(Decode(bytes).dump());const auto receipt=fs::path(processing.wstring()+L".loaded");
    if(!MoveFileExW(processing.c_str(),receipt.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Restore loaded, but its receipt needs review");return true;
}
inline void RestoreFile(const fs::path& directory,const Entry& entry,const fs::path& nativeRoot,const fs::path& target,const std::string& expected,const std::function<bool()>& stopped){
    if(!stopped||!stopped())throw std::runtime_error("Close Dragonwilds before restoring a save");
    CacheBundleIO::Safe(nativeRoot);CacheBundleIO::Safe(target);
    if(fs::absolute(target.parent_path()).lexically_normal()!=fs::absolute(nativeRoot).lexically_normal()||target.extension()!=L".json"||!fs::is_regular_file(target))throw std::runtime_error("Select an existing native character save in its exact save folder");
    const auto before=CacheBundleIO::Read(target,MaximumBytes),bytes=Read(directory,entry.Guid,entry.Hash);
    if(Identity(Decode(before))!=entry.Guid||CacheBundleIO::Hash(before)!=expected)throw std::runtime_error("Current character changed or has another GUID; rescan before restoring");
    Store(directory,before,entry.Guid,"Before restore");Lock lock(directory);
    if(!stopped())throw std::runtime_error("The game started; restore was not applied");Verify(target,before);
    CacheBundleIO::Publish(target,bytes);Verify(target,bytes);
}
}
