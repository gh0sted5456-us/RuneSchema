#pragma once
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <cctype>
#include <algorithm>
#include <atomic>
namespace PS::LoadProtectionNotice {
struct Notice {std::size_t Removed=0,Skipped=0;bool Bypassed=false;std::uint64_t Version=0;};
inline std::mutex Mutex;
inline std::map<std::string,Notice> Pending;
inline std::uint64_t Version=0;
inline std::atomic_uint64_t SessionBypasses{0};
inline std::string Key(const std::string& id){std::string result;for(unsigned char c:id){if(c=='-'||c=='{'||c=='}')continue;if(!std::isxdigit(c))return {};result+=static_cast<char>(std::tolower(c));}return result.size()==32?result:std::string{};}
inline void Record(const std::string& id,std::size_t removed,std::size_t skipped,bool bypassed)noexcept{
    try{const auto key=Key(id);if(key.empty()||(!removed&&!skipped&&!bypassed))return;std::lock_guard lock(Mutex);
        if(!Pending.contains(key)&&Pending.size()>=256)return;
        auto& notice=Pending[key];notice.Removed+=removed;notice.Skipped=std::max(notice.Skipped,skipped);notice.Bypassed|=bypassed;notice.Version=++Version;
    }catch(...){} // Notification failure must never affect loading or a verified cleanup.
}
inline std::optional<Notice> Peek(const std::string& id){const auto key=Key(id);std::lock_guard lock(Mutex);const auto found=Pending.find(key);return found==Pending.end()?std::optional<Notice>{}:found->second;}
inline void Acknowledge(const std::string& id,std::uint64_t version){std::lock_guard lock(Mutex);const auto found=Pending.find(Key(id));if(found!=Pending.end()&&found->second.Version==version)Pending.erase(found);}
inline std::string Message(const Notice& notice){
    std::string message="RuneSchema: ";
    if(notice.Removed)message+="cleaned "+std::to_string(notice.Removed)+" unavailable-content records after backup. ";
    if(notice.Skipped)message+="Uncertain sections were kept; other cleanup continued. ";
    if(notice.Bypassed)message+="Native loading rejected the save; fallback entry was used. ";
    return message+"Details: RuneSchema.log.";
}
}
