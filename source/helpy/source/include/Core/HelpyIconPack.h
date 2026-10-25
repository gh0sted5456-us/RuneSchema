#pragma once
#include "Core/HelpySourceCache.h"
#include "Core/HelpyKitFiles.h"
#include "Core/ContentSource.h"
#include "Core/IconCacheCoverage.h"
#include "Core/CacheBundleStore.h"
#include <bcrypt.h>
#include <array>
#include <cstring>
#include <optional>
#pragma comment(lib,"bcrypt.lib")

// Portable presentation bundles: pixels plus typed path/ID references.
// No archive extraction, native objects, executable definitions or mutations.
namespace PS::HelpyIconPack {
namespace fs=std::filesystem;
inline constexpr size_t MaxIcons=32768,MaxManifest=80*1024*1024;
inline constexpr uint64_t MaxPack=512ull*1024*1024;
inline constexpr char Magic[8]={'R','S','I','P','A','C','K','1'};
inline constexpr char BundleMagic[8]={'R','S','C','A','C','H','E','1'};
enum class Phase { Discover,Verify,Validate,Write,Commit,Complete };
struct Progress {
    std::atomic<bool> Cancel{false};std::atomic<size_t> Done{0},Total{0};
    std::atomic<uint64_t> BytesDone{0},BytesTotal{0};std::atomic<Phase> Stage{Phase::Discover};
    void Begin(Phase stage,size_t total=0,uint64_t bytes=0){Done=0;Total=total;BytesDone=0;BytesTotal=bytes;Stage=stage;}
};
inline void Check(Progress& p){if(p.Cancel)throw std::runtime_error("[RS-CACHE-006] Cache operation cancelled; previous references retained.");}
inline bool Digest(const std::string& text){return text.size()==64&&text.find_first_not_of("0123456789abcdef")==text.npos;}
template<class Input> inline nlohmann::json Json(const Input& input){return nlohmann::json::parse(input,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){if(depth>16)throw std::runtime_error("Icon pack JSON nesting exceeds its limit");return true;});}
class Sha256 {
    BCRYPT_ALG_HANDLE Algorithm{};BCRYPT_HASH_HANDLE Handle{};
public:
    Sha256(){if(BCryptOpenAlgorithmProvider(&Algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA-256 unavailable");
        if(BCryptCreateHash(Algorithm,&Handle,nullptr,0,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(Algorithm,0);Algorithm=nullptr;throw std::runtime_error("SHA-256 initialization failed");}}
    ~Sha256(){if(Handle)BCryptDestroyHash(Handle);if(Algorithm)BCryptCloseAlgorithmProvider(Algorithm,0);}
    Sha256(const Sha256&)=delete;Sha256& operator=(const Sha256&)=delete;
    void Add(const char* data,size_t length){if(length>ULONG_MAX||BCryptHashData(Handle,reinterpret_cast<PUCHAR>(const_cast<char*>(data)),static_cast<ULONG>(length),0)<0)throw std::runtime_error("SHA-256 update failed");}
    void Add(const std::string& value){Add(value.data(),value.size());}
    std::string Finish(){std::array<unsigned char,32> bytes{};if(BCryptFinishHash(Handle,bytes.data(),ULONG(bytes.size()),0)<0)throw std::runtime_error("SHA-256 finish failed");
        constexpr char digits[]="0123456789abcdef";std::string result;for(const auto c:bytes){result+=digits[c>>4];result+=digits[c&15];}return result;}
};
inline std::string Hash(const std::vector<char>& bytes){Sha256 sha;sha.Add(bytes.data(),bytes.size());return sha.Finish();}
inline void Safe(const fs::path& path){
    // Inspect uses extended-length Windows paths. Walk normal logical parents,
    // then restore the prefix for each Win32 check (including long filenames).
    auto text=path.wstring();if(text.starts_with(L"\\\\?\\UNC\\"))text=L"\\\\"+text.substr(8);else if(text.starts_with(L"\\\\?\\"))text=text.substr(4);
    auto current=fs::absolute(text).lexically_normal();
    for(;;){const auto flags=GetFileAttributesW(HelpySourceCache::NativePath(current).c_str());
        if(flags!=INVALID_FILE_ATTRIBUTES&&(flags&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Icon cache paths cannot pass through links or junctions");
        if(flags==INVALID_FILE_ATTRIBUTES){const auto error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)throw std::runtime_error("Icon path cannot be inspected (Windows error "+std::to_string(error)+")");}
        const auto parent=current.parent_path();if(parent.empty()||parent==current)break;current=parent;
    }
}
inline void Publish(const fs::path& target,const std::vector<char>& bytes,bool replace=true){
    Safe(target);fs::create_directories(target.parent_path());Safe(target);
    static std::atomic<uint64_t> serial{0};const auto temporary=target.wstring()+L".pack-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++serial)+L".tmp";Safe(temporary);
    try{{std::ofstream file(fs::path(temporary),std::ios::binary|std::ios::trunc);file.write(bytes.data(),std::streamsize(bytes.size()));file.close();if(!file)throw std::runtime_error("Icon cache write failed");}
        Safe(target);if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH|(replace?MOVEFILE_REPLACE_EXISTING:0)))throw std::runtime_error("Icon cache publish failed");
    }catch(...){std::error_code ec;fs::remove(temporary,ec);throw;}
}
inline bool IconPath(const std::string& icon){return icon.size()<=2048&&!HelpyCacheSources::Source(icon).empty()&&icon.find_first_of("\\\r\n\t:") == icon.npos&&icon.find('.')!=icon.npos&&std::none_of(icon.begin(),icon.end(),[](unsigned char c){return c<32||c==127;});}
inline void Index(const fs::path& file,const std::string& icon,std::set<std::string> origins={},std::optional<bool> displayOnly=std::nullopt) noexcept {
    try{if(!IconPath(icon))return;const auto index=fs::path(file.wstring()+L".json");Safe(index);
        bool visual=displayOnly.value_or(false);nlohmann::json previous;
        try{if(fs::is_regular_file(index)&&fs::file_size(index)<=8192){std::ifstream input(index);auto old=nlohmann::json::parse(input);
            if(old.value("Icon",std::string{})==icon){previous=old;if(origins.empty()&&!displayOnly.has_value())return;if(!displayOnly.has_value())visual=old.value("DisplayOnly",false);if(old.contains("Origins")&&old["Origins"].is_array()&&old["Origins"].size()<=128)for(const auto& value:old["Origins"])if(value.is_string()&&ContentSource::Valid(value.get<std::string>()))origins.insert(value.get<std::string>());}}}catch(...){} // Repair a corrupt sidecar, not a corrupt pixel file.
        const auto document=nlohmann::json({{"Icon",icon},{"Origins",origins},{"DisplayOnly",visual}});
        if(document==previous)return; // Re-importing an identical mapping is read-only.
        const auto text=document.dump();Publish(index,{text.begin(),text.end()});}catch(...){}
}
inline std::vector<char> Read(const fs::path& file,size_t limit){Safe(file);const auto size=fs::file_size(file);if(size>limit)throw std::runtime_error("Icon pack file exceeds its limit");
    std::vector<char> bytes(static_cast<size_t>(size));std::ifstream stream(file,std::ios::binary);stream.read(bytes.data(),std::streamsize(bytes.size()));if(!stream)throw std::runtime_error("Icon pack file is unreadable");return bytes;}
inline void Pixels(const std::vector<char>& bytes){
    uint32_t width=0,height=0,length=0;if(bytes.size()<16||std::memcmp(bytes.data(),"RSI3",4))throw std::runtime_error("Invalid icon pixel format");
    std::memcpy(&width,bytes.data()+4,4);std::memcpy(&height,bytes.data()+8,4);std::memcpy(&length,bytes.data()+12,4);
    if(!width||!height||width>256||height>256||length!=width*height*4||bytes.size()!=16+length)throw std::runtime_error("Invalid icon pixel dimensions");
}
inline fs::path Target(const fs::path& cache,const HelpySourceCache::Snapshot& sources,const std::string& icon){
    if(!IconPath(icon))throw std::runtime_error("Invalid icon asset path");const auto group=HelpyCacheSources::Source(icon);const auto found=sources.SourceKeys.find(group);
    if(found==sources.SourceKeys.end()||!HelpySourceCache::HexKey(found->second))return {};
    return cache/(HelpyCacheSources::Folder(group)+"-"+HelpySourceCache::Hex(HelpySourceCache::Hash(group)))/found->second/(HelpySourceCache::Hex(HelpySourceCache::Hash(found->second+"|"+icon))+".rsicon");
}
// Hash real content on an explicit import/export worker, never the render/game
// thread or normal startup. Memoization is local and keyed by file metadata.
// Package identities exclude absolute paths, timestamps and storefront binaries.
inline std::map<std::string,std::string> Portable(const fs::path& root,const HelpySourceCache::Snapshot& sources,const std::set<std::string>& wanted,const fs::path& cache,Progress& progress,bool nativeOnly=false,const std::string& contentOwner={}){
    if(!contentOwner.empty()&&(!contentOwner.starts_with("mods/")||ContentSource::Mod(contentOwner.substr(5))!=contentOwner))throw std::runtime_error("Invalid portable cache owner");
    const auto selectedOwner=[&](const std::string& owner){return contentOwner.empty()||contentOwner=="mods/"+owner;};
    HelpyCacheSources::Plan plan;plan.Authoritative=true;plan.Revision=sources.PlanRevision;plan.Owners=sources.Owners;for(const auto& [group,key]:sources.SourceKeys)plan.Registered.insert(group);
    const auto inspected=HelpySourceCache::Inspect(root,plan);
    if(inspected.Key!=sources.Key)throw std::runtime_error("[RS-CACHE-001] Content changed. Wait for the source list to refresh, then retry the bundle.");
    std::map<std::string,std::map<std::string,fs::path>> ownerFiles;std::map<std::string,fs::path> baseFiles;
    for(const auto& [name,path]:inspected.FilePaths){const auto owner=HelpyCacheSources::FileOwner(name);
        if(!owner.empty()){const auto marker="/runeschema/mods/";const auto begin=name.find(marker);ownerFiles[owner][name.substr(begin+std::strlen(marker))]=path;}
        else if(ContentSource::NativeContentFile(name))baseFiles[name]=path;
    }
    if(baseFiles.empty())throw std::runtime_error("Game content identity is unavailable; icon packs cannot be matched safely yet");
    const auto memoFile=cache/"portable-digests.json";nlohmann::json memo=nlohmann::json::object();
    try{const auto bytes=Read(memoFile,16*1024*1024);memo=Json(bytes);if(!memo.is_object())memo=nlohmann::json::object();}catch(...){memo=nlohmann::json::object();}
    const auto previousMemo=memo;
    std::set<fs::path> files;for(const auto& [name,path]:baseFiles)files.insert(path);
    if(!nativeOnly)for(const auto& group:wanted)if(sources.SourceKeys.contains(group)){const auto found=sources.Owners.find(group);if(found!=sources.Owners.end())for(const auto& owner:found->second)if(selectedOwner(owner))for(const auto& [name,path]:ownerFiles[owner])files.insert(path);}
    uint64_t sourceBytes=0;for(const auto& path:files){Check(progress);sourceBytes+=fs::file_size(path);}
    progress.Begin(Phase::Verify,files.size(),sourceBytes);std::map<fs::path,std::string> hashes;
    for(const auto& path:files){Check(progress);Safe(path);const auto size=fs::file_size(path);const auto stamp=fs::last_write_time(path).time_since_epoch().count();const auto key=path.generic_string();
        auto old=memo.value(key,nlohmann::json::object());std::string digest;
        try{if(old.is_object()&&old.value("Size",uint64_t(-1))==size&&old.value("Stamp",int64_t{})==stamp&&old.contains("SHA256")&&old["SHA256"].is_string()&&Digest(old["SHA256"].get<std::string>()))digest=old["SHA256"].get<std::string>();}catch(...){}
        if(digest.empty()){Sha256 sha;std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Cannot verify icon source content");std::vector<char> data(1024*1024);
            while(input){Check(progress);input.read(data.data(),data.size());sha.Add(data.data(),size_t(input.gcount()));progress.BytesDone+=uint64_t(input.gcount());}if(!input.eof())throw std::runtime_error("Icon source read failed");
            if(fs::file_size(path)!=size||fs::last_write_time(path).time_since_epoch().count()!=stamp)throw std::runtime_error("Source changed during verification; retry later");digest=sha.Finish();}
        else progress.BytesDone+=size;
        hashes[path]=digest;memo[key]={{"Size",size},{"Stamp",stamp},{"SHA256",digest}};++progress.Done;
    }
    Check(progress);try{if(memo!=previousMemo&&memo.size()<=100000){const auto text=memo.dump();if(text.size()<=16*1024*1024)Publish(memoFile,{text.begin(),text.end()});}}catch(...){}
    Sha256 base; base.Add("rsmk-icon-content-v1\n");for(const auto& [name,path]:baseFiles)base.Add(name+"|"+hashes.at(path)+"\n");const auto baseKey=base.Finish();
    std::map<std::string,std::string> result;
    for(const auto& group:wanted){if(!sources.SourceKeys.contains(group))continue;Sha256 sha;sha.Add(baseKey+"|"+group+"\n");bool complete=true;
        const auto owners=sources.Owners.find(group);if(!nativeOnly&&owners!=sources.Owners.end())for(const auto& owner:owners->second){if(!selectedOwner(owner))continue;if(ownerFiles[owner].empty()){complete=false;break;}for(const auto& [name,path]:ownerFiles[owner])sha.Add(name+"|"+hashes.at(path)+"\n");}
        if(complete)result[group]=sha.Finish();
    }
    // Recheck the cheap metadata plan after lengthy reads; an update must not
    // publish old pixels under the identity of new content.
    if(HelpySourceCache::Inspect(root,plan).Key!=sources.Key)throw std::runtime_error("Source changed during verification; retry later");return result;
}
struct Entry {std::string Icon,Source,SHA256;uint32_t Bytes=0;uint64_t Offset=0;std::set<std::string> Origins;};
struct Manifest {nlohmann::json Sources;std::vector<Entry> Icons;nlohmann::json Coverage;std::vector<CacheBundle::Record> Records;std::string Locale,ContentOwner;int Version=1;};
inline Manifest Parse(const nlohmann::json& value,uint64_t payload){
    const auto version=value.value("Version",0);
    if(!value.is_object()||(version!=1&&version!=2)||!value.contains("Sources")||!value["Sources"].is_object()||value["Sources"].size()>512
        ||!value.contains("Icons")||!value["Icons"].is_array()||(version==1&&value["Icons"].empty())||value["Icons"].size()>MaxIcons)throw std::runtime_error("[RS-CACHE-002] Invalid cache bundle manifest");
    Manifest result;result.Sources=value["Sources"];for(const auto& [group,key]:result.Sources.items())if(HelpyCacheSources::Source("/"+group+"/x.x")!=group||!key.is_string()||!Digest(key.get<std::string>()))throw std::runtime_error("Invalid icon pack source");
    result.Version=version;
    result.ContentOwner=value.value("ContentOwner",std::string{});
    if(!result.ContentOwner.empty()&&(version!=2||!result.ContentOwner.starts_with("mods/")||ContentSource::Mod(result.ContentOwner.substr(5))!=result.ContentOwner))throw std::runtime_error("Invalid bundle content owner");
    if(version==2){
        if(value.value("Kind",std::string{})!="RuneSchemaCacheBundle"||!value.contains("Locale")||!CacheBundle::Text(value["Locale"],64)||value["Locale"].get_ref<const std::string&>().empty())throw std::runtime_error("[RS-CACHE-002] Invalid bundle kind or locale");
        result.Locale=value["Locale"].get<std::string>();result.Records=CacheBundle::Records(value.at("Records"));
        for(const auto& row:result.Records)if(!result.Sources.contains(row.Source)||(!result.ContentOwner.empty()&&row.Owner!=result.ContentOwner))throw std::runtime_error("[RS-CACHE-002] Record has no source fingerprint or escapes its content owner");
        if(result.Records.empty()&&value["Icons"].empty())throw std::runtime_error("[RS-CACHE-002] Empty cache bundle");
    }
    uint64_t offset=0;std::set<std::string> paths;
    for(const auto& row:value["Icons"]){Entry e;e.Icon=row.at("Icon").get<std::string>();e.Source=row.at("Source").get<std::string>();e.SHA256=row.at("SHA256").get<std::string>();
        const auto bytes=row.at("Bytes").get<int64_t>();if(!IconPath(e.Icon)||e.Source!=HelpyCacheSources::Source(e.Icon)||!result.Sources.contains(e.Source)||!Digest(e.SHA256)||bytes<20||bytes>16+256*256*4||!paths.insert(e.Icon).second)throw std::runtime_error("Invalid icon pack entry");
        if(row.contains("Origins")){if(!row["Origins"].is_array()||row["Origins"].size()>128)throw std::runtime_error("Invalid icon origin list");for(const auto& origin:row["Origins"]){if(!origin.is_string()||!ContentSource::Valid(origin.get<std::string>()))throw std::runtime_error("Invalid icon origin");e.Origins.insert(origin.get<std::string>());}}
        e.Bytes=uint32_t(bytes);e.Offset=offset;offset+=e.Bytes;if(offset>MaxPack)throw std::runtime_error("Icon pack payload exceeds its limit");result.Icons.push_back(std::move(e));
    }
    if(offset!=payload)throw std::runtime_error("Icon pack payload length mismatch");
    if(value.contains("Coverage")){
        const auto& coverage=value["Coverage"];
        if(!coverage.is_object()||!coverage.contains("Complete")||!coverage["Complete"].is_boolean()
            ||coverage.value("Expected",int64_t(-1))<0||coverage.value("Expected",int64_t(-1))>int64_t(MaxIcons)
            ||coverage.value("Exported",size_t{})!=result.Icons.size())throw std::runtime_error("Invalid icon pack coverage");
        result.Coverage=coverage;
    }
    return result;
}
inline Manifest Header(std::ifstream& input,uint64_t& start,uint64_t size){
    char magic[8]{};uint32_t length=0;input.read(magic,8);input.read(reinterpret_cast<char*>(&length),4);
    const bool bundle=!std::memcmp(magic,BundleMagic,8);
    if(!input||(!bundle&&std::memcmp(magic,Magic,8))||!length||length>(bundle?MaxManifest:8*1024*1024)||size>MaxPack||size<12ull+length)throw std::runtime_error("[RS-CACHE-002] Not a supported cache bundle or legacy icon pack");
    std::string text(length,'\0');input.read(text.data(),length);if(!input)throw std::runtime_error("[RS-CACHE-002] Truncated cache manifest");start=12ull+length;auto result=Parse(Json(text),size-start);
    if((result.Version==2)!=bundle)throw std::runtime_error("[RS-CACHE-002] Cache header/version mismatch");return result;
}
inline std::vector<char> Blob(std::ifstream& input,const Entry& entry,uint64_t start){
    input.seekg(std::streamoff(start+entry.Offset));std::vector<char> bytes(entry.Bytes);input.read(bytes.data(),bytes.size());if(!input)throw std::runtime_error("Truncated icon pack pixels");Pixels(bytes);
    if(Hash(bytes)!=entry.SHA256)throw std::runtime_error("[RS-CACHE-004] Cached artwork checksum mismatch");return bytes;
}
struct Result {size_t Imported=0,Skipped=0,Exported=0;std::string Message;size_t Unchanged=0;std::vector<std::string> ChangedIcons;size_t Packs=0,Warnings=0,RecordsImported=0,RecordsExported=0;};
enum class ImportPolicy { VerifiedContent,ServerDisplayOnly,NativeArtworkFallback };
inline Result Import(const fs::path& file,const fs::path& root,const fs::path& cache,const HelpySourceCache::Snapshot& snapshot,uint64_t budget,Progress& progress,ImportPolicy policy=ImportPolicy::VerifiedContent,const std::string& source={},const std::set<std::string>& allowedGroups={},const std::string& allowedOwner={}){
    if(!source.empty()&&!ContentSource::Valid(source))throw std::runtime_error("Invalid cache source selection");
    Safe(file);const auto size=fs::file_size(file);std::ifstream input(file,std::ios::binary);uint64_t start=0;const auto manifest=Header(input,start,size);
    HelpyCacheSources::Plan originPlan;originPlan.Owners=snapshot.Owners;
    const auto recordScope=[&](const CacheBundle::Record& row){return policy!=ImportPolicy::ServerDisplayOnly&&(source.empty()||source==row.Owner)&&(allowedOwner.empty()||row.Owner==allowedOwner)&&(allowedGroups.empty()||allowedGroups.contains(row.Source))&&CacheBundleStore::ActiveOwner(row,snapshot);};
    if(!manifest.ContentOwner.empty()){
        const auto plan=CacheBundleStore::Plan(snapshot);
        if(!HelpyCacheSources::ActiveOwners(plan).contains(manifest.ContentOwner.substr(5))||(!source.empty()&&source!=manifest.ContentOwner)||(!allowedOwner.empty()&&allowedOwner!=manifest.ContentOwner))throw std::runtime_error("[RS-CACHE-008] Cache owner is inactive or outside the selected mod");
    }
    std::map<std::string,std::set<std::string>> borrowed;
    for(const auto& row:manifest.Records)if(recordScope(row)&&CacheBundle::OriginalOwnerProven(row,CacheBundleStore::Plan(snapshot),snapshot.NativePlugins)){
        const auto icon=row.Data.value("Icon",std::string{}),group=HelpyCacheSources::Source(icon);
        if(!icon.empty()&&(group=="game"||group=="engine"||snapshot.NativePlugins.contains(group)))borrowed[icon].insert(row.Source);
    }
    const auto inScope=[&](const Entry& entry){return ((allowedGroups.empty()||allowedGroups.contains(entry.Source))&&(source.empty()||(entry.Origins.empty()?ContentSource::Origins(entry.Icon,originPlan):entry.Origins).contains(source)))||borrowed.contains(entry.Icon);};
    std::set<std::string> wanted;for(const auto& entry:manifest.Icons)if(inScope(entry)&&snapshot.SourceKeys.contains(entry.Source))wanted.insert(entry.Source);
    if(manifest.Locale==CacheBundleStore::Locale())for(const auto& row:manifest.Records)if(recordScope(row))wanted.insert(row.Source);
    if(wanted.empty())return {0,manifest.Icons.size()+manifest.Records.size(),0,"[RS-CACHE-003] No matching active sources or locale. Nothing imported."};
    // A pure native bundle matches the installed native cook independently of
    // optional mods. Mixed/mod bundles retain their owner-content fingerprints.
    const auto nativeGroup=[&](const std::string& group){return group=="game"||group=="engine"||snapshot.NativePlugins.contains(group);};
    const auto nativeOwner=[&](const std::string& owner){return owner=="core"||(owner.starts_with("plugins/")&&snapshot.NativePlugins.contains(owner.substr(8)));};
    const bool nativeBundle=std::all_of(manifest.Sources.items().begin(),manifest.Sources.items().end(),[&](const auto& entry){return nativeGroup(entry.key());})
        &&std::all_of(manifest.Records.begin(),manifest.Records.end(),[&](const auto& row){return nativeOwner(row.Owner)&&(row.Owner!="core"||row.Source=="game"||row.Source=="engine");})
        &&std::all_of(manifest.Icons.begin(),manifest.Icons.end(),[&](const auto& icon){return !icon.Origins.empty()&&std::all_of(icon.Origins.begin(),icon.Origins.end(),nativeOwner);});
    const bool baselineArtwork=nativeBundle&&!snapshot.DeclaredOwners.empty();
    std::set<std::string> matches;
    if(policy==ImportPolicy::ServerDisplayOnly){
        // Client and server cooks can legitimately differ or omit textures.
        // This opt-in imports pictures by exact path for ACTIVE source groups
        // only; it cannot add definitions, validate spawnability or grant rights.
        HelpyCacheSources::Plan plan;plan.Authoritative=snapshot.Authoritative;plan.Revision=snapshot.PlanRevision;plan.Owners=snapshot.Owners;plan.DeclaredOwners=snapshot.DeclaredOwners;
        for(const auto& [group,key]:snapshot.SourceKeys)plan.Registered.insert(group);
        Check(progress);if(!snapshot.Authoritative||HelpySourceCache::Inspect(root,plan).Key!=snapshot.Key)throw std::runtime_error("Content changed. Refresh sources before importing display-only icons.");
        matches=wanted;
    }else{
        const auto keys=Portable(root,snapshot,wanted,cache,progress,nativeBundle,manifest.ContentOwner);
        for(const auto& [group,key]:keys)if(manifest.Sources.at(group)==key)matches.insert(group);
    }
    // Borrowed native artwork is admitted only with a matching owning module,
    // never merely because the pack claims a native icon as its own.
    for(auto it=borrowed.begin();it!=borrowed.end();)if(std::none_of(it->second.begin(),it->second.end(),[&](const auto& group){return matches.contains(group);}))it=borrowed.erase(it);else ++it;
    const bool nativeFallback=policy==ImportPolicy::NativeArtworkFallback&&nativeBundle&&matches.empty();
    const auto pictureMatch=[&](const Entry& entry){
        if(!inScope(entry))return false;if(matches.contains(entry.Source))return true;
        if(!nativeFallback||!snapshot.SourceKeys.contains(entry.Source)||!snapshot.ContainerAssets)return false;
        const auto found=snapshot.ContainerAssets->find(HelpyCacheSources::AssetKey(entry.Icon));
        return found!=snapshot.ContainerAssets->end()&&found->second.size()==1&&nativeOwner(*found->second.begin());
    };
    uint64_t total=0;for(const auto& e:manifest.Icons)if(pictureMatch(e))total+=e.Bytes;
    uint64_t existing=0;Safe(cache);if(fs::exists(cache)){size_t inspected=0;for(auto it=fs::recursive_directory_iterator(cache);it!=fs::recursive_directory_iterator();++it){Check(progress);Safe(it->path());if(++inspected>MaxIcons*8)throw std::runtime_error("Cache has too many files; clean old caches first");if(it->is_regular_file()&&it->path().extension()==L".rsicon")existing+=it->file_size();}}
    for(const auto& e:manifest.Icons)if(pictureMatch(e)){const auto target=Target(cache,snapshot,e.Icon);Safe(target);if(fs::is_regular_file(target))existing-=std::min(existing,fs::file_size(target));}
    if(total>budget||existing>budget-total)throw std::runtime_error("[RS-CACHE-005] Matched icons exceed your cache limit. Increase the limit or clean old caches before importing.");
    // Validate ALL pixel blobs before publishing any of them, even skipped mods.
    progress.Begin(Phase::Validate,manifest.Icons.size());
    for(const auto& e:manifest.Icons){Check(progress);(void)Blob(input,e,start);++progress.Done;}
    progress.Begin(Phase::Write,manifest.Icons.size(),total);Result result;
    for(const auto& e:manifest.Icons){Check(progress);if(pictureMatch(e)){const auto target=Target(cache,snapshot,e.Icon);const auto bytes=Blob(input,e,start);
        bool unchanged=false;try{unchanged=Read(target,16+256*256*4)==bytes;}catch(...){}
        if(unchanged)++result.Unchanged;else{Publish(target,bytes);++result.Imported;result.ChangedIcons.push_back(e.Icon);}
        Index(target,e.Icon,e.Origins,policy==ImportPolicy::ServerDisplayOnly||baselineArtwork||nativeFallback);progress.BytesDone+=bytes.size();}else ++result.Skipped;++progress.Done;}
    std::vector<CacheBundle::Record> records;
    for(const auto& row:manifest.Records){if(manifest.Locale==CacheBundleStore::Locale()&&recordScope(row)&&matches.contains(row.Source))records.push_back(row);else ++result.Skipped;}
    // Commit metadata only after every blob passed validation and publication.
    // Cancellation before this commit never replaces the last-good index.
    if(!records.empty()){Check(progress);progress.Stage=Phase::Commit;result.RecordsImported=CacheBundleStore::Commit(cache,root,snapshot,manifest.Locale,records,[&]{Check(progress);});}
    progress.Stage=Phase::Complete;result.Message="Imported "+std::to_string(result.Imported)+" new/changed icons; kept "+std::to_string(result.Unchanged)+" unchanged; skipped "+std::to_string(result.Skipped)+" from missing or changed sources.";
    if(policy==ImportPolicy::ServerDisplayOnly)result.Message+=" Display-only thumbnails; live item definitions and permissions are unchanged.";
    if(baselineArtwork)result.Message+=" Base-game artwork references; installed mods may override presentation. Not relabelled as mod-verified artwork.";
    if(manifest.Version==1)result.Message+=" [RS-CACHE-010] Legacy pixels-only pack; no item or journal identities included.";
    else result.Message+=" "+std::to_string(result.RecordsImported)+" new/updated path and persistence-ID references routed to their content owners. References are not live availability.";
    if(nativeFallback)result.Message+=" Different game cook: native artwork reused as display references for existing native paths only. Item identities/IDs were not imported; live discovery supplies current data.";
    if(manifest.Coverage.is_object()&&!manifest.Coverage.value("Complete",false))result.Message+=" Warning: this pack has partial coverage ("+std::to_string(manifest.Icons.size())+" / "+std::to_string(manifest.Coverage.value("Expected",size_t{}))+"). Missing icons will be captured on demand.";
    return result;
}
inline Result Export(const fs::path& file,const fs::path& root,const fs::path& cache,const HelpySourceCache::Snapshot& snapshot,Progress& progress,const std::string& source={},const nlohmann::json& coverage=nlohmann::json::object(),const nlohmann::json& references=nlohmann::json::array()){
    if(!source.empty()&&!ContentSource::Valid(source))throw std::runtime_error("Invalid cache source selection");
    const bool surveyed=!coverage.empty();const auto expected=surveyed?IconCacheCoverage::Expected(coverage,source,nlohmann::json(snapshot.SourceKeys)):std::set<std::string>{};
    const auto plan=CacheBundleStore::Plan(snapshot);const bool targeted=source.starts_with("mods/");
    const bool bundle=HelpyCacheSources::Lower(file.extension().string())==".rsmk-cache";
    auto records=nlohmann::json::array();std::set<std::string> referenced;size_t unproven=0;
    for(const auto& row:CacheBundle::Records(references))if((source.empty()||row.Owner==source)&&CacheBundleStore::ActiveOwner(row,snapshot)){
        if(targeted&&!CacheBundle::OriginalOwnerProven(row,plan,snapshot.NativePlugins)){++unproven;continue;}
        records.push_back(CacheBundle::Document(row));const auto icon=row.Data.value("Icon",std::string{});if(!icon.empty())referenced.insert(icon);
    }
    progress.Begin(Phase::Discover);Safe(cache);std::map<std::string,fs::path> icons;std::map<std::string,std::set<std::string>> origins;size_t inspected=0;
    if(fs::exists(cache))for(auto it=fs::recursive_directory_iterator(cache);it!=fs::recursive_directory_iterator();++it){Check(progress);if(++inspected>MaxIcons*8)throw std::runtime_error("Cache has too many files; clean old caches first");
        progress.Done=inspected;Safe(it->path());if(!it->is_regular_file()||it->path().extension()!=L".rsicon")continue;
        try{const auto index=Json(Read(it->path().wstring()+L".json",8192));const auto icon=index.at("Icon").get<std::string>();
            // Never relabel a cross-cook visual as content-verified on export.
            if(index.value("DisplayOnly",false))continue;
            std::set<std::string> values;
            if(index.contains("Origins")&&index["Origins"].is_array()&&index["Origins"].size()<=128)for(const auto& value:index["Origins"])if(value.is_string()&&ContentSource::Valid(value.get<std::string>()))values.insert(value.get<std::string>());
            if(values.empty()){HelpyCacheSources::Plan plan;plan.Owners=snapshot.Owners;values=ContentSource::Origins(icon,plan);}
            const auto group=HelpyCacheSources::Source(icon);const bool native=group=="game"||group=="engine"||snapshot.NativePlugins.contains(group);
            const CacheBundle::Record art{"art",source,group,icon,"",nlohmann::json::object()};
            const bool owned=!targeted||CacheBundle::OriginalOwnerProven(art,plan,snapshot.NativePlugins);
            const bool dependency=bundle&&targeted&&native&&referenced.contains(icon);
            const bool selected=source.empty()||(targeted?(owned||dependency):values.contains(source));
            if(Target(cache,snapshot,icon)==it->path()&&(surveyed?expected.contains(icon)&&(!targeted||owned||dependency):selected)){
                if(dependency)values={group=="game"||group=="engine"?"core":"plugins/"+group};else if(targeted&&owned)values={source};icons[icon]=it->path();origins[icon]=std::move(values);}}catch(...){}
    }
    if(!bundle)records=nlohmann::json::array();
    if(icons.empty()&&records.empty())throw std::runtime_error("[RS-CACHE-007] No cached presentation records or indexed icons to export yet.");
    if(icons.size()>MaxIcons)throw std::runtime_error("Too many icons for one pack");std::set<std::string> wanted;for(const auto& [icon,path]:icons)wanted.insert(HelpyCacheSources::Source(icon));
    for(const auto& row:records)wanted.insert(row.at("Source").get<std::string>());
    const auto contentOwner=bundle&&targeted?source:std::string{};
    const auto keys=Portable(root,snapshot,wanted,cache,progress,false,contentOwner);nlohmann::json document={{"Version",bundle?2:1},{"Sources",keys},{"Icons",nlohmann::json::array()}};
    if(!contentOwner.empty())document["ContentOwner"]=contentOwner;
    if(bundle){
        auto verified=nlohmann::json::array();for(const auto& row:records)if(keys.contains(row.at("Source").get<std::string>()))verified.push_back(row);
        document["Kind"]="RuneSchemaCacheBundle";document["Locale"]=CacheBundleStore::Locale();document["Records"]=std::move(verified);
        document["RecordCoverage"]={{"Complete",false},{"ExcludedUnprovenOwner",unproven},{"Scope",source.empty()?"all observed cached references":source},{"Note","Observed references only; missing persistence IDs are unknown"}};
    }
    uint64_t payload=0;std::vector<std::pair<fs::path,std::string>> selected;progress.Begin(Phase::Validate,icons.size());
    std::set<std::string> exported;auto warnings=nlohmann::json::array();
    for(const auto& [icon,path]:icons){Check(progress);const auto group=HelpyCacheSources::Source(icon);if(keys.contains(group)){
        // A corrupt thumbnail should not discard every good one. Record the
        // exact missing path and continue; only validated pixels enter a pack.
        std::vector<char> bytes;try{bytes=Read(path,16+256*256*4);Pixels(bytes);}catch(const std::exception& e){if(warnings.size()<64)warnings.push_back({{"Path",icon},{"Reason",e.what()}});++progress.Done;continue;}
        payload+=bytes.size();if(payload>MaxPack-MaxManifest-12)throw std::runtime_error("Icon pack exceeds 512 MB");
        document["Icons"].push_back({{"Icon",icon},{"Source",group},{"SHA256",Hash(bytes)},{"Bytes",bytes.size()},{"Origins",origins[icon]}});selected.emplace_back(path,Hash(bytes));exported.insert(icon);}++progress.Done;}
    auto missing=nlohmann::json::array();for(const auto& icon:expected)if(!exported.contains(icon))missing.push_back(icon);
    const bool full=surveyed&&IconCacheCoverage::Complete(coverage)&&missing.empty();
    document["Coverage"]={{"Complete",full},{"Surveyed",surveyed},{"Expected",surveyed?expected.size():icons.size()},{"Exported",selected.size()},
        {"Missing",missing},{"Scope",source.empty()?"all registered icon sources":source},{"Kind",full?"Verified full build":"Partial cache"},
        {"DiscoveryFailed",coverage.value("DiscoveryFailed",size_t{})},{"CaptureFailed",coverage.value("Failed",size_t{})},
        {"BuildIssues",coverage.value("Issues",nlohmann::json::array())},{"ExportIssues",warnings}};
    if(selected.empty()&&(!bundle||document["Records"].empty()))throw std::runtime_error("[RS-CACHE-003] No cached content has verifiable active sources");const auto text=document.dump();if(text.size()>MaxManifest)throw std::runtime_error("[RS-CACHE-005] Cache manifest is too large");
    Safe(file);fs::create_directories(file.parent_path());const auto temporary=file.wstring()+L".pack-"+std::to_wstring(GetCurrentProcessId())+L".tmp";Safe(temporary);
    progress.Begin(Phase::Write,selected.size(),payload);
    try{std::ofstream output(fs::path(temporary),std::ios::binary|std::ios::trunc);const uint32_t length=uint32_t(text.size());output.write(bundle?BundleMagic:Magic,8);output.write(reinterpret_cast<const char*>(&length),4);output.write(text.data(),text.size());
        for(const auto& [path,digest]:selected){Check(progress);const auto bytes=Read(path,16+256*256*4);if(Hash(bytes)!=digest)throw std::runtime_error("Cache changed during export; retry later");output.write(bytes.data(),bytes.size());progress.BytesDone+=bytes.size();++progress.Done;}
        output.close();if(!output)throw std::runtime_error("Icon pack export failed");Check(progress);progress.Stage=Phase::Commit;Safe(file);if(!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Icon pack export could not be saved");
    }catch(...){std::error_code ec;fs::remove(temporary,ec);throw;}
    progress.Stage=Phase::Complete;Result result;result.Exported=selected.size();result.Message=(full?"Full cache exported: ":"Exported with warnings: ")+std::to_string(result.Exported)+(surveyed?" / "+std::to_string(expected.size())+" icons.":" icons; coverage has not been scanned.");
    if(bundle){result.RecordsExported=document["Records"].size();result.Message+=" "+std::to_string(result.RecordsExported)+" typed records with exact paths and known persistence IDs. Record coverage is partial, not a full-game dump.";}
    if(unproven)result.Message+=" "+std::to_string(unproven)+" references excluded: exact module ownership is unproven.";
    if(!full){if(surveyed)result.Message+=" Missing "+std::to_string(missing.size())+"; discovery failures "+std::to_string(coverage.value("DiscoveryFailed",size_t{}))+".";
        result.Message+=" Coverage details are included in the pack.";}return result;
}
}
