#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <memory>
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
    for(const std::string_view prefix:{"/game/mods/","/game/runeschema/","/game/gameplay/mods/"})
        if(text.starts_with(prefix)){const auto next=text.find('/',prefix.size());if(next!=text.npos)end=next;break;}
    auto source=text.substr(1,end-1);
    if(source.size()>192||source.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-/.")!=source.npos)return {};
    return source;
}
struct Plan {
    std::map<std::string,std::string> DisplayNames;
    std::map<std::string,std::string> Versions;
    bool Authoritative=false,Complete=true;
    std::uint64_t Revision=0;
    std::map<std::string,std::set<std::string>> Owners;
    std::set<std::string> Registered;
    std::map<std::string,std::set<std::string>> DeclaredOwners; // Content declarations only, never borrowed icon ownership.
    std::uint64_t ContentRevision=0; // Icon references advance Revision, not item availability.
    using AssetOwnerMap=std::map<std::string,std::set<std::string>>;
    std::shared_ptr<const AssetOwnerMap> AssetOwners; // Immutable, process-local exact-path declarations; never sent wholesale to Helpy.
    std::shared_ptr<const AssetOwnerMap> AuthoredAssets; // Exact /assets targets inspected on the filesystem worker, not mount guesses.
    std::shared_ptr<const AssetOwnerMap> ContainerAssets; // Exact package -> supplying container owner tokens; discovery, never gameplay authority.
    std::shared_ptr<const AssetOwnerMap> ContainerFiles; // Exact package -> logical container filenames, including ambiguous suppliers.
};
inline std::mutex Mutex;
inline Plan Current;
inline void SetDisplayName(const std::string& owner,const std::string& label,const std::string& version={}) noexcept {try{
    if(owner.empty()||owner.size()>128||owner.find("..")!=owner.npos||owner=="."||owner.find_first_of("/\\\r\n\t:")!=owner.npos||label.empty()||label.size()>256
        ||std::any_of(owner.begin(),owner.end(),[](unsigned char c){return c<32||c==127;})
        ||std::any_of(label.begin(),label.end(),[](unsigned char c){return c<32||c==127;}))return;
    std::lock_guard lock(Mutex);const auto key=Lower(owner);
    if(Current.DisplayNames.size()>=512&&!Current.DisplayNames.contains(key))return;
    if(Current.DisplayNames[key]!=label){Current.DisplayNames[key]=label;++Current.Revision;}
    if(!version.empty()&&version.size()<=128&&version.find_first_of("\r\n\t")==version.npos
        &&std::none_of(version.begin(),version.end(),[](unsigned char c){return c<32||c==127;})&&Current.Versions[key]!=version){Current.Versions[key]=version;++Current.Revision;}
}catch(...) {}}
inline std::shared_ptr<Plan::AssetOwnerMap> CurrentAssetOwners=std::make_shared<Plan::AssetOwnerMap>();
inline std::string AssetKey(const std::string& path){
    if(Source(path).empty()||path.find_first_of("\\\r\n\t:")!=path.npos||std::any_of(path.begin(),path.end(),[](unsigned char c){return c<32||c==127;}))return {};
    auto key=Lower(path);const auto dot=key.find('.',key.find_last_of('/')+1);if(dot!=key.npos)key.resize(dot);return key;
}
inline void Reset() noexcept {try {std::lock_guard lock(Mutex);const auto revision=Current.Revision+1,content=Current.ContentRevision+1;Current={};CurrentAssetOwners=std::make_shared<Plan::AssetOwnerMap>();Current.Authoritative=true;Current.Complete=false;Current.Revision=revision;Current.ContentRevision=content;}catch(...) {}}
inline void Seal() noexcept {try{std::lock_guard lock(Mutex);if(!Current.Complete){Current.Complete=true;++Current.Revision;}}catch(...) {}}
inline void Declare(const std::string& owner,const std::string& asset) noexcept {
    try {const auto source=Source(asset),name=Lower(owner);
        if(source.empty()||name.empty()||name.size()>128||name=="."||name==".."||name.find_first_of("/\\\r\n\t:")!=name.npos)return;
        std::lock_guard lock(Mutex);if(Current.Owners.size()>=512&&!Current.Owners.contains(source))return;
        const auto assetKey=AssetKey(asset);
        if(!assetKey.empty()&&(CurrentAssetOwners->size()<262144||CurrentAssetOwners->contains(assetKey))){
            const auto found=CurrentAssetOwners->find(assetKey);
            if(found==CurrentAssetOwners->end()||(!found->second.contains(name)&&found->second.size()<16)){
                if(CurrentAssetOwners.use_count()>1)CurrentAssetOwners=std::make_shared<Plan::AssetOwnerMap>(*CurrentAssetOwners);
                (*CurrentAssetOwners)[assetKey].insert(name);++Current.Revision;++Current.ContentRevision;
            }
        }
        auto& owners=Current.Owners[source];if(owners.size()<256&&owners.insert(name).second)++Current.Revision;
        if(Current.DeclaredOwners.size()<512||Current.DeclaredOwners.contains(source)){
            auto& declared=Current.DeclaredOwners[source];if(declared.size()<256&&declared.insert(name).second){++Current.Revision;++Current.ContentRevision;}
        }
    }catch(...) {/* Optional cache metadata cannot fail content registration. */}
}
inline void Registered(const std::string& asset) noexcept {
    try {const auto source=Source(asset);if(source.empty())return;std::lock_guard lock(Mutex);
        Current.Authoritative=true;
        if(Current.Registered.size()<512&&Current.Registered.insert(source).second){++Current.Revision;++Current.ContentRevision;}
    }catch(...) {}
}
// Publish cache ownership only AFTER the loose or sharded registry transaction
// succeeds. Declaration is not native registration. Sharded PAK manifests must
// contribute their actual provider folders just like loose manifests do.
template<class Manifest> inline void DeclareManifest(const Manifest& manifest) noexcept {
    try{
        std::set<std::string> seen;
        const auto add=[&](const std::string& path){const auto key=AssetKey(path);if(!key.empty()&&seen.insert(key).second)Declare(manifest.Owner,path);};
        for(const auto& lane:manifest.AssetLanes)for(const auto& path:lane.Paths)add(path);
        for(const auto& path:manifest.WearableMeshData)add(path);
        for(const auto& path:manifest.MeleeAttackClasses)add(path);
        for(const auto& path:manifest.RangedAttackClasses)add(path);
        for(const auto& path:manifest.CacheAssets)add(path);
        for(const auto& [lane,entries]:manifest.LoaderDefinitions.items())for(const auto& [path,body]:entries.items())add(path);
    }catch(...){} // Optional bookkeeping cannot undo an accepted registry.
}
inline Plan Snapshot(){std::lock_guard lock(Mutex);auto result=Current;result.AssetOwners=CurrentAssetOwners;return result;}
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
