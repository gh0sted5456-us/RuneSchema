#pragma once
#include "Core/HelpyPersonalization.h"
#include <objbase.h>
#include <chrono>
#include <array>
#include <future>
#include <map>
namespace PS::RuneSchemaProfile {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline std::mutex Gate;
inline fs::path Directory,Legacy;
inline Json Document=Json::object();
inline std::string Baseline,Status;
inline std::atomic<bool> Initialized=false,Ready=false;
inline std::atomic<uint64_t> Revision=0;
inline std::future<std::string> Migration;
inline std::chrono::steady_clock::time_point NextObserve{};
struct SharedProfile{Json Public;std::chrono::steady_clock::time_point Expires;};
inline std::map<std::string,SharedProfile> Shared;
inline std::string Guid(){GUID id{};if(FAILED(CoCreateGuid(&id)))throw std::runtime_error("Cannot create a profile identity");char text[33]{};std::snprintf(text,sizeof(text),"%08lx%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",id.Data1,id.Data2,id.Data3,id.Data4[0],id.Data4[1],id.Data4[2],id.Data4[3],id.Data4[4],id.Data4[5],id.Data4[6],id.Data4[7]);return text;}
inline bool Link(const std::string& key,const std::string& value){
    if(value.empty())return true;if(!HelpyPersonalization::Text(value,256))return false;
    if(key=="Discord")return value.size()<=128;
    const std::string prefix=key=="Steam"?"https://steamcommunity.com/":key=="Nexus"?"https://www.nexusmods.com/users/":"";
    if(prefix.empty()||!value.starts_with(prefix))return false;auto tail=value.substr(prefix.size());if(!tail.empty()&&tail.back()=='/')tail.pop_back();
    if(key=="Steam"){const bool numeric=tail.starts_with("profiles/");if(!numeric&&!tail.starts_with("id/"))return false;tail=tail.substr(numeric?9:3);if(numeric)return !tail.empty()&&tail.find_first_not_of("0123456789")==tail.npos;}
    return !tail.empty()&&tail.size()<=128&&tail.find("..") == tail.npos&&tail.find_first_not_of(key=="Nexus"?"0123456789":"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==tail.npos;
}
inline bool Platform(const std::string& value){return value=="Steam"||value=="Epic"||value=="Epic Games"||value=="Game Pass"||value=="Xbox"||value=="PS5"||value=="Switch 2"||value=="Jagex";}
inline Json Validate(const Json& doc){
    if(!doc.is_object()||(doc.size()!=6&&(doc.size()!=7||!doc.contains("PlatformAccounts")))||doc.at("Version")!=1||!doc.at("ProfileId").is_string()||doc.at("ProfileId").get<std::string>().empty()||HelpyPersonalization::Guid(doc.at("ProfileId").get<std::string>())!=doc.at("ProfileId").get<std::string>())throw std::runtime_error("Invalid RuneSchema profile identity");
    const auto name=doc.at("ProfileName").get<std::string>();if(!HelpyPersonalization::Text(name,64)||name!=HelpyPersonalization::Trim(name))throw std::runtime_error("Profile name must use 1–64 bytes of visible text");
    const auto& social=doc.at("Social");if(!social.is_object()||social.size()>3)throw std::runtime_error("Invalid profile links");for(const auto& [key,value]:social.items())if((key!="Steam"&&key!="Nexus"&&key!="Discord")||!value.is_string()||!Link(key,value.get<std::string>()))throw std::runtime_error("Use a Steam/Nexus profile link or Discord username");
    const auto platforms=[](const Json& rows){if(!rows.is_array()||rows.size()>8)throw std::runtime_error("Invalid played platforms");std::set<std::string> seen;for(const auto& value:rows)if(!value.is_string()||!Platform(value.get<std::string>())||!seen.insert(value.get<std::string>()).second)throw std::runtime_error("Invalid played platform");};platforms(doc.at("Platforms"));
    const auto& characters=doc.at("Characters");if(!characters.is_object()||characters.size()>256)throw std::runtime_error("Profile character limit reached");
    for(const auto& [guid,row]:characters.items())if(guid.empty()||HelpyPersonalization::Guid(guid)!=guid||!row.is_object()||row.size()!=3||!HelpyPersonalization::Text(row.at("Name").get<std::string>(),128)||!row.at("LastSeen").is_number_unsigned())throw std::runtime_error("Invalid linked character");else platforms(row.at("Platforms"));
    if(doc.contains("PlatformAccounts")){
        const auto& accounts=doc.at("PlatformAccounts");if(!accounts.is_object()||accounts.size()>8)throw std::runtime_error("Invalid platform accounts");
        for(const auto& [platform,account]:accounts.items())if(!Platform(platform)||!account.is_string()||account.get_ref<const std::string&>().empty()||!HelpyPersonalization::Text(account.get<std::string>(),128))throw std::runtime_error("Invalid platform account name");
    }
    return doc;
}
struct FileLock{HANDLE Handle{};explicit FileLock(const fs::path& file){uint64_t hash=1469598103934665603ull;for(unsigned char c:HelpyPersonalization::Fold(fs::absolute(file).generic_string())){hash^=c;hash*=1099511628211ull;}Handle=CreateMutexW(nullptr,FALSE,(L"Local\\RuneSchema-Profile-"+std::to_wstring(hash)).c_str());if(!Handle)throw std::runtime_error("Profile lock unavailable");const auto result=WaitForSingleObject(Handle,1000);if(result!=WAIT_OBJECT_0&&result!=WAIT_ABANDONED){CloseHandle(Handle);Handle=nullptr;throw std::runtime_error("Profile is busy");}}~FileLock(){if(Handle){ReleaseMutex(Handle);CloseHandle(Handle);}}};
inline void CopyNew(const fs::path& from,const fs::path& to,size_t maximum){
    HelpyPersonalization::Safe(from);HelpyPersonalization::Safe(to);if(!fs::exists(from)||fs::exists(to))return;
    if(!fs::is_regular_file(from)||fs::file_size(from)>maximum)throw std::runtime_error("Legacy profile data exceeds its safe bound");
    fs::create_directories(to.parent_path());const auto staging=to.parent_path()/(".profile-copy-"+Guid());
    if(!fs::create_directory(staging))throw std::runtime_error("Cannot reserve profile migration copy");const auto temporary=staging/"copy.tmp";
    const auto cleanup=[&]{std::error_code error;fs::remove(temporary,error);fs::remove(staging,error);};
    try{if(!CopyFileW(from.c_str(),temporary.c_str(),TRUE)||ConfigFiles::Read(from,maximum)!=ConfigFiles::Read(temporary,maximum))throw std::runtime_error("Profile copy did not verify; original retained");
        if(!MoveFileExW(temporary.c_str(),to.c_str(),MOVEFILE_WRITE_THROUGH)&&GetLastError()!=ERROR_FILE_EXISTS&&GetLastError()!=ERROR_ALREADY_EXISTS)throw std::runtime_error("Cannot publish profile copy; original retained");
    }catch(...){cleanup();throw;}cleanup();
}
inline bool Load(const fs::path& legacy){
    std::lock_guard lock(Gate);Initialized=true;Ready=false;Legacy=legacy;Directory=legacy.parent_path()/"runeschema.profile";
    try{HelpyPersonalization::Safe(Directory);FileLock guard(Directory/"profile.json");const auto file=Directory/"profile.json";
        const bool exists=fs::exists(file);auto text=exists?ConfigFiles::Read(file,1024*1024):std::string{};
        auto doc=exists?Validate(Json::parse(text)):Json{{"Version",1},{"ProfileId",Guid()},{"ProfileName",""},{"Social",Json::object()},{"Platforms",Json::array()},{"Characters",Json::object()}};
        if(!fs::exists(Directory/"labels.json")&&fs::exists(legacy/"personalization.json"))(void)HelpyPersonalization::Validate(Json::parse(ConfigFiles::Read(legacy/"personalization.json",4*1024*1024)));
        CopyNew(legacy/"personalization.json",Directory/"labels.json",4*1024*1024);
        CopyNew(legacy/"runtime/saved/player-portraits.json",Directory/"portraits.json",1024*1024);
        if(!exists){text=doc.dump(2);ConfigFiles::Write(file,text);}Document=std::move(doc);Baseline=text;Status.clear();Ready=true;++Revision;
        if(!Migration.valid()){const auto root=Directory;Migration=std::async(std::launch::async,[legacy,root]{try{
            for(const auto& entry:std::array{std::pair{legacy/"runtime/saved/portraits",root/"portraits"},std::pair{legacy/"runtime/Helpy/kits",root/"loadouts"}}){
                HelpyPersonalization::Safe(entry.first);if(!fs::exists(entry.first))continue;size_t count=0;
                for(const auto& item:fs::directory_iterator(entry.first)){if(++count>256)throw std::runtime_error("Legacy profile folder exceeds 256 entries; originals retained");if(item.is_regular_file())CopyNew(item.path(),entry.second/item.path().filename(),entry.second.filename()=="portraits"?1024*1024:128*1024);}
            }return std::string{};
        }catch(const std::exception& e){return std::string(e.what());}});}return true;
    }catch(const std::exception& e){Status=e.what();return false;}
}
inline Json Read(){std::lock_guard lock(Gate);return Document;}
inline std::string Id(){std::lock_guard lock(Gate);return Ready?Document.value("ProfileId",std::string{}):std::string{};}
inline fs::path Data(const char* name){std::lock_guard lock(Gate);return Ready?Directory/name:Legacy/name;}
inline bool CommitLocked(const Json& next){
    try{if(!Ready)throw std::runtime_error("Profile is unavailable; original data retained");auto value=Validate(next);if(value==Document)return true;
        if(value.at("ProfileId")!=Document.at("ProfileId")||(!Document.at("ProfileName").get<std::string>().empty()&&value.at("ProfileName")!=Document.at("ProfileName")))throw std::runtime_error("The profile identity and established profile name cannot change");
        FileLock guard(Directory/"profile.json");HelpyPersonalization::Safe(Directory/"profile.json");if(ConfigFiles::Read(Directory/"profile.json",1024*1024)!=Baseline)throw std::runtime_error("Profile changed in another game/window; reload before saving");
        const auto text=value.dump(2);if(text.size()>1024*1024)throw std::runtime_error("Profile exceeds 1 MiB");ConfigFiles::Write(Directory/"profile.json",text);Document=std::move(value);Baseline=text;Status="Saved";++Revision;return true;
    }catch(const std::exception& e){Status=e.what();return false;}
}
inline bool Save(const std::string& name,const Json& social){std::lock_guard lock(Gate);auto next=Document;if(next.empty())return false;const auto trimmed=HelpyPersonalization::Trim(name);if(trimmed.empty()){Status="Choose a profile name";return false;}next["ProfileName"]=trimmed;next["Social"]=social;return CommitLocked(next);}
inline void Observe(const Json& row){
    if(!row.is_object()||!row.contains("IsLocal")||row["IsLocal"]!=true||!row.contains("Guid")||!row["Guid"].is_string()||!row.contains("Name")||!row["Name"].is_string()||(row.contains("Platform")&&!row["Platform"].is_string()))return;const auto guid=HelpyPersonalization::Guid(row.value("Guid",std::string{}));const auto name=row.value("Name",std::string{}),platform=row.value("Platform",std::string{});
    if(guid.empty()||!HelpyPersonalization::Text(name,128))return;std::lock_guard lock(Gate);if(!Ready||std::chrono::steady_clock::now()<NextObserve)return;
    const auto account=row.contains("PlatformAccountName")&&row["PlatformAccountName"].is_string()?row["PlatformAccountName"].get<std::string>():std::string{};
    const bool accountKnown=Platform(platform)&&!account.empty()&&HelpyPersonalization::Text(account,128);
    const bool accountChanged=accountKnown&&Document.value("PlatformAccounts",Json::object()).value(platform,std::string{})!=account;
    const auto found=Document["Characters"].find(guid);if(!accountChanged&&found!=Document["Characters"].end()&&found->at("Name")==name&&(!Platform(platform)||std::find(found->at("Platforms").begin(),found->at("Platforms").end(),platform)!=found->at("Platforms").end()))return;
    auto next=Document;
    if(accountChanged){if(!next.contains("PlatformAccounts"))next["PlatformAccounts"]=Json::object();next["PlatformAccounts"][platform]=account;}
    auto character=next["Characters"].value(guid,Json{{"Name",name},{"Platforms",Json::array()},{"LastSeen",uint64_t(0)}});const auto old=character;
    character["Name"]=name;if(Platform(platform)){if(std::find(character["Platforms"].begin(),character["Platforms"].end(),platform)==character["Platforms"].end())character["Platforms"].push_back(platform);if(std::find(next["Platforms"].begin(),next["Platforms"].end(),platform)==next["Platforms"].end())next["Platforms"].push_back(platform);}
    if(!accountChanged&&character==old&&next["Characters"].contains(guid))return;
    character["LastSeen"]=uint64_t(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());next["Characters"][guid]=character;if(!CommitLocked(next))NextObserve=std::chrono::steady_clock::now()+std::chrono::seconds(5);
}
inline Json ValidatePublic(const Json& value){
    if(value.is_null())return value;
    if(!value.is_object()||value.size()!=4||value.dump().size()>1600)throw std::runtime_error("Invalid public profile");
    const Json full={{"Version",1},{"ProfileId",value.at("ProfileId")},{"ProfileName",value.at("ProfileName")},{"Social",value.at("Social")},{"Platforms",value.at("Platforms")},{"Characters",Json::object()}};
    Validate(full);if(full.at("ProfileName").get<std::string>().empty())throw std::runtime_error("A shared profile needs a name");return value;
}
inline Json PublicLocked(){return Ready&&!Document.at("ProfileName").get<std::string>().empty()?Json{{"ProfileId",Document.at("ProfileId")},{"ProfileName",Document.at("ProfileName")},{"Social",Document.at("Social")},{"Platforms",Document.at("Platforms")}}:Json{};}
inline Json Public(){std::lock_guard lock(Gate);return PublicLocked();}
inline void ReceivePublic(const std::string& character,const Json& value){
    if(character.empty()||HelpyPersonalization::Guid(character)!=character)throw std::runtime_error("Invalid public profile character");const auto profile=ValidatePublic(value);std::lock_guard lock(Gate);
    const auto now=std::chrono::steady_clock::now();std::erase_if(Shared,[&](const auto& entry){return entry.second.Expires<=now;});
    if(profile.is_null()){Shared.erase(character);return;}if(!Shared.contains(character)&&Shared.size()>=256)throw std::runtime_error("Public profile limit reached");
    Shared[character]={profile,now+std::chrono::minutes(15)};
}
inline Json PublicFor(const std::string& character,bool local=false){std::lock_guard lock(Gate);if(local)return PublicLocked();const auto found=Shared.find(HelpyPersonalization::Guid(character));return found!=Shared.end()&&found->second.Expires>std::chrono::steady_clock::now()?found->second.Public:Json{};}
inline void ClearShared(){std::lock_guard lock(Gate);Shared.clear();}
inline std::string Name(const Json& row){const auto character=row.value("Name",std::string{});std::lock_guard lock(Gate);std::string profile;if(Ready&&row.value("IsLocal",false))profile=Document.value("ProfileName",std::string{});else {const auto found=Shared.find(HelpyPersonalization::Guid(row.value("Guid",std::string{})));if(found!=Shared.end()&&found->second.Expires>std::chrono::steady_clock::now())profile=found->second.Public.value("ProfileName",std::string{});}return profile.empty()?character:profile+" ("+character+")";}
inline void Shutdown(){if(Migration.valid())Migration.wait();}
}
