#pragma once
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <cctype>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include "../../../shared/CleanupNotice.h"
namespace PS::LoadProtectionNotice {
struct Notice {std::size_t Removed=0,Skipped=0;bool Bypassed=false;std::uint64_t Version=0;std::map<std::string,std::size_t> Owners;std::size_t PayloadRemoved=0;std::map<std::string,std::size_t> PayloadOwners;};
inline std::mutex Mutex;
inline std::map<std::string,Notice> Pending;
inline std::uint64_t Version=0;
inline std::atomic_uint64_t SessionBypasses{0};
inline std::map<std::string,std::size_t> UninstalledOwners(const nlohmann::json& rows,const std::map<std::string,std::size_t>& known,const nlohmann::json& active,const std::filesystem::path& mods){
    const auto fold=[](std::string value){for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;};
    std::map<std::string,std::size_t> result;
    std::error_code error;if(!std::filesystem::is_directory(mods,error)||error)return result;
    for(const auto& row:rows){if(row.value("Kind",std::string{})=="Appearance")continue;const auto owner=row.value("Mod",std::string{});
        if(!known.contains(owner)||!CleanupNotice::ModName(owner))continue;
        if(std::none_of(active.begin(),active.end(),[&](const auto& mod){return fold(mod.value("Id",std::string{}))==fold(owner);})
            &&!std::filesystem::exists(mods/owner,error)&&!error)++result[owner];
    }return result;
}
inline std::string Key(const std::string& id){std::string result;for(unsigned char c:id){if(c=='-'||c=='{'||c=='}')continue;if(!std::isxdigit(c))return {};result+=static_cast<char>(std::tolower(c));}return result.size()==32?result:std::string{};}
inline void Record(const std::string& id,std::size_t removed,std::size_t skipped,bool bypassed,const std::map<std::string,std::size_t>& owners={},bool payload=false)noexcept{
    try{const auto key=Key(id);if(key.empty()||(!payload&&!removed&&!skipped&&!bypassed))return;std::lock_guard lock(Mutex);
        if(payload&&!removed&&!Pending.contains(key))return;
        if(!Pending.contains(key)&&Pending.size()>=256)return;
        auto& notice=Pending[key];
        if(payload){notice.Removed-=notice.PayloadRemoved;for(const auto& [owner,count]:notice.PayloadOwners){notice.Owners[owner]-=count;if(!notice.Owners[owner])notice.Owners.erase(owner);}notice.PayloadOwners.clear();notice.PayloadRemoved=removed;}
        notice.Removed+=removed;notice.Skipped=std::max(notice.Skipped,skipped);notice.Bypassed|=bypassed;notice.Version=++Version;
        std::size_t remaining=removed;for(const auto& [owner,count]:owners)if(CleanupNotice::ModName(owner)&&count<=remaining){notice.Owners[owner]+=count;if(payload)notice.PayloadOwners[owner]+=count;remaining-=count;}
    }catch(...){} // Notification failure must never affect loading or a verified cleanup.
}
inline std::optional<Notice> Peek(const std::string& id){const auto key=Key(id);std::lock_guard lock(Mutex);const auto found=Pending.find(key);return found==Pending.end()?std::optional<Notice>{}:found->second;}
inline void Acknowledge(const std::string& id,std::uint64_t version){std::lock_guard lock(Mutex);const auto found=Pending.find(Key(id));if(found!=Pending.end()&&found->second.Version==version)Pending.erase(found);}
inline std::string Message(const Notice& notice){
    return CleanupNotice::Message(notice.Removed,notice.Owners.size()==1&&notice.Owners.begin()->second==notice.Removed?notice.Owners.begin()->first:std::string{});
}
}
