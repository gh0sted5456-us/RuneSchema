#include "Runtime/HelpyModCacheBuilder.h"
#include "Core/ConfigFiles.h"
#include "Core/RuneSchemaDataRoot.h"
#include "Core/RsmkResources.h"
#include <windows.h>
#include <shlobj.h>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <fstream>

namespace PS::HelpyModCacheBuilder {
namespace {
namespace fs=std::filesystem;
View State;HANDLE Process=nullptr;bool Imported=false,Creation=false;
std::chrono::steady_clock::time_point Next{};
bool Name(const std::string& value){return !value.empty()&&value.size()<=128&&value!="."&&value.find("..")==value.npos&&value.back()!='.'&&value.back()!=' '&&std::all_of(value.begin(),value.end(),[](unsigned char c){return std::isalnum(c)||c=='_'||c==' '||c=='.'||c=='-';});}
std::wstring Quote(const fs::path& path){const auto value=path.wstring();if(value.empty()||value.find_first_of(L"\"\r\n")!=value.npos||value.back()==L'\\')throw std::runtime_error("Invalid authoring path.");return L"\""+value+L"\"";}
fs::path NewJob(){
    GUID id{};if(FAILED(CoCreateGuid(&id)))throw std::runtime_error("Could not reserve a cache job.");wchar_t key[40]{};StringFromGUID2(id,key,40);
    const auto root=RuneSchemaData::Directory()/"core/tools/cache-builder";RuneSchemaData::Safe(root);fs::create_directories(root);
    const auto job=root/key;RuneSchemaData::Safe(job);if(!fs::create_directory(job))throw std::runtime_error("Cache job already exists.");return job;
}
void Start(const fs::path& input,bool creation,bool installNet,const std::string& platform){
    const auto resource=FindResourceW(nullptr,MAKEINTRESOURCEW(State.Repair?IDR_RUNESCHEMA_REPAIR:IDR_CACHE_BUILDER),RT_RCDATA);
    const auto bytes=resource?SizeofResource(nullptr,resource):0;const auto loaded=resource?LoadResource(nullptr,resource):nullptr;
    const auto data=loaded?static_cast<const char*>(LockResource(loaded)):nullptr;
    if(!data||bytes==0||bytes>2*1024*1024)throw std::runtime_error("The built-in cache builder is unavailable. Update Helpy.exe.");
    const auto script=State.Job/"builder.ps1";ConfigFiles::Write(script,std::string_view(data,bytes));
    ConfigFiles::Write(State.Job/"control.json",nlohmann::json{{"Action","run"},{"OwnerPid",GetCurrentProcessId()}}.dump());
    wchar_t windows[32768]{};if(!GetWindowsDirectoryW(windows,32768))throw std::runtime_error("Windows directory unavailable.");
    const auto executable=fs::path(windows)/"System32/WindowsPowerShell/v1.0/powershell.exe";
    auto command=Quote(executable)+(State.Repair?L" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "+Quote(script)+L" -Request "+Quote(input)+L" -ProgressFile "+Quote(State.Job/"progress.json")+L" -ControlFile "+Quote(State.Job/"control.json"):
        Arguments(script,input,creation,State.Job/"progress.json",State.Job/"control.json",installNet,State.Output,platform));
    STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,State.Job.c_str(),&startup,&process))throw std::runtime_error("Could not start the offline cache builder.");
    CloseHandle(process.hThread);Process=process.hProcess;Creation=creation;Imported=false;Next={};State.Running=true;State.State="Running";State.Stage=State.Repair?"Checking RuneSchema base files":"Checking authoring dependencies";State.Percent=0;
}
}
const View& Current(){return State;}
std::filesystem::path InstalledFolder(const fs::path& root,const fs::path& folder,const std::string& owner){
    if(!Name(owner))throw std::runtime_error("Invalid mod folder name.");
    const auto expected=fs::absolute(root/"mods"/fs::u8path(owner)).lexically_normal();RuneSchemaData::Safe(expected);RuneSchemaData::Safe(folder);
    if(!fs::is_directory(expected)||!fs::is_directory(folder)||!fs::equivalent(expected,folder))throw std::runtime_error("Refresh Mods: the selected folder is no longer inside this runtime's mods directory.");return expected;
}
std::wstring Arguments(const fs::path& script,const fs::path& input,bool creation,const fs::path& progress,const fs::path& control,bool installNet,const fs::path& output,const std::string& platform){
    if(platform!="Win64"&&platform!="WinGDK")throw std::runtime_error("Invalid packaging platform.");
    auto result=L" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "+Quote(script)+(creation?L" -CreationRequest ":L" -ModDirectory ")+Quote(input)+L" -ProgressFile "+Quote(progress)+L" -ControlFile "+Quote(control);
    if(creation)result+=L" -PackagePath "+Quote(output)+L" -Platform "+std::wstring(platform.begin(),platform.end());
    if(installNet)result+=L" -InstallDotNet";return result;
}
bool StartInstalled(const fs::path& root,const fs::path& folder,const std::string& owner,bool installNet){
    if(Process)return false;
    try{const auto mod=InstalledFolder(root,folder,owner);State={};State.Owner=owner;State.Cache=mod/"cache";State.Output=State.Cache;State.Job=NewJob();Start(mod,false,installNet,"Win64");return true;}
    catch(const std::exception& error){State.State="Failed";State.Stage=error.what();return false;}
}
bool StartCreation(const std::vector<fs::path>& inputs,const nlohmann::json& metadata,const std::string& platform,bool installNet){
    if(Process)return false;
    try{
        const auto owner=metadata.value("ModID",std::string{});if(!Name(owner)||inputs.empty()||inputs.size()>64)throw std::runtime_error("Choose source files/folders and a valid Mod ID.");
        State={};State.Owner=owner;State.Job=NewJob();State.Cache=State.Job/"cache";
        PWSTR downloads=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_Downloads,0,nullptr,&downloads)))throw std::runtime_error("Downloads folder is unavailable.");
        const fs::path destination(downloads);CoTaskMemFree(downloads);RuneSchemaData::Safe(destination);
        State.Output=destination/(owner+"-RuneSchema-"+State.Job.filename().string()+".zip");
        nlohmann::json files=nlohmann::json::array();for(const auto& input:inputs){RuneSchemaData::Safe(input);if(!fs::exists(input))throw std::runtime_error("A selected source is missing.");const auto text=input.u8string();files.push_back(std::string(text.begin(),text.end()));}
        const auto request=State.Job/"creation.json";ConfigFiles::Write(request,nlohmann::json{{"Inputs",files},{"Metadata",metadata}}.dump());Start(request,true,installNet,platform);return true;
    }catch(const std::exception& error){State.State="Failed";State.Stage=error.what();return false;}
}
bool StartRepair(const fs::path& root,const std::string& version){
    if(Process)return false;
    try{State={};State.Repair=true;State.Owner="RuneSchema";RuneSchemaData::Safe(root);if(!fs::is_directory(root))throw std::runtime_error("The assigned RuneSchema installation is unavailable.");
        State.Job=NewJob();State.Output=root;
        const auto request=State.Job/"repair.json";const auto text=root.u8string();ConfigFiles::Write(request,nlohmann::json{{"Root",std::string(text.begin(),text.end())},{"Version",version}}.dump());
        Start(request,false,false,"Win64");return true;
    }catch(const std::exception& error){State.State="Failed";State.Stage=error.what();return false;}
}
void Control(const std::string& action){
    if(!Process)return;if(action!="run"&&action!="pause"&&action!="stop")throw std::runtime_error("Invalid authoring control.");
    try{ConfigFiles::Write(State.Job/"control.json",nlohmann::json{{"Action",action},{"OwnerPid",GetCurrentProcessId()}}.dump());State.State=action=="run"?"Running":action=="pause"?"Pausing":"Stopping";State.Stage=action=="run"?"Resuming":action=="pause"?"Pause requested; waiting for a safe checkpoint":"Stop requested; waiting for a safe checkpoint";}
    catch(const std::exception& error){State.Stage=error.what();}
}
void Poll(){
    if(!Process||std::chrono::steady_clock::now()<Next)return;Next=std::chrono::steady_clock::now()+std::chrono::milliseconds(250);
    const bool finished=WaitForSingleObject(Process,0)==WAIT_OBJECT_0;
    try{const auto result=nlohmann::json::parse(ConfigFiles::Read(State.Job/"progress.json",16384));const auto state=result.value("State",std::string{});
        if((State.State!="Pausing"&&State.State!="Stopping")||state=="Paused"||finished){State.State=state;State.Stage=result.value("Stage",State.Stage);}
        State.Percent=std::clamp(result.value("Percent",0),0,100);State.Warnings.clear();for(const auto& warning:result.value("Warnings",nlohmann::json::array()))if(warning.is_string()&&State.Warnings.size()<64)State.Warnings.push_back(warning.get<std::string>());
    }catch(...){} // An in-progress status write is retried on the next visible frame.
    if(State.Repair)try{std::ifstream file(State.Job/"console.log",std::ios::binary);if(file){file.seekg(0,std::ios::end);const auto length=file.tellg();const auto start=std::max<std::streamoff>(0,static_cast<std::streamoff>(length)-65536);file.seekg(start);State.Console.assign(std::istreambuf_iterator<char>(file),{});if(start>0){const auto newline=State.Console.find('\n');if(newline!=State.Console.npos)State.Console.erase(0,newline+1);}}}catch(...){}
    if(finished){DWORD code=1;GetExitCodeProcess(Process,&code);CloseHandle(Process);Process=nullptr;State.Running=false;
        if(code!=0&&State.State!="Cancelled"){State.State="Failed";if(State.Stage=="Checking authoring dependencies")State.Stage="Cache build did not finish. Open the job log for details.";}
        if(code==0){State.State="Succeeded";State.Percent=100;}
    }
}
fs::path ReadyBundle(){return !State.Repair&&!Creation&&!Imported&&!State.Running&&State.State=="Succeeded"?State.Cache/"offline-helpy.rsmk-cache":fs::path{};}
void BundleApplied(){Imported=true;}
void Shutdown() noexcept{if(Process){try{Control("stop");}catch(...){}CloseHandle(Process);Process=nullptr;State.Running=false;}}
}
