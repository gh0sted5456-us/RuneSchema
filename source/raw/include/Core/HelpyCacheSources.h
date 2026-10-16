#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>

// Copied source identities only. This is never an asset registry, a loader, or
// evidence that an item is available. Only the live catalogue supplies items.
namespace PS::HelpyCacheSources {
inline std::string Lower(std::string value) {
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return value;
}
inline std::string Source(std::string_view path) {
    if(path.empty()||path.front()!='/'||path.size()>4096||path.find("..")!=path.npos)return {};
    const auto text=Lower(std::string(path));auto end=text.find('/',1);
    if(end==text.npos||end==1)return {};
    if(text.starts_with("/game/mods/")||text.starts_with("/game/runeschema/")) {
        const auto owner=text.find('/',end+1);if(owner!=text.npos){const auto next=text.find('/',owner+1);if(next!=text.npos)end=next;}
    }
    auto source=text.substr(1,end-1);
    if(source.size()>192||source.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-/.")!=source.npos)return {};
    return source;
}
struct Plan {
    bool Authoritative=false;
    std::uint64_t Revision=0;
    std::map<std::string,std::set<std::string>> Owners;
    std::set<std::string> Registered;
};
inline std::mutex Mutex;
inline Plan Current;
inline void Reset() noexcept {try {std::lock_guard lock(Mutex);const auto revision=Current.Revision+1;Current={};Current.Authoritative=true;Current.Revision=revision;}catch(...) {}}
inline void Declare(const std::string& owner,const std::string& asset) noexcept {
    try {const auto source=Source(asset),name=Lower(owner);
        if(source.empty()||name.empty()||name.size()>128||name=="."||name==".."||name.find_first_of("/\\\r\n\t:")!=name.npos)return;
        std::lock_guard lock(Mutex);if(Current.Owners.size()>=512&&!Current.Owners.contains(source))return;
        auto& owners=Current.Owners[source];if(owners.size()<256&&owners.insert(name).second)++Current.Revision;
    }catch(...) {/* Optional cache metadata cannot fail content registration. */}
}
inline void Registered(const std::string& asset) noexcept {
    try {const auto source=Source(asset);if(source.empty())return;std::lock_guard lock(Mutex);
        Current.Authoritative=true;
        if(Current.Registered.size()<512&&Current.Registered.insert(source).second)++Current.Revision;
    }catch(...) {}
}
inline Plan Snapshot(){std::lock_guard lock(Mutex);return Current;}
// A registered item can reference an icon in another mounted content source.
// Observe that native relationship, not a UI-supplied or cached asset list.
inline void Reference(const std::string& asset,const std::string& icon) noexcept {
    try {const auto source=Source(asset),dependency=Source(icon);
        if(source.empty()||dependency.empty()||source==dependency)return;
        std::lock_guard lock(Mutex);if(!Current.Registered.contains(source))return;
        if(Current.Registered.size()>=512&&!Current.Registered.contains(dependency))return;
        if(Current.Registered.insert(dependency).second)++Current.Revision;
        const auto found=Current.Owners.find(source);
        if(found!=Current.Owners.end())for(const auto& owner:found->second){
            auto& owners=Current.Owners[dependency];if(owners.size()<256&&owners.insert(owner).second)++Current.Revision;
        }
    }catch(...) {}
}
inline std::set<std::string> ActiveOwners(const Plan& plan) {
    std::set<std::string> owners;for(const auto& source:plan.Registered){const auto found=plan.Owners.find(source);if(found!=plan.Owners.end())owners.insert(found->second.begin(),found->second.end());}return owners;
}
inline std::string FileOwner(const std::string& relative) {
    const auto text=Lower(relative);constexpr std::string_view prefix="/ue4ss/mods/runeschema/mods/";
    const auto begin=text.find(prefix);if(begin==text.npos)return {};
    const auto start=begin+prefix.size(),end=text.find('/',start);return end==text.npos?std::string{}:text.substr(start,end-start);
}
inline std::string Folder(const std::string& source) {
    std::string name;for(unsigned char c:source)name.push_back(std::isalnum(c)||c=='-'||c=='_'?static_cast<char>(c):'_');return name;
}
}
