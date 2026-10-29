#include "Core/ProcessExitFuture.h"
#include "Runtime/HelpyDownloads.h"
#include "Runtime/HelpyPaths.h"
#include "Runtime/HostServices.h"
#include "Core/HelpyUiAssets.h"
#include "Core/HelpyBranding.h"
#include "Core/BaseCacheDownloads.h"
#include "Core/HelpySourceCache.h"
#include "Core/MapPresentationCache.h"
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <functional>
#include <stdexcept>
#include <vector>
#pragma comment(lib,"bcrypt.lib")
namespace PS::HelpyDownloads {
namespace {
using Bytes=std::vector<char>;
struct Result {bool Artwork=false,NeedsArtwork=false,Dismissed=false;std::string Status;std::filesystem::path Pack;bool ApplyArtworkState=false;std::filesystem::path AuthorTools;};
View State;PS::ProcessExitFuture<Result> Worker;std::atomic<bool> Cancelled=false;
std::atomic<uint64_t> DownloadedBytes=0,DownloadBytes=0;
std::atomic<unsigned> Done=0,Count=0;bool Initialized=false;
std::filesystem::path PendingPack;std::chrono::steady_clock::time_point NextAttempt{};
std::filesystem::path Folder(){return HostServices::StateDirectory()/"ui-assets"/HelpyUiAssets::Version;}
void Safe(const std::filesystem::path& path){
    for(auto current=std::filesystem::absolute(path).lexically_normal();!current.empty();){
        const auto attributes=GetFileAttributesW(HelpySourceCache::NativePath(current).c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Download folders cannot contain links.");
        const auto parent=current.parent_path();if(parent==current)break;current=parent;
    }
}
std::string Hash(const Bytes& bytes){
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    struct Cleanup{BCRYPT_ALG_HANDLE& A;BCRYPT_HASH_HANDLE& H;~Cleanup(){if(H)BCryptDestroyHash(H);if(A)BCryptCloseAlgorithmProvider(A,0);}} cleanup{algorithm,hash};
    std::array<unsigned char,32> digest{};
    if(bytes.size()>ULONG_MAX||BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0
        ||BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0
        ||BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),static_cast<ULONG>(bytes.size()),0)<0
        ||BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)<0)throw std::runtime_error("Download SHA-256 verification failed.");
    constexpr char digits[]="0123456789abcdef";std::string result;for(const auto c:digest){result+=digits[c>>4];result+=digits[c&15];}return result;
}
Bytes Read(const std::filesystem::path& path,size_t limit){
    Safe(path);const auto native=HelpySourceCache::NativePath(path);if(!std::filesystem::is_regular_file(native)||std::filesystem::file_size(native)>limit)return {};
    const auto size=std::filesystem::file_size(native);std::ifstream input(native,std::ios::binary);Bytes bytes(static_cast<size_t>(size));
    input.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    if(!input||input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Cached download changed while being read.");return bytes;
}
bool Verified(const std::filesystem::path& path,size_t size,const std::string& digest){
    try{const auto bytes=Read(path,size);return bytes.size()==size&&Hash(bytes)==digest;}catch(...){return false;}
}
bool ArtworkReady(const std::filesystem::path& folder){
    for(const auto& file:HelpyUiAssets::Files)if(!Verified(folder/file.Name,file.Bytes,file.SHA256))return false;return true;
}
struct Handle {HINTERNET Value{};~Handle(){if(Value)WinHttpCloseHandle(Value);}operator HINTERNET()const{return Value;}};
Bytes Fetch(const std::string& relative,size_t limit){
    if(Cancelled)throw std::runtime_error("Download cancelled.");
    // Callers supply compiled artwork names or validated content-addressed names.
    // UI images have compiled hashes; the data-only cache catalogue updates
    // independently of DLL releases. Import still verifies digest and scope.
    const auto ref=relative.starts_with("cache-bundles/")||relative.starts_with("tools/authoring-tools/")?"main":"86c8c20ca6a8fb696781669179b3ef942114bf08";
    const std::string path=std::string("/gh0sted5456-us/RuneSchema/")+ref+"/resources/"+relative;
    const std::wstring wide(path.begin(),path.end());
    Handle session{WinHttpOpen(L"RuneSchema-Helpy-Assets/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session||!WinHttpSetTimeouts(session,2000,2000,2000,2000))throw std::runtime_error("Download network session unavailable.");
    Handle connection{WinHttpConnect(session,L"raw.githubusercontent.com",INTERNET_DEFAULT_HTTPS_PORT,0)};
    Handle request{connection?WinHttpOpenRequest(connection,L"GET",wide.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):nullptr};
    DWORD disabled=WINHTTP_DISABLE_REDIRECTS;
    if(!request||!WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled))
        ||!WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request,nullptr))throw std::runtime_error("GitHub download failed or timed out. Cached files are retained.");
    DWORD status=0,length=sizeof(status);
    if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX)||status!=200)
        throw std::runtime_error(status==404?"This asset/cache pack has not been published on GitHub yet. Helpy remains usable offline.":"GitHub download returned HTTP "+std::to_string(status));
    DownloadedBytes=0;DownloadBytes=0;DWORD contentLength=0,contentSize=sizeof(contentLength);
    if(WinHttpQueryHeaders(request,WINHTTP_QUERY_CONTENT_LENGTH|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&contentLength,&contentSize,WINHTTP_NO_HEADER_INDEX)){if(contentLength>limit)throw std::runtime_error("Download exceeds its size limit.");DownloadBytes=contentLength;}
    Bytes bytes;std::array<char,16384> buffer{};const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(120);
    for(;;){
        if(Cancelled||std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Download cancelled or timed out; existing files retained.");
        DWORD count=0;if(!WinHttpReadData(request,buffer.data(),static_cast<DWORD>(buffer.size()),&count))throw std::runtime_error("Download interrupted; existing files retained.");
        if(!count)break;if(bytes.size()+count>limit)throw std::runtime_error("Download exceeds its size limit.");bytes.insert(bytes.end(),buffer.data(),buffer.data()+count);DownloadedBytes=bytes.size();
    }return bytes;
}
void Commit(const std::filesystem::path& path,const Bytes& bytes){
    Safe(path);std::filesystem::create_directories(HelpySourceCache::NativePath(path.parent_path()));Safe(path);
    static std::atomic<uint64_t> stagingSequence{0};
    const auto temporary=path.parent_path()/(path.filename().wstring()+L"."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64())+L"."+std::to_wstring(++stagingSequence)+L".tmp");Safe(temporary);
    const auto nativeTemporary=HelpySourceCache::NativePath(temporary),nativeTarget=HelpySourceCache::NativePath(path);
    HANDLE file=CreateFileW(nativeTemporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot stage download; an unfinished staging file may remain.");
    DWORD written=0;const bool ok=bytes.size()<=MAXDWORD&&WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(file);CloseHandle(file);
    if(!ok||!MoveFileExW(nativeTemporary.c_str(),nativeTarget.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        DeleteFileW(nativeTemporary.c_str());throw std::runtime_error("Download commit failed; existing file retained.");
    }
}
using Fetcher=std::function<Bytes(const std::string&,size_t)>;
Result InstallArtwork(const std::filesystem::path& folder,const Fetcher& fetch){
    Count=static_cast<unsigned>(HelpyUiAssets::Files.size());Done=0;
    for(const auto& file:HelpyUiAssets::Files){
        if(Cancelled)throw std::runtime_error("Artwork download cancelled.");
        if(!Verified(folder/file.Name,file.Bytes,file.SHA256)){
            const auto bytes=fetch(std::string("helpy-ui/v1/")+file.Name,file.Bytes);
            if(bytes.size()!=file.Bytes||Hash(bytes)!=file.SHA256)throw std::runtime_error("Artwork integrity check failed; previous artwork retained.");
            Commit(folder/file.Name,bytes);
        }++Done;
    }
    Commit(folder/"ready",Bytes{'1'});return {true,false,true,"UI artwork ready. Saved locally for future launches.",{},true};
}
bool Hex(const std::string& text,size_t size){return text.size()==size&&text.find_first_not_of("0123456789abcdef")==text.npos;}
Result DownloadWorldMap(const std::filesystem::path& cache,const std::filesystem::path& downloads,const Fetcher& fetch){
    Count=2;Done=0;const auto bytes=fetch("cache-bundles/world-map.json",16*1024);
    const auto index=nlohmann::json::parse(bytes,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){if(depth>12)throw std::runtime_error("[RS-DL-002] Map manifest nesting exceeds its limit.");return true;});
    if(!index.is_object()||index.value("Format",0)!=1||index.value("Kind",std::string{})!="RuneSchemaWorldMapCache")throw std::runtime_error("[RS-DL-002] Invalid world-map download manifest.");
    const auto digest=index.value("SHA256",std::string{}),name=index.value("File",std::string{});const auto size=index.value("Bytes",uint64_t{});
    if(!Hex(digest,64)||name!=digest+".rsmk-cache"||size<20||size>24ull*1024*1024)throw std::runtime_error("[RS-DL-002] Invalid world-map download bounds.");
    const auto metadata=MapPresentationCache::Metadata(index.at("WorldMap"));++Done;
    try{const auto current=MapPresentationCache::Reference(cache);
        if(!current.empty()&&MapPresentationCache::Metadata(current)==metadata){(void)MapPresentationCache::Bytes(cache,current);++Done;
            return {false,false,false,"World map is already current; local artwork and alignment reused."};}
    }catch(...){} // A damaged local copy can be replaced by a verified download.
    const auto target=downloads/name;
    if(!Verified(target,static_cast<size_t>(size),digest)){
        const auto pack=fetch("cache-bundles/"+name,static_cast<size_t>(size));
        if(pack.size()!=size||Hash(pack)!=digest)throw std::runtime_error("[RS-CACHE-004] World-map cache checksum mismatch.");
        if(Cancelled)throw std::runtime_error("Map download cancelled; existing map retained.");Commit(target,pack);
    }
    ++Done;return {false,false,false,"World map downloaded; validating and installing artwork with its alignment...",target};
}
Result InstallAuthorTools(const std::filesystem::path& folder,const Fetcher& fetch){
    Count=2;Done=0;
    const auto bytes=fetch("tools/authoring-tools/index.json",16*1024);
    const auto index=nlohmann::json::parse(bytes,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){if(depth>8)throw std::runtime_error("[RS-DL-002] Authoring-tools index is too deeply nested.");return true;});
    if(!index.is_object()||index.value("Format",0)!=1||index.value("Kind",std::string{})!="RuneSchemaAuthoringTools")throw std::runtime_error("[RS-DL-002] Unsupported authoring-tools index.");
    const auto digest=index.value("SHA256",std::string{}),name=index.value("File",std::string{});const auto size=index.value("Bytes",uint64_t{});
    if(!Hex(digest,64)||name!=digest+".zip"||size<22||size>2*1024*1024)throw std::runtime_error("[RS-DL-002] Invalid authoring-tools filename, size or checksum metadata.");
    ++Done;const auto target=folder/"RuneSchema-Authoring-Tools.zip";
    if(!Verified(target,static_cast<size_t>(size),digest)){
        const auto archive=fetch("tools/authoring-tools/"+name,static_cast<size_t>(size));
        if(archive.size()!=size||Hash(archive)!=digest||archive[0]!='P'||archive[1]!='K'||archive[2]!=3||archive[3]!=4)throw std::runtime_error("[RS-DL-002] Authoring-tools ZIP verification failed; previous download retained.");
        if(Cancelled)throw std::runtime_error("Authoring-tools download cancelled; previous ZIP retained.");
        Commit(target,archive);
    }
    ++Done;Result result;result.AuthorTools=target;result.Status="Authoring tools ZIP ready. Extract it and follow README.md; run the Bash script manually with the target game closed.";return result;
}
Result DownloadBase(const std::map<std::string,std::string>& sources,const std::set<std::string>& nativePlugins,const std::string& runtime,const std::string& gameVersion){
    Count=3;Done=0;
    for(const bool bundle:{true,false}){
    const std::string directory=bundle?"cache-bundles":"item-icon-caches";Bytes bytes;
    try{bytes=Fetch(directory+"/index.json",256*1024);}catch(const std::exception& e){
        if(bundle&&std::string(e.what()).find("has not been published")!=std::string::npos)continue;throw;
    }
    const auto index=nlohmann::json::parse(bytes,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){if(depth>16)throw std::runtime_error("[RS-DL-002] Cache index nesting exceeds its limit.");return true;});
    if(index.value("Format",0)!=(bundle?2:1)||index.value("Kind",std::string{})!=(bundle?"RuneSchemaCacheBundles":"RuneSchemaBaseGameIcons")||!index.contains("Packs")||!index["Packs"].is_array()||index["Packs"].size()>128)throw std::runtime_error("[RS-DL-002] Invalid base-game cache index.");
    ++Done;
    wchar_t language[LOCALE_NAME_MAX_LENGTH]{};GetUserDefaultLocaleName(language,LOCALE_NAME_MAX_LENGTH);std::string locale;for(const auto c:std::wstring(language))locale+=static_cast<char>(c);
    for(const auto& pack:BaseCacheDownloads::Candidates(index["Packs"],sources,nativePlugins,runtime,gameVersion,locale,bundle)){
        // Machine-local source keys contain timestamps and are not portable.
        // Build/runtime narrows the candidate; pack import independently checks
        // actual game-content SHA-256 before any thumbnails are applied.
        const auto hash=pack.value("SHA256",std::string{});const auto size=pack.value("Bytes",uint64_t{});
        if(!Hex(hash,64)||size<16||size>256ull*1024*1024)throw std::runtime_error("Invalid base-game cache download bounds.");
        const auto name=hash+(bundle?".rsmk-cache":".rsmk-icons");const auto path=HostServices::CacheDirectory()/"downloads"/name;
        if(!Verified(path,static_cast<size_t>(size),hash)){
            const auto data=Fetch(directory+"/"+name,static_cast<size_t>(size));
            if(data.size()!=size||Hash(data)!=hash)throw std::runtime_error("[RS-CACHE-004] Base-game cache integrity check failed.");Commit(path,data);
        }++Done;return {false,false,false,"Download verified; processing cached items and artwork relationships...",path};
    }}throw std::runtime_error("[RS-DL-001] No compatible base-game cache is published for these active sources. Build / update still works locally.");
}
}
const View& Current(){return State;}
bool Busy(){return Worker.valid();}
unsigned Completed(){return Done.load();}unsigned Total(){return Count.load();}
uint64_t BytesCompleted(){return DownloadedBytes.load();}uint64_t BytesTotal(){return DownloadBytes.load();}
void CacheApplied(const std::string& status){State.Status=status;}
void Initialize(){
    if(Initialized)return;Initialized=true;
    try{const auto folder=Folder(),bundled=HelpyPaths::PluginRoot()/"assets";Worker=std::async(std::launch::async,[folder,bundled]{
        Result result;result.ApplyArtworkState=true;result.Dismissed=std::filesystem::is_regular_file(HelpySourceCache::NativePath(folder/"hint-dismissed"));
        bool bundledReady=true;for(const auto& file:HelpyUiAssets::Files)bundledReady=bundledReady&&std::filesystem::is_regular_file(bundled/file.Name);
        result.NeedsArtwork=!ArtworkReady(folder)&&!bundledReady;return result;
    });}catch(...){State.NeedsArtwork=true;State.Status="Artwork inspection unavailable; offline mode is active.";}
}
bool StartArtwork(){
    return StartAssets(true,false);
}
bool StartAssets(bool artwork,bool worldMap){
    if(!artwork&&!worldMap)return false;
    if(Busy()||std::chrono::steady_clock::now()<NextAttempt)return false;NextAttempt=std::chrono::steady_clock::now()+std::chrono::seconds(10);Cancelled=false;
    State.Status="Updating selected Helpy dependencies...";
    try{const auto folder=Folder(),cache=HostServices::CacheDirectory()/"item-icons",downloads=HostServices::CacheDirectory()/"downloads";
        Worker=std::async(std::launch::async,[folder,cache,downloads,artwork,worldMap]{Result result;std::string status;
            if(artwork)try{result=InstallArtwork(folder,Fetch);status=result.Status;}catch(const std::exception& error){status="UI artwork: "+std::string(error.what());}
            if(worldMap)try{auto map=DownloadWorldMap(cache,downloads,Fetch);result.Pack=std::move(map.Pack);if(!status.empty())status+="\n";status+=map.Status;}
                catch(const std::exception& error){if(!status.empty())status+="\n";status+="World map: "+std::string(error.what());}
            result.Status=std::move(status);return result;});return true;}
    catch(...){State.Status="Could not start dependency download; existing files are retained.";return false;}
}
std::filesystem::path AuthorToolsFolder(){return HostServices::ExportsDirectory()/"authoring-tools";}
bool StartAuthorTools(){
    if(Busy())return false;
    if(std::chrono::steady_clock::now()<NextAttempt){State.Status="Please wait a few seconds before retrying the download.";return false;}
    NextAttempt=std::chrono::steady_clock::now()+std::chrono::seconds(10);Cancelled=false;Done=0;Count=2;
    State.Status="Downloading the authoring tools ZIP from GitHub...";
    try{const auto folder=AuthorToolsFolder();Worker=std::async(std::launch::async,[folder]{return InstallAuthorTools(folder,Fetch);});return true;}
    catch(...){State.Status="[RS-DL-001] Could not start the authoring-tools download. The previous ZIP is retained.";return false;}
}
bool StartBaseCache(const std::map<std::string,std::string>& sources,const std::set<std::string>& nativePlugins,const std::string& runtime,const std::string& gameVersion){
    if(Busy()||std::chrono::steady_clock::now()<NextAttempt)return false;NextAttempt=std::chrono::steady_clock::now()+std::chrono::seconds(10);Cancelled=false;
    DownloadedBytes=0;DownloadBytes=0;State.Status="Looking for a base-game cache on GitHub...";
    try{Worker=std::async(std::launch::async,[sources,nativePlugins,runtime,gameVersion]{return DownloadBase(sources,nativePlugins,runtime,gameVersion);});return true;}
    catch(...){State.Status="Could not start cache download; local Build / update remains available.";return false;}
}
bool Poll(){
    if(!Worker.valid()||Worker.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
    try{const auto result=Worker.get();if(!result.Status.empty())State.Status=result.Status;
        if(result.ApplyArtworkState){State.NeedsArtwork=result.NeedsArtwork;State.HintDismissed=State.HintDismissed||result.Dismissed;}
        if(!result.Pack.empty())PendingPack=result.Pack;
        if(!result.AuthorTools.empty())State.AuthorTools=result.AuthorTools;
        return result.Artwork;
    }catch(const std::exception& error){State.Status=error.what();return false;}
}
std::filesystem::path TakeBaseCache(){auto file=std::move(PendingPack);PendingPack.clear();return file;}
bool ReleaseImportedPack(const std::filesystem::path& file) noexcept{
    try{
        const auto root=std::filesystem::absolute(HostServices::CacheDirectory()/"downloads").lexically_normal(),target=std::filesystem::absolute(file).lexically_normal();
        if(target.parent_path()!=root||!Hex(target.stem().string(),64)||(target.extension()!=L".rsmk-cache"&&target.extension()!=L".rsmk-icons"))return false;
        Safe(target);const auto native=HelpySourceCache::NativePath(target);if(!std::filesystem::is_regular_file(native))return false;return DeleteFileW(native.c_str())!=0;
    }catch(...){return false;}
}
void DismissHint(){State.HintDismissed=true;try{Commit(Folder()/"hint-dismissed",Bytes{'1'});}catch(...){};}
void Cancel()noexcept{Cancelled=true;}
void Shutdown()noexcept{Cancelled=true;if(Worker.valid())try{(void)Worker.get();}catch(...){}Initialized=false;PendingPack.clear();State.AuthorTools.clear();}
#if defined(HELPY_ASSET_TEST)
void TestArtwork(const std::filesystem::path& source,const std::filesystem::path& destination){
    Cancelled=false;(void)InstallArtwork(destination,[&](const std::string& name,size_t size){return Read(source/std::filesystem::path(name).filename(),size);});
}
std::filesystem::path TestWorldMap(const std::filesystem::path& source,const std::filesystem::path& cache,const std::filesystem::path& downloads){
    Cancelled=false;return DownloadWorldMap(cache,downloads,[&](const std::string& name,size_t size){return Read(source/std::filesystem::path(name).filename(),size);}).Pack;
}
void TestAuthorTools(const std::filesystem::path& source,const std::filesystem::path& destination,bool cancelBeforeCommit){
    Cancelled=false;(void)InstallAuthorTools(destination,[&](const std::string& name,size_t size){
        auto bytes=Read(source/std::filesystem::path(name).filename(),size);
        if(cancelBeforeCommit&&name.ends_with(".zip"))Cancel();
        return bytes;
    });
}
#endif
}
