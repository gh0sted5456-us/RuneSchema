#include "Runtime/HelpyUpdates.h"
#include "Core/RuneSchemaUpdateInfo.h"
#include "Core/SteamNewsInfo.h"
#include "Core/ConfigFiles.h"
#include <windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace PS::HelpyUpdates {
namespace {
View Result;std::future<View> Worker;std::atomic_bool Cancelled=false;std::atomic_int Progress=0;
bool Loaded=false;std::chrono::steady_clock::time_point NextManual{};
std::int64_t Now(){return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
std::filesystem::path CachePath(){const auto* appdata=std::getenv("LOCALAPPDATA");return appdata&&*appdata?std::filesystem::path(appdata)/"RuneSchema"/"HELPY"/"updates.json":std::filesystem::path{};}
void Load(){
    if(Loaded)return;Loaded=true;
    try{
        const auto path=CachePath();std::error_code error;
        if(path.empty()||!std::filesystem::is_regular_file(path,error)||std::filesystem::file_size(path,error)>32768)return;
        const auto data=nlohmann::json::parse(ConfigFiles::Read(path));
        if(data.value("Format",0)!=1)return;
        Result.CheckedAt=data.value("CheckedAt",std::int64_t{});
        if(Result.CheckedAt<0||Result.CheckedAt>Now()+300){Result={};return;}
        const auto bounded=[&](const char* key){const auto value=data.value(key,std::string{});return value.size()<=256?value:std::string{};};
        Result.MainVersion=bounded("MainVersion");Result.ReleaseVersion=bounded("ReleaseVersion");
        Result.MainError=bounded("MainError");Result.ReleaseError=bounded("ReleaseError");
        const auto news=SteamNewsInfo::LatestUpdate(data.value("GameNews",nlohmann::json::object()),Now());
        if(news){Result.GameUpdateTitle=news->Title;Result.GameUpdateUrl=news->Url;Result.GameUpdateDate=news->Date;}
        if(!Result.MainVersion.empty()&&!RuneSchemaUpdateInfo::Parse(Result.MainVersion))Result.MainVersion.clear();
        if(!Result.ReleaseVersion.empty()&&!RuneSchemaUpdateInfo::Parse(Result.ReleaseVersion))Result.ReleaseVersion.clear();
    }catch(...){Result={};}
}
void Save(){try{
    const auto path=CachePath();if(path.empty())return;
    ConfigFiles::Write(path,nlohmann::json{{"Format",1},{"CheckedAt",Result.CheckedAt},{"MainVersion",Result.MainVersion},
        {"ReleaseVersion",Result.ReleaseVersion},{"MainError",Result.MainError},{"ReleaseError",Result.ReleaseError},
        {"GameNews",{{"appnews",{{"appid",1374490},{"newsitems",nlohmann::json::array({{{"feedname","steam_community_announcements"},{"title",Result.GameUpdateTitle},{"url",Result.GameUpdateUrl},{"date",Result.GameUpdateDate}}})}}}}}}.dump(2));
}catch(...){Result.CacheError="Update results could not be cached; checking still works for this session.";}}
struct InternetHandle {HINTERNET Value{};~InternetHandle(){if(Value)WinHttpCloseHandle(Value);}operator HINTERNET() const{return Value;}};
std::string Fetch(const wchar_t* host,const wchar_t* path){
    if(Cancelled)throw std::runtime_error("Check cancelled");
    InternetHandle session{WinHttpOpen(L"RuneSchema-Helpy-UpdateCheck/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session)throw std::runtime_error("Network session unavailable");
    if(!WinHttpSetTimeouts(session,1500,1500,1500,1500))throw std::runtime_error("Could not enforce network timeouts");
    InternetHandle connection{WinHttpConnect(session,host,INTERNET_DEFAULT_HTTPS_PORT,0)};
    if(!connection)throw std::runtime_error("Could not connect");
    InternetHandle request{WinHttpOpenRequest(connection,L"GET",path,nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
    if(!request)throw std::runtime_error("Could not create metadata request");
    DWORD disabled=WINHTTP_DISABLE_REDIRECTS;
    if(!WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled)))throw std::runtime_error("Could not enforce HTTPS endpoint policy");
    // Only fixed public endpoints, default certificate validation, no account,
    // game/player data, cookies, tokens, executable downloads or auto-install.
    constexpr wchar_t headers[]=L"Accept: application/json\r\n";
    if(!WinHttpSendRequest(request,headers,-1L,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request,nullptr))throw std::runtime_error("Network request failed or timed out");
    DWORD status=0,length=sizeof(status);
    if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX))throw std::runtime_error("Missing HTTP status");
    if(status!=200)throw std::runtime_error(status==403||status==429?"GitHub rate limit or access restriction; retry later":"Metadata endpoint returned HTTP "+std::to_string(status));
    std::string body;std::array<char,8192> buffer{};
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
    for(;;){
        if(Cancelled)throw std::runtime_error("Check cancelled");
        if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Metadata read timed out");
        DWORD count=0;if(!WinHttpReadData(request,buffer.data(),static_cast<DWORD>(buffer.size()),&count))throw std::runtime_error("Metadata read failed");
        if(!count)break;if(body.size()+count>256*1024)throw std::runtime_error("Metadata response exceeds the size limit");
        body.append(buffer.data(),count);
    }
    return body;
}
View Check(){
    View result;result.CheckedAt=Now();Progress=1;
    try{
        const auto source=Fetch(L"raw.githubusercontent.com",L"/gh0sted5456-us/RuneSchema/main/source/raw/include/Utility/BuildInfo.h");
        const auto version=RuneSchemaUpdateInfo::MainVersion(source);
        if(!version)throw std::runtime_error("Main branch did not publish a recognizable version");result.MainVersion=*version;
    }catch(const std::exception& error){result.MainError=error.what();}
    Progress=2;
    if(!Cancelled)try{
        const auto release=nlohmann::json::parse(Fetch(L"api.github.com",L"/repos/gh0sted5456-us/RuneSchema/releases/latest"));
        if(release.value("draft",true)||release.value("prerelease",true))throw std::runtime_error("No public stable release was returned");
        result.ReleaseVersion=release.value("tag_name",std::string{});
        if(!RuneSchemaUpdateInfo::Parse(result.ReleaseVersion))throw std::runtime_error("Release tag could not be compared");
    }catch(const std::exception& error){result.ReleaseVersion.clear();result.ReleaseError=error.what();}
    Progress=3;
    if(!Cancelled)try{
        const auto news=SteamNewsInfo::LatestUpdate(nlohmann::json::parse(Fetch(L"api.steampowered.com",L"/ISteamNews/GetNewsForApp/v2/?appid=1374490&count=30&maxlength=1&feeds=steam_community_announcements")),Now());
        if(!news)throw std::runtime_error("No dated update announcement was returned");
        result.GameUpdateTitle=news->Title;result.GameUpdateUrl=news->Url;result.GameUpdateDate=news->Date;
    }catch(const std::exception& error){result.GameUpdateError=error.what();}
    Progress=4;return result;
}
}
const View& Current(){Load();return Result;}
bool Busy(){return Worker.valid();}
int Stage(){return Progress.load();}
int CooldownSeconds(){const auto remaining=NextManual-std::chrono::steady_clock::now();return remaining>std::chrono::steady_clock::duration::zero()?static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(remaining).count()+1):0;}
bool Start(const std::string& installed){
    Load();if(Busy()||!RuneSchemaUpdateInfo::Parse(installed)||std::chrono::steady_clock::now()<NextManual)return false;
    NextManual=std::chrono::steady_clock::now()+std::chrono::seconds(60);Cancelled=false;Progress=0;
    try{Worker=std::async(std::launch::async,Check);return true;}catch(...){Result.MainError="Could not start update worker";return false;}
}
bool Tick(bool automatic,bool networkAllowed,const std::string& installed){
    Load();bool notify=false;
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{auto completed=Worker.get();if(!Cancelled){Result=std::move(completed);Save();notify=
            RuneSchemaUpdateInfo::Compare(installed,Result.MainVersion)==RuneSchemaUpdateInfo::Comparison::Newer
            ||RuneSchemaUpdateInfo::Compare(installed,Result.ReleaseVersion)==RuneSchemaUpdateInfo::Comparison::Newer;}}
        catch(...){Result.MainError="Update check failed; installed files were not changed";}
    }
    if(automatic&&networkAllowed&&!Busy()&&Now()-Result.CheckedAt>=86400)Start(installed);
    return notify;
}
void Cancel() noexcept{Cancelled=true;}
void Shutdown() noexcept{Cancelled=true;if(Worker.valid())try{(void)Worker.get();}catch(...){}Progress=0;}
}
