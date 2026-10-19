#include "Runtime/HelpyDownloads.h"
#include "Runtime/HelpyPaths.h"
#include "Runtime/HostServices.h"
#include "Core/HelpyUiAssets.h"
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
struct Result {bool Artwork=false,NeedsArtwork=false,Dismissed=false;std::string Status;std::filesystem::path Pack;bool ApplyArtworkState=false;};
View State;std::future<Result> Worker;std::atomic<bool> Cancelled=false;
std::atomic<unsigned> Done=0,Count=0;bool Initialized=false;
std::filesystem::path PendingPack;std::chrono::steady_clock::time_point NextAttempt{};
std::filesystem::path Folder(){return HostServices::StateDirectory()/"ui-assets"/HelpyUiAssets::Version;}
void Safe(const std::filesystem::path& path){
    for(auto current=std::filesystem::absolute(path).lexically_normal();!current.empty();){
        const auto attributes=GetFileAttributesW(current.c_str());
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
    Safe(path);if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>limit)return {};
    const auto size=std::filesystem::file_size(path);std::ifstream input(path,std::ios::binary);Bytes bytes(static_cast<size_t>(size));
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
    // Callers supply only compiled artwork names or a validated hex pack name.
    const std::string path="/gh0sted5456-us/RuneSchema/main/resources/"+relative;
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
    Bytes bytes;std::array<char,16384> buffer{};const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(120);
    for(;;){
        if(Cancelled||std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Download cancelled or timed out; existing files retained.");
        DWORD count=0;if(!WinHttpReadData(request,buffer.data(),static_cast<DWORD>(buffer.size()),&count))throw std::runtime_error("Download interrupted; existing files retained.");
        if(!count)break;if(bytes.size()+count>limit)throw std::runtime_error("Download exceeds its size limit.");bytes.insert(bytes.end(),buffer.data(),buffer.data()+count);
    }return bytes;
}
void Commit(const std::filesystem::path& path,const Bytes& bytes){
    Safe(path);std::filesystem::create_directories(path.parent_path());Safe(path);
    const auto temporary=path.parent_path()/(path.filename().wstring()+L"."+std::to_wstring(GetCurrentProcessId())+L".tmp");Safe(temporary);
    HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot stage download; an unfinished staging file may remain.");
    DWORD written=0;const bool ok=bytes.size()<=MAXDWORD&&WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(file);CloseHandle(file);
    if(!ok||!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        DeleteFileW(temporary.c_str());throw std::runtime_error("Download commit failed; existing file retained.");
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
    for(const auto& pack:index["Packs"]){
        // Machine-local source keys contain timestamps and are not portable.
        // Build/runtime narrows the candidate; pack import independently checks
        // actual game-content SHA-256 before any thumbnails are applied.
        if(gameVersion.empty()||pack.value("GameVersion",std::string{})!=gameVersion||pack.value("Runtime",std::string{})!=runtime)continue;
        if(bundle){
            wchar_t language[LOCALE_NAME_MAX_LENGTH]{};GetUserDefaultLocaleName(language,LOCALE_NAME_MAX_LENGTH);std::string locale;for(const auto c:std::wstring(language))locale+=static_cast<char>(c);
            if(pack.value("Locale",std::string{})!=locale)continue;
        }
        if(!pack.is_object()||!pack.contains("Sources")||!pack["Sources"].is_object()||pack["Sources"].empty()||pack["Sources"].size()>512||!pack["Sources"].contains("game"))continue;
        bool matches=true;for(const auto& [source,key]:pack["Sources"].items()){
            const auto found=sources.find(source);
            if((source!="game"&&source!="engine"&&!nativePlugins.contains(source))||!key.is_string()||!Hex(key.get<std::string>(),64)||found==sources.end())matches=false;
        }if(!matches)continue;
        const auto hash=pack.value("SHA256",std::string{});const auto size=pack.value("Bytes",uint64_t{});
        if(!Hex(hash,64)||size<16||size>256ull*1024*1024)throw std::runtime_error("Invalid base-game cache download bounds.");
        const auto name=hash+(bundle?".rsmk-cache":".rsmk-icons");const auto path=HostServices::CacheDirectory()/"downloads"/name;
        if(!Verified(path,static_cast<size_t>(size),hash)){
            const auto data=Fetch(directory+"/"+name,static_cast<size_t>(size));
            if(data.size()!=size||Hash(data)!=hash)throw std::runtime_error("[RS-CACHE-004] Base-game cache integrity check failed.");Commit(path,data);
        }++Done;return {false,false,false,"Base-game pack downloaded; checking content compatibility before import.",path};
    }}throw std::runtime_error("[RS-DL-001] No compatible base-game cache is published for these active sources. Build / update still works locally.");
}
}
const View& Current(){return State;}
bool Busy(){return Worker.valid();}
unsigned Completed(){return Done.load();}unsigned Total(){return Count.load();}
void Initialize(){
    if(Initialized)return;Initialized=true;
    try{const auto folder=Folder(),bundled=HelpyPaths::PluginRoot()/"assets";Worker=std::async(std::launch::async,[folder,bundled]{
        Result result;result.ApplyArtworkState=true;result.Dismissed=std::filesystem::is_regular_file(folder/"hint-dismissed");
        bool bundledReady=true;for(const auto& file:HelpyUiAssets::Files)bundledReady=bundledReady&&std::filesystem::is_regular_file(bundled/file.Name);
        result.NeedsArtwork=!ArtworkReady(folder)&&!bundledReady;return result;
    });}catch(...){State.NeedsArtwork=true;State.Status="Artwork inspection unavailable; offline mode is active.";}
}
bool StartArtwork(){
    if(Busy()||std::chrono::steady_clock::now()<NextAttempt)return false;NextAttempt=std::chrono::steady_clock::now()+std::chrono::seconds(10);Cancelled=false;
    State.Status="Downloading optional UI artwork from GitHub...";
    try{const auto folder=Folder();Worker=std::async(std::launch::async,[folder]{return InstallArtwork(folder,Fetch);});return true;}
    catch(...){State.Status="Could not start artwork download; offline mode remains available.";return false;}
}
bool StartBaseCache(const std::map<std::string,std::string>& sources,const std::set<std::string>& nativePlugins,const std::string& runtime,const std::string& gameVersion){
    if(Busy()||std::chrono::steady_clock::now()<NextAttempt)return false;NextAttempt=std::chrono::steady_clock::now()+std::chrono::seconds(10);Cancelled=false;
    State.Status="Looking for a compatible base-game cache on GitHub...";
    try{Worker=std::async(std::launch::async,[sources,nativePlugins,runtime,gameVersion]{return DownloadBase(sources,nativePlugins,runtime,gameVersion);});return true;}
    catch(...){State.Status="Could not start cache download; local Build / update remains available.";return false;}
}
bool Poll(){
    if(!Worker.valid()||Worker.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
    try{const auto result=Worker.get();if(!result.Status.empty())State.Status=result.Status;
        if(result.ApplyArtworkState){State.NeedsArtwork=result.NeedsArtwork;State.HintDismissed=State.HintDismissed||result.Dismissed;}PendingPack=result.Pack;return result.Artwork;
    }catch(const std::exception& error){State.Status=error.what();return false;}
}
std::filesystem::path TakeBaseCache(){auto file=std::move(PendingPack);PendingPack.clear();return file;}
void DismissHint(){State.HintDismissed=true;try{Commit(Folder()/"hint-dismissed",Bytes{'1'});}catch(...){};}
void Cancel()noexcept{Cancelled=true;}
void Shutdown()noexcept{Cancelled=true;if(Worker.valid())try{(void)Worker.get();}catch(...){}Initialized=false;}
#if defined(HELPY_ASSET_TEST)
void TestArtwork(const std::filesystem::path& source,const std::filesystem::path& destination){
    Cancelled=false;(void)InstallArtwork(destination,[&](const std::string& name,size_t size){return Read(source/std::filesystem::path(name).filename(),size);});
}
#endif
}
