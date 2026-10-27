#include "Core/ProcessExitFuture.h"
#include "Runtime/HelpyMapCache.h"
#include "Runtime/HostServices.h"
#include "Core/HelpyEnvironment.h"
#include <windows.h>
#include <winhttp.h>
#include <future>
#include <fstream>
#include <array>
#include <atomic>
#include <chrono>
namespace PS::HelpyMapCache {
namespace {
using Clock=std::chrono::steady_clock;using Json=nlohmann::json;
View State;bool Initialized=false,Dirty=false;std::atomic<bool> Cancelled=false;
Clock::time_point SaveAt{};std::string AttemptedBuild;
struct Result {std::shared_ptr<const MapFilters::Dataset> Data;std::string Build,Status;MapFilters::Selection Filters;bool Load=false;};
PS::ProcessExitFuture<Result> Worker;
std::string Lane(){using K=HelpyEnvironment::Kind;switch(HelpyEnvironment::Current){case K::Steam:return "steam";case K::GamePass:return "gamepass";case K::Dedicated:return "dedicated";case K::Epic:return "epic";case K::SteamEpic:return "desktop";default:return "unknown";}}
std::string Installed(){return MapFilters::ConfirmedBuild(HelpyEnvironment::GameVersion);}
std::filesystem::path Folder(){return HostServices::CacheDirectory()/"map"/Lane();}
void Safe(const std::filesystem::path& path){
    for(auto check=std::filesystem::absolute(path).lexically_normal();!check.empty();){
        std::error_code error;const auto status=std::filesystem::symlink_status(check,error);
        if(!error&&std::filesystem::exists(status)){
            const auto flags=GetFileAttributesW(check.c_str());if(flags==INVALID_FILE_ATTRIBUTES||(flags&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Redirected map cache path refused");
        }else if(error&&error!=std::errc::no_such_file_or_directory)throw std::runtime_error("Cannot inspect map cache path");
        const auto parent=check.parent_path();if(parent==check)break;check=parent;
    }
}
std::string Read(const std::filesystem::path& path,size_t limit){
    Safe(path);if(!std::filesystem::is_regular_file(path))return {};const auto size=std::filesystem::file_size(path);
    if(size>limit)throw std::runtime_error("Cached map file exceeds its limit");std::ifstream input(path,std::ios::binary);std::string bytes(static_cast<size_t>(size),'\0');input.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    if(!input||input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Map cache changed during read");return bytes;
}
void Commit(const std::filesystem::path& path,const std::string& bytes){
    Safe(path);std::filesystem::create_directories(path.parent_path());Safe(path);
    const auto temporary=path.parent_path()/(path.filename().wstring()+L"."+std::to_wstring(GetCurrentProcessId())+L".tmp");Safe(temporary);
    HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot stage map cache");
    DWORD written=0;const bool ok=bytes.size()<=MAXDWORD&&WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(file);CloseHandle(file);
    if(!ok||!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temporary.c_str());throw std::runtime_error("Map cache commit failed; previous file retained");}
}
struct Handle {HINTERNET Value{};~Handle(){if(Value)WinHttpCloseHandle(Value);}operator HINTERNET()const{return Value;}};
std::string Fetch(){
    Handle session{WinHttpOpen(L"RuneSchema-Helpy-Map/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session||!WinHttpSetTimeouts(session,2000,2000,2000,2000))throw std::runtime_error("Map network session unavailable");
    Handle connection{WinHttpConnect(session,L"wildsforge.org",INTERNET_DEFAULT_HTTPS_PORT,0)};
    Handle request{connection?WinHttpOpenRequest(connection,L"GET",L"/map/markers.json",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):nullptr};DWORD disabled=WINHTTP_DISABLE_REDIRECTS;
    if(!request||!WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled))||!WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request,nullptr))throw std::runtime_error("Map download unavailable; cached filters retained");
    DWORD status=0,length=sizeof(status);if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX)||status!=200)throw std::runtime_error("Map download returned HTTP "+std::to_string(status));
    std::string bytes;std::array<char,16384> buffer{};const auto deadline=Clock::now()+std::chrono::seconds(30);
    for(;;){if(Cancelled||Clock::now()>deadline)throw std::runtime_error("Map download cancelled/timed out; cache retained");DWORD count=0;if(!WinHttpReadData(request,buffer.data(),static_cast<DWORD>(buffer.size()),&count))throw std::runtime_error("Map download interrupted; cache retained");if(!count)break;if(bytes.size()+count>MapFilters::MaxBytes)throw std::runtime_error("Map download exceeds its limit");bytes.append(buffer.data(),count);}return bytes;
}
Result Install(const std::filesystem::path& folder,const std::string& build,const std::string& bytes){
    auto data=std::make_shared<MapFilters::Dataset>(MapFilters::Parse(bytes));
    if(Cancelled)throw std::runtime_error("Map download cancelled; cache retained");
    Commit(folder/"markers.json",Json{{"Version",1},{"Build",build},{"Attribution",MapFilters::Credit},{"SourceUrl",MapFilters::SourceUrl},{"LicenceUrl",MapFilters::LicenseUrl},{"Payload",bytes}}.dump());
    return {std::move(data),build,"Map filter cache ready. Kept until a confirmed installed game update."};
}
bool Download(){
    if(Worker.valid())return false;const auto build=Installed();Cancelled=false;AttemptedBuild=build;
    try{const auto folder=Folder();State.Status="Downloading map markers; validating source, coordinates and limits...";Worker=std::async(std::launch::async,[folder,build]{return Install(folder,build,Fetch());});return true;}
    catch(...){State.Status="Cannot start map download; existing map cache retained.";return false;}
}
}
View& Current(){return State;}
bool Busy(){return Worker.valid();}
void Initialize(){
    if(Initialized)return;Initialized=true;Cancelled=false;
    try{const auto folder=Folder();Worker=std::async(std::launch::async,[folder]{
        Result result;result.Load=true;result.Status="No map cache. Use Download Initial Assets to add resource filters.";
        const auto settings=Read(folder/"filters.json",1024*1024);if(!settings.empty())try{result.Filters=MapFilters::Selection::From(Json::parse(settings));}catch(...){}
        const auto bytes=Read(folder/"markers.json",MapFilters::MaxBytes*2+1024);
        if(!bytes.empty()){
            const auto json=Json::parse(bytes,[](int depth,Json::parse_event_t,Json&){if(depth>4)throw std::runtime_error("Map cache wrapper exceeds depth limit");return true;});
            if(json.value("Version",0)!=1)throw std::runtime_error("Unsupported map cache version");
            result.Data=std::make_shared<MapFilters::Dataset>(MapFilters::Parse(json.at("Payload").get<std::string>()));result.Build=json.value("Build",std::string{});
            if(result.Build.size()>160)throw std::runtime_error("Cached build identity exceeds its bound");result.Status="Map filters loaded from local cache.";
        }return result;
    });}catch(...){State.Ready=true;State.Status="Map cache could not be read; live map remains available.";}
}
bool RefreshEligible(){return State.Ready&&MapFilters::NeedsUpdate(static_cast<bool>(State.Data),State.CachedBuild,Installed(),true);}
bool StartInitial(){if(!State.Ready||Busy())return false;if(State.Data)return Refresh();return Download();}
bool Refresh(){
    if(!RefreshEligible()){State.Status=State.Data?"Cached filters retained: no confirmed installed game update.":"Use Download Initial Assets to obtain map filters.";return false;}return Download();
}
void SaveFilters(){++State.Revision;Dirty=true;SaveAt=Clock::now()+std::chrono::milliseconds(300);}
void Cancel()noexcept{Cancelled=true;}
void Poll(){
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{auto result=Worker.get();if(result.Data){State.Data=std::move(result.Data);State.CachedBuild=std::move(result.Build);++State.Revision;}if(result.Load){State.Filters=std::move(result.Filters);++State.Revision;}if(!result.Status.empty())State.Status=std::move(result.Status);State.Ready=true;}
        catch(const std::exception& error){State.Ready=true;State.Status=error.what();}
    }
    if(!Busy()&&Dirty&&Clock::now()>=SaveAt){
        try{const auto folder=Folder();const auto bytes=State.Filters.ToJson().dump();Worker=std::async(std::launch::async,[folder,bytes]{Commit(folder/"filters.json",bytes);return Result{};});Dirty=false;}
        catch(...){Dirty=false;State.Status="Map filter preferences could not be saved.";}
    }
#if !defined(HELPY_PREVIEW) && !defined(HELPY_MAP_CACHE_TEST)
    // One attempt per confirmed build per session, never calendar/mod polling.
    if(!Busy()&&State.Filters.UpdateAfterGamePatch&&RefreshEligible()&&AttemptedBuild!=Installed())Download();
#endif
}
void Shutdown()noexcept{
    Cancelled=true;if(Worker.valid())try{(void)Worker.get();}catch(...){}
    if(Dirty)try{Commit(Folder()/"filters.json",State.Filters.ToJson().dump());}catch(...){}
    Dirty=false;Initialized=false;
}
#if defined(HELPY_MAP_CACHE_TEST)
void Test(const std::filesystem::path& folder,const std::string& bytes){Cancelled=false;(void)Install(folder,"CL-251392",bytes);}
#endif
}
