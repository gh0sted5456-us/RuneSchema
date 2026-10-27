#pragma once
#include "Core/CacheBundle.h"
#include "Core/CacheBundleIO.h"
#include <algorithm>
#include <set>
#include <map>
#include <mutex>
#include <chrono>
#include <functional>
// File workers only. Cached paths/IDs are presentation, never a live registry.
namespace PS::HelpyPresentationCache {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline constexpr size_t MaxSources=512,MaxImages=32768;
inline fs::path Directory(const fs::path& icons){return icons.parent_path()/"content-library";}
inline std::mutex Mutex;
inline std::atomic<uint64_t> Epoch{0};
inline bool Native(const std::string& owner){return owner=="core"||owner.starts_with("plugins/");}
inline Json Index(const fs::path& folder){
    const auto file=folder/"index.json";if(!fs::exists(file))return {{"Version",1},{"Sources",Json::object()}};
    auto value=Json::parse(CacheBundleIO::Read(file,1024*1024),[](int depth,Json::parse_event_t,Json&){if(depth>16)throw std::runtime_error("[RS-CACHE-002] Cache library nesting exceeds its limit.");return true;});
    if(value.value("Version",0)!=1||!value.at("Sources").is_object()||value["Sources"].size()>MaxSources)throw std::runtime_error("[RS-CACHE-002] Invalid presentation cache index.");
    for(const auto& [id,entry]:value["Sources"].items())if(!CacheBundleIO::Digest(id)||!ContentSource::Valid(entry.at("Owner").get<std::string>())||!CacheBundleIO::Digest(entry.at("Fingerprint").get<std::string>())||!CacheBundleIO::Digest(entry.at("SHA256").get<std::string>())||!CacheBundle::Text(entry.at("Locale"),64)||!entry.at("Revision").is_number_unsigned())throw std::runtime_error("[RS-CACHE-002] Invalid cache source entry.");return value;
}
inline Json Data(const fs::path& folder,const Json& entry){
    const auto digest=entry.at("SHA256").get<std::string>();if(!CacheBundleIO::Digest(digest))throw std::runtime_error("[RS-CACHE-002] Invalid cache data digest.");
    const auto text=CacheBundleIO::Read(folder/(digest+".json"),CacheBundle::MaxMetadata);
    if(CacheBundleIO::Hash(text)!=digest)throw std::runtime_error("[RS-CACHE-004] Cached references failed integrity verification.");
    auto result=Json::parse(text,[](int depth,Json::parse_event_t,Json&){if(depth>20)throw std::runtime_error("[RS-CACHE-002] Cache references are too deeply nested.");return true;});(void)CacheBundle::Records(result.at("Records"));
    if(!result.at("Icons").is_object()||result["Icons"].size()>MaxImages)throw std::runtime_error("[RS-CACHE-002] Invalid artwork relationships.");
    for(const auto& [icon,pixel]:result["Icons"].items())if(!CacheBundle::Asset(icon)||!CacheBundleIO::Digest(pixel.at("SHA256").get<std::string>())||!pixel.at("Bytes").is_number_unsigned()||pixel.value("Bytes",uint64_t{})<20||pixel.value("Bytes",uint64_t{})>16+256*256*4)
        throw std::runtime_error("[RS-CACHE-002] Invalid cached artwork relationship.");return result;
}
struct Group {std::string Owner,Fingerprint,Locale,InstalledOwner;uint64_t PublishedVersion=0;Json Records=Json::array(),Icons=Json::object();};
struct Result {size_t Records=0,Images=0,Unchanged=0;std::set<std::string> ChangedIcons;};
using Check=std::function<void()>;
using Pixels=std::function<std::vector<char>(const std::string&)>;
inline Result Commit(const fs::path& icons,const std::vector<Group>& groups,uint64_t budget,const Pixels& pixels,const Check& check={}){
    std::lock_guard guard(Mutex);const auto folder=Directory(icons);CacheBundleIO::Safe(folder);fs::create_directories(folder);
    const auto lockFile=CacheBundleIO::NativePath(folder/"write.lock");CacheBundleIO::Safe(lockFile);
    struct Lock{HANDLE Handle;~Lock(){if(Handle!=INVALID_HANDLE_VALUE)CloseHandle(Handle);}}lock{CreateFileW(lockFile.c_str(),GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr)};
    if(lock.Handle==INVALID_HANDLE_VALUE)throw std::runtime_error("[RS-CACHE-009] Another cache import is in progress; retry shortly.");
    auto before=Index(folder),index=before;
    std::map<std::string,Json> documents;Result result;std::set<std::string> previousData,previousPixels;
    for(const auto& [id,entry]:before["Sources"].items())try{auto data=Data(folder,entry);previousData.insert(entry.at("SHA256"));for(const auto& [path,pixel]:data["Icons"].items())previousPixels.insert(pixel.at("SHA256"));documents[id]=std::move(data);}catch(...){}
    for(const auto& group:groups){
        if(check)check();if(!ContentSource::Valid(group.Owner)||!CacheBundleIO::Digest(group.Fingerprint)||group.Locale.empty()||group.Locale.size()>64)throw std::runtime_error("[RS-CACHE-002] Invalid cache source identity.");
        const auto logical=(Native(group.Owner)?std::string("native"):group.Owner)+"|"+group.Locale;
        auto id=CacheBundleIO::Hash(logical);
        // A repackaged mod folder can retain its exact content identity.
        if(!index["Sources"].contains(id))for(const auto& [known,entry]:index["Sources"].items())if(entry.value("Locale",std::string{})==group.Locale&&Native(entry.value("Owner",std::string{}))==Native(group.Owner)){
            const auto aliases=entry.value("Aliases",Json::array());
            if(entry.value("Fingerprint",std::string{})==group.Fingerprint||std::find(aliases.begin(),aliases.end(),group.Owner)!=aliases.end()){id=known;break;}
        }
        const auto old=index["Sources"].value(id,Json::object());const bool known=!old.empty();
        if(known&&group.PublishedVersion&&old.value("PublishedVersion",uint64_t{})>group.PublishedVersion)continue;
        const bool same=known&&old.value("Fingerprint",std::string{})==group.Fingerprint;
        if(known&&!same&&group.PublishedVersion<=old.value("PublishedVersion",uint64_t{})&&old.contains("History")&&std::find(old["History"].begin(),old["History"].end(),group.Fingerprint)!=old["History"].end())continue;
        {
            const bool merge=same&&documents.contains(id)&&(!group.PublishedVersion||group.PublishedVersion==old.value("PublishedVersion",uint64_t{}));
            auto data=merge?documents.at(id):Json{{"Records",Json::array()},{"Icons",Json::object()}};
            if(!merge&&documents.contains(id))for(const auto& [icon,pixel]:documents.at(id)["Icons"].items())result.ChangedIcons.insert(icon);
            auto owner=known?old.value("Owner",group.Owner):group.Owner;std::map<std::string,Json> records;
            for(const auto& row:data["Records"])records[row.at("Kind").get<std::string>()+"|"+row.at("Path").get<std::string>()]=row;
            for(const auto& value:group.Records){auto row=CacheBundle::Parse(value);if(!Native(owner)&&row.Owner!=owner){row.Owner=owner;row=CacheBundle::Parse(CacheBundle::Document(row));}
                const auto key=row.Kind+"|"+row.Path;const auto doc=CacheBundle::Document(row);if(!records.contains(key)||records.at(key)!=doc)++result.Records;records[key]=doc;}
            data["Records"]=Json::array();for(const auto& [key,row]:records)data["Records"].push_back(row);(void)CacheBundle::Records(data["Records"]);
            for(const auto& [icon,pixel]:group.Icons.items()){if(!CacheBundle::Asset(icon)||!CacheBundleIO::Digest(pixel.at("SHA256").get<std::string>()))throw std::runtime_error("[RS-CACHE-002] Invalid picture identity.");
                if(!data["Icons"].contains(icon)||data["Icons"][icon]!=pixel)result.ChangedIcons.insert(icon);data["Icons"][icon]=pixel;}
            const auto text=data.dump(),digest=CacheBundleIO::Hash(text);if(text.size()>CacheBundle::MaxMetadata)throw std::runtime_error("[RS-CACHE-005] Cache source exceeds its metadata limit.");
            auto history=old.value("History",Json::array());if(known&&!same){history.push_back(old.at("Fingerprint"));while(history.size()>16)history.erase(history.begin());}
            auto aliases=old.value("Aliases",Json::array());if(std::find(aliases.begin(),aliases.end(),group.Owner)==aliases.end())aliases.push_back(group.Owner);if(!group.InstalledOwner.empty()){if(!ContentSource::Valid(group.InstalledOwner)||!group.InstalledOwner.starts_with("mods/"))throw std::runtime_error("[RS-CACHE-002] Invalid installed cache owner.");if(std::find(aliases.begin(),aliases.end(),group.InstalledOwner)==aliases.end())aliases.push_back(group.InstalledOwner);}if(aliases.size()>32)throw std::runtime_error("[RS-CACHE-005] Too many cache source aliases.");
            const bool changed=!known||old.value("SHA256",std::string{})!=digest||!same;
            index["Sources"][id]={{"Owner",owner},{"Aliases",aliases},{"Locale",group.Locale},{"Fingerprint",group.Fingerprint},{"Revision",old.value("Revision",uint64_t{})+(changed?1:0)},
                {"PublishedVersion",std::max(group.PublishedVersion,old.value("PublishedVersion",uint64_t{}))},{"SHA256",digest},{"History",history}};documents[id]=std::move(data);
            if(changed&&!result.Records)result.Records=1;
        }
    }
    if(index["Sources"].size()>MaxSources)throw std::runtime_error("[RS-CACHE-005] Too many cached sources.");
    uint64_t total=0;size_t count=0,metadata=0;std::map<std::string,uint64_t> activePixels;std::set<std::string> activeData;
    for(const auto& [id,entry]:index["Sources"].items()){
        if(!documents.contains(id))continue;const auto& data=documents.at(id);count+=data["Records"].size();metadata+=data.dump().size();activeData.insert(entry.at("SHA256"));
        for(const auto& [icon,pixel]:data["Icons"].items())activePixels[pixel.at("SHA256").get<std::string>()]=pixel.at("Bytes").get<uint64_t>();
    }
    for(const auto& [digest,bytes]:activePixels)total+=bytes;
    if(total>budget||activePixels.size()>MaxImages||count>CacheBundle::MaxRecords||metadata>CacheBundle::MaxMetadata)throw std::runtime_error("[RS-CACHE-005] Presentation cache exceeds its configured budget.");
    for(const auto& group:groups)for(const auto& [icon,pixel]:group.Icons.items()){
        if(check)check();const auto digest=pixel.at("SHA256").get<std::string>();if(!activePixels.contains(digest))continue;
        const auto file=folder/"pixels"/(digest+".rsicon");bool valid=false;try{const auto data=CacheBundleIO::Read(file,16+256*256*4);valid=data.size()==pixel.at("Bytes").get<uint64_t>()&&CacheBundleIO::Hash(data)==digest;}catch(...){}
        if(valid){++result.Unchanged;continue;}const auto data=pixels(icon);
        if(data.size()!=pixel.at("Bytes").get<uint64_t>()||CacheBundleIO::Hash(std::string_view(data.data(),data.size()))!=digest)throw std::runtime_error("[RS-CACHE-004] Picture changed during cache publication.");
        CacheBundleIO::Publish(file,std::string_view(data.data(),data.size()));++result.Images;
    }
    for(const auto& [id,entry]:index["Sources"].items())if(documents.contains(id)){
        const auto digest=entry.at("SHA256").get<std::string>(),text=documents.at(id).dump();const auto file=folder/(digest+".json");bool valid=false;
        try{valid=CacheBundleIO::Hash(CacheBundleIO::Read(file,CacheBundle::MaxMetadata))==digest;}catch(...){}
        if(!valid)CacheBundleIO::Publish(file,text);
    }
    if(check)check();if(index!=before){CacheBundleIO::Publish(folder/"index.json",index.dump());++Epoch;}
    // Retire only files referenced by the old committed index, never loose files.
    for(const auto& digest:previousData)if(!activeData.contains(digest)){std::error_code error;fs::remove(folder/(digest+".json"),error);}
    for(const auto& digest:previousPixels)if(!activePixels.contains(digest)){std::error_code error;fs::remove(folder/"pixels"/(digest+".rsicon"),error);}
    return result;
}
inline Json Records(const fs::path& icons,const std::string& locale,const std::string& kind={}){
    const auto folder=Directory(icons);Json result=Json::array();const auto index=Index(folder);std::set<std::string> seen;std::vector<Json> entries;
    for(const auto& [id,entry]:index["Sources"].items())entries.push_back(entry);
    const auto rank=[&](const Json& entry){const auto language=entry.value("Locale",std::string{});return language==locale?0:language=="en-US"?1:2;};
    std::stable_sort(entries.begin(),entries.end(),[&](const auto& a,const auto& b){return rank(a)<rank(b);});
    for(const auto& entry:entries)try{for(auto value:Data(folder,entry)["Records"]){auto row=CacheBundle::Parse(value);if(!kind.empty()&&kind!=row.Kind)continue;
        if(!seen.insert(row.Kind+"|"+row.Path).second)continue;row.Data["CacheSourceVerified"]=false;row.Data["ObservedCurrent"]=false;row.Data["CacheLocale"]=entry.at("Locale");row.Data["CacheOwners"]=entry.value("Aliases",Json::array());row.Data["CacheRevision"]=entry.value("Revision",uint64_t{});row.Data["CacheFingerprint"]=entry.value("Fingerprint",std::string{});result.push_back(CacheBundle::Document(row));if(result.size()>=CacheBundle::MaxRecords)return result;}}catch(...){}
    return result;
}
inline fs::path IconFile(const fs::path& icons,const std::string& icon){
    // Called by the existing thumbnail file worker, never by the UI renderer.
    static std::mutex mutex;static fs::path location;static std::chrono::steady_clock::time_point next{};static uint64_t epoch=~uint64_t{};static std::map<std::string,fs::path> files;static fs::file_time_type stamp{};
    std::lock_guard lock(mutex);const auto folder=Directory(icons);const auto now=std::chrono::steady_clock::now();
    if(folder!=location||epoch!=Epoch.load()||now>=next){
        std::error_code error;const auto modified=fs::last_write_time(folder/"index.json",error);
        next=now+std::chrono::seconds(2);if(folder==location&&epoch==Epoch.load()&&!error&&modified==stamp){const auto found=files.find(icon);return found==files.end()?fs::path{}:found->second;}
        location=folder;epoch=Epoch;stamp=modified;files.clear();
        const auto index=Index(folder);for(const auto& [id,entry]:index["Sources"].items())try{for(const auto& [path,pixel]:Data(folder,entry)["Icons"].items())files[path]=folder/"pixels"/(pixel.at("SHA256").get<std::string>()+".rsicon");}catch(...){}
    }
    const auto found=files.find(icon);return found==files.end()?fs::path{}:found->second;
}
}
