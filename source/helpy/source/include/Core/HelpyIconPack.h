#pragma once
#include "Core/HelpySourceCache.h"
#include "Core/HelpyKitFiles.h"
#include "Core/ContentSource.h"
#include <bcrypt.h>
#include <array>
#include <cstring>
#include <optional>
#pragma comment(lib,"bcrypt.lib")

// Portable *pixels only*. No archive extraction, native objects, scripts, item
// definitions or loader mutations. All output names are generated locally.
namespace PS::HelpyIconPack {
namespace fs=std::filesystem;
inline constexpr size_t MaxIcons=32768,MaxManifest=8*1024*1024;
inline constexpr uint64_t MaxPack=512ull*1024*1024;
inline constexpr char Magic[8]={'R','S','I','P','A','C','K','1'};
enum class Phase { Discover,Verify,Validate,Write,Commit,Complete };
struct Progress {
    std::atomic<bool> Cancel{false};std::atomic<size_t> Done{0},Total{0};
    std::atomic<uint64_t> BytesDone{0},BytesTotal{0};std::atomic<Phase> Stage{Phase::Discover};
    void Begin(Phase stage,size_t total=0,uint64_t bytes=0){Done=0;Total=total;BytesDone=0;BytesTotal=bytes;Stage=stage;}
};
inline void Check(Progress& p){if(p.Cancel)throw std::runtime_error("Icon pack operation cancelled");}
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
        bool visual=displayOnly.value_or(false);
        try{if(fs::is_regular_file(index)&&fs::file_size(index)<=8192){std::ifstream input(index);auto old=nlohmann::json::parse(input);
            if(old.value("Icon",std::string{})==icon){if(origins.empty()&&!displayOnly.has_value())return;if(!displayOnly.has_value())visual=old.value("DisplayOnly",false);if(old.contains("Origins")&&old["Origins"].is_array()&&old["Origins"].size()<=128)for(const auto& value:old["Origins"])if(value.is_string()&&ContentSource::Valid(value.get<std::string>()))origins.insert(value.get<std::string>());}}}catch(...){} // Repair a corrupt sidecar, not a corrupt pixel file.
        const auto text=nlohmann::json({{"Icon",icon},{"Origins",origins},{"DisplayOnly",visual}}).dump();Publish(index,{text.begin(),text.end()});}catch(...){}
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
inline std::map<std::string,std::string> Portable(const fs::path& root,const HelpySourceCache::Snapshot& sources,const std::set<std::string>& wanted,const fs::path& cache,Progress& progress){
    HelpyCacheSources::Plan plan;plan.Authoritative=true;plan.Revision=sources.PlanRevision;plan.Owners=sources.Owners;for(const auto& [group,key]:sources.SourceKeys)plan.Registered.insert(group);
    const auto inspected=HelpySourceCache::Inspect(root,plan);
    if(inspected.Key!=sources.Key)throw std::runtime_error("Content changed. Wait for the source list to refresh, then retry the icon pack.");
    std::map<std::string,std::map<std::string,fs::path>> ownerFiles;std::map<std::string,fs::path> baseFiles;
    for(const auto& [name,path]:inspected.FilePaths){const auto owner=HelpyCacheSources::FileOwner(name);
        if(!owner.empty()){const auto marker="/runeschema/mods/";const auto begin=name.find(marker);ownerFiles[owner][name.substr(begin+std::strlen(marker))]=path;}
        else if(ContentSource::NativeContentFile(name))baseFiles[name]=path;
    }
    if(baseFiles.empty())throw std::runtime_error("Game content identity is unavailable; icon packs cannot be matched safely yet");
    const auto memoFile=cache/"portable-digests.json";nlohmann::json memo=nlohmann::json::object();
    try{const auto bytes=Read(memoFile,16*1024*1024);memo=Json(bytes);if(!memo.is_object())memo=nlohmann::json::object();}catch(...){memo=nlohmann::json::object();}
    std::set<fs::path> files;for(const auto& [name,path]:baseFiles)files.insert(path);
    for(const auto& group:wanted)if(sources.SourceKeys.contains(group)){const auto found=sources.Owners.find(group);if(found!=sources.Owners.end())for(const auto& owner:found->second)for(const auto& [name,path]:ownerFiles[owner])files.insert(path);}
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
    Check(progress);try{if(memo.size()<=100000){const auto text=memo.dump();if(text.size()<=16*1024*1024)Publish(memoFile,{text.begin(),text.end()});}}catch(...){}
    Sha256 base; base.Add("rsmk-icon-content-v1\n");for(const auto& [name,path]:baseFiles)base.Add(name+"|"+hashes.at(path)+"\n");const auto baseKey=base.Finish();
    std::map<std::string,std::string> result;
    for(const auto& group:wanted){if(!sources.SourceKeys.contains(group))continue;Sha256 sha;sha.Add(baseKey+"|"+group+"\n");bool complete=true;
        const auto owners=sources.Owners.find(group);if(owners!=sources.Owners.end())for(const auto& owner:owners->second){if(ownerFiles[owner].empty()){complete=false;break;}for(const auto& [name,path]:ownerFiles[owner])sha.Add(name+"|"+hashes.at(path)+"\n");}
        if(complete)result[group]=sha.Finish();
    }
    // Recheck the cheap metadata plan after lengthy reads; an update must not
    // publish old pixels under the identity of new content.
    if(HelpySourceCache::Inspect(root,plan).Key!=sources.Key)throw std::runtime_error("Source changed during verification; retry later");return result;
}
struct Entry {std::string Icon,Source,SHA256;uint32_t Bytes=0;uint64_t Offset=0;std::set<std::string> Origins;};
struct Manifest {nlohmann::json Sources;std::vector<Entry> Icons;};
inline Manifest Parse(const nlohmann::json& value,uint64_t payload){
    if(!value.is_object()||value.value("Version",0)!=1||!value.contains("Sources")||!value["Sources"].is_object()||value["Sources"].size()>512
        ||!value.contains("Icons")||!value["Icons"].is_array()||value["Icons"].empty()||value["Icons"].size()>MaxIcons)throw std::runtime_error("Invalid icon pack manifest");
    Manifest result;result.Sources=value["Sources"];for(const auto& [group,key]:result.Sources.items())if(HelpyCacheSources::Source("/"+group+"/x.x")!=group||!key.is_string()||!Digest(key.get<std::string>()))throw std::runtime_error("Invalid icon pack source");
    uint64_t offset=0;std::set<std::string> paths;
    for(const auto& row:value["Icons"]){Entry e;e.Icon=row.at("Icon").get<std::string>();e.Source=row.at("Source").get<std::string>();e.SHA256=row.at("SHA256").get<std::string>();
        const auto bytes=row.at("Bytes").get<int64_t>();if(!IconPath(e.Icon)||e.Source!=HelpyCacheSources::Source(e.Icon)||!result.Sources.contains(e.Source)||!Digest(e.SHA256)||bytes<20||bytes>16+256*256*4||!paths.insert(e.Icon).second)throw std::runtime_error("Invalid icon pack entry");
        if(row.contains("Origins")){if(!row["Origins"].is_array()||row["Origins"].size()>128)throw std::runtime_error("Invalid icon origin list");for(const auto& origin:row["Origins"]){if(!origin.is_string()||!ContentSource::Valid(origin.get<std::string>()))throw std::runtime_error("Invalid icon origin");e.Origins.insert(origin.get<std::string>());}}
        e.Bytes=uint32_t(bytes);e.Offset=offset;offset+=e.Bytes;if(offset>MaxPack)throw std::runtime_error("Icon pack payload exceeds its limit");result.Icons.push_back(std::move(e));
    }
    if(offset!=payload)throw std::runtime_error("Icon pack payload length mismatch");return result;
}
inline Manifest Header(std::ifstream& input,uint64_t& start,uint64_t size){
    char magic[8]{};uint32_t length=0;input.read(magic,8);input.read(reinterpret_cast<char*>(&length),4);
    if(!input||std::memcmp(magic,Magic,8)||!length||length>MaxManifest||size>MaxPack||size<12ull+length)throw std::runtime_error("Not a supported .rsmk-icons pack");
    std::string text(length,'\0');input.read(text.data(),length);if(!input)throw std::runtime_error("Truncated icon pack manifest");start=12ull+length;return Parse(Json(text),size-start);
}
inline std::vector<char> Blob(std::ifstream& input,const Entry& entry,uint64_t start){
    input.seekg(std::streamoff(start+entry.Offset));std::vector<char> bytes(entry.Bytes);input.read(bytes.data(),bytes.size());if(!input)throw std::runtime_error("Truncated icon pack pixels");Pixels(bytes);
    if(Hash(bytes)!=entry.SHA256)throw std::runtime_error("Icon pack checksum mismatch");return bytes;
}
struct Result {size_t Imported=0,Skipped=0,Exported=0;std::string Message;};
enum class ImportPolicy { VerifiedContent,ServerDisplayOnly };
inline Result Import(const fs::path& file,const fs::path& root,const fs::path& cache,const HelpySourceCache::Snapshot& snapshot,uint64_t budget,Progress& progress,ImportPolicy policy=ImportPolicy::VerifiedContent){
    Safe(file);const auto size=fs::file_size(file);std::ifstream input(file,std::ios::binary);uint64_t start=0;const auto manifest=Header(input,start,size);
    std::set<std::string> wanted;for(const auto& [group,key]:manifest.Sources.items())if(snapshot.SourceKeys.contains(group))wanted.insert(group);
    if(wanted.empty())return {0,manifest.Icons.size(),0,"No matching active sources. Nothing imported."};
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
        const auto keys=Portable(root,snapshot,wanted,cache,progress);
        for(const auto& [group,key]:keys)if(manifest.Sources.at(group)==key)matches.insert(group);
    }
    uint64_t total=0;for(const auto& e:manifest.Icons)if(matches.contains(e.Source))total+=e.Bytes;
    uint64_t existing=0;Safe(cache);if(fs::exists(cache)){size_t inspected=0;for(auto it=fs::recursive_directory_iterator(cache);it!=fs::recursive_directory_iterator();++it){Check(progress);Safe(it->path());if(++inspected>MaxIcons*8)throw std::runtime_error("Cache has too many files; clean old caches first");if(it->is_regular_file()&&it->path().extension()==L".rsicon")existing+=it->file_size();}}
    for(const auto& e:manifest.Icons)if(matches.contains(e.Source)){const auto target=Target(cache,snapshot,e.Icon);Safe(target);if(fs::is_regular_file(target))existing-=std::min(existing,fs::file_size(target));}
    if(total>budget||existing>budget-total)throw std::runtime_error("Matched icons exceed your cache limit. Increase the limit or clean old caches before importing.");
    // Validate ALL pixel blobs before publishing any of them, even skipped mods.
    progress.Begin(Phase::Validate,manifest.Icons.size()*2);
    for(const auto& e:manifest.Icons){Check(progress);(void)Blob(input,e,start);++progress.Done;}
    progress.Begin(Phase::Write,manifest.Icons.size(),total);Result result;
    for(const auto& e:manifest.Icons){Check(progress);if(matches.contains(e.Source)){const auto target=Target(cache,snapshot,e.Icon);const auto bytes=Blob(input,e,start);Publish(target,bytes);Index(target,e.Icon,e.Origins,policy==ImportPolicy::ServerDisplayOnly);progress.BytesDone+=bytes.size();++result.Imported;}else ++result.Skipped;++progress.Done;}
    progress.Stage=Phase::Complete;result.Message="Imported "+std::to_string(result.Imported)+" icons; skipped "+std::to_string(result.Skipped)+" from missing or changed sources.";
    if(policy==ImportPolicy::ServerDisplayOnly)result.Message+=" Display-only thumbnails; live item definitions and permissions are unchanged.";return result;
}
inline Result Export(const fs::path& file,const fs::path& root,const fs::path& cache,const HelpySourceCache::Snapshot& snapshot,Progress& progress,const std::string& source={}){
    if(!source.empty()&&!ContentSource::Valid(source))throw std::runtime_error("Invalid cache source selection");
    progress.Begin(Phase::Discover);Safe(cache);std::map<std::string,fs::path> icons;std::map<std::string,std::set<std::string>> origins;size_t inspected=0;
    if(fs::exists(cache))for(auto it=fs::recursive_directory_iterator(cache);it!=fs::recursive_directory_iterator();++it){Check(progress);if(++inspected>MaxIcons*8)throw std::runtime_error("Cache has too many files; clean old caches first");
        progress.Done=inspected;Safe(it->path());if(!it->is_regular_file()||it->path().extension()!=L".rsicon")continue;
        try{const auto index=Json(Read(it->path().wstring()+L".json",8192));const auto icon=index.at("Icon").get<std::string>();
            // Never relabel a cross-cook visual as content-verified on export.
            if(index.value("DisplayOnly",false))continue;
            std::set<std::string> values;
            if(index.contains("Origins")&&index["Origins"].is_array()&&index["Origins"].size()<=128)for(const auto& value:index["Origins"])if(value.is_string()&&ContentSource::Valid(value.get<std::string>()))values.insert(value.get<std::string>());
            if(values.empty()){HelpyCacheSources::Plan plan;plan.Owners=snapshot.Owners;values=ContentSource::Origins(icon,plan);}
            if(Target(cache,snapshot,icon)==it->path()&&(source.empty()||values.contains(source))){icons[icon]=it->path();origins[icon]=std::move(values);}}catch(...){}
    }
    if(icons.empty())throw std::runtime_error("No indexed icons to export yet. Browse items once to index older cached icons.");
    if(icons.size()>MaxIcons)throw std::runtime_error("Too many icons for one pack");std::set<std::string> wanted;for(const auto& [icon,path]:icons)wanted.insert(HelpyCacheSources::Source(icon));
    const auto keys=Portable(root,snapshot,wanted,cache,progress);nlohmann::json document={{"Version",1},{"Sources",keys},{"Icons",nlohmann::json::array()}};
    uint64_t payload=0;std::vector<std::pair<fs::path,std::string>> selected;progress.Begin(Phase::Validate,icons.size());
    for(const auto& [icon,path]:icons){Check(progress);const auto group=HelpyCacheSources::Source(icon);if(keys.contains(group)){const auto bytes=Read(path,16+256*256*4);Pixels(bytes);payload+=bytes.size();if(payload>MaxPack-MaxManifest-12)throw std::runtime_error("Icon pack exceeds 512 MB");
        document["Icons"].push_back({{"Icon",icon},{"Source",group},{"SHA256",Hash(bytes)},{"Bytes",bytes.size()},{"Origins",origins[icon]}});selected.emplace_back(path,Hash(bytes));}++progress.Done;}
    if(selected.empty())throw std::runtime_error("No icons have verifiable active sources");const auto text=document.dump();if(text.size()>MaxManifest)throw std::runtime_error("Icon pack manifest is too large");
    Safe(file);fs::create_directories(file.parent_path());const auto temporary=file.wstring()+L".pack-"+std::to_wstring(GetCurrentProcessId())+L".tmp";Safe(temporary);
    progress.Begin(Phase::Write,selected.size(),payload);
    try{std::ofstream output(fs::path(temporary),std::ios::binary|std::ios::trunc);const uint32_t length=uint32_t(text.size());output.write(Magic,8);output.write(reinterpret_cast<const char*>(&length),4);output.write(text.data(),text.size());
        for(const auto& [path,digest]:selected){Check(progress);const auto bytes=Read(path,16+256*256*4);if(Hash(bytes)!=digest)throw std::runtime_error("Cache changed during export; retry later");output.write(bytes.data(),bytes.size());progress.BytesDone+=bytes.size();++progress.Done;}
        output.close();if(!output)throw std::runtime_error("Icon pack export failed");Check(progress);progress.Stage=Phase::Commit;Safe(file);if(!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Icon pack export could not be saved");
    }catch(...){std::error_code ec;fs::remove(temporary,ec);throw;}
    progress.Stage=Phase::Complete;Result result;result.Exported=selected.size();result.Message="Exported "+std::to_string(result.Exported)+" icons. Share the .rsmk-icons file as an optional visual cache.";return result;
}
}
