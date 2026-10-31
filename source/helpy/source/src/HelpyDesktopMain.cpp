#include "HelpyDesktopTransport.h"
#include "Core/HelpyGameLifetime.h"
#include "Core/HelpyFramePacing.h"
#include "Runtime/HelpySettings.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include "Runtime/HelpyOfflineRuntime.h"
#include "Core/RuneSchemaDataRoot.h"
#include "Core/DiagnosticLogPaths.h"
#include "Runtime/HelpyPaths.h"
#include "HelpyDesktopLaunch.h"
#include "HelpyDesktopShortcut.h"
#include <shellapi.h>
#include <map>
#include <cstring>
#include <cstdio>

namespace {
using PS::HelpyDesktop::Json;
struct Service{RuneSchemaServiceFn Callback{};void* Context{};};
std::mutex ServicesMutex,RpcMutex;std::map<std::string,Service> Services;
PS::HelpyDesktop::Client Bridge;std::atomic<bool> Visible=true;
uint32_t ConnectedPid=0;
Json OfflineEnvironment;
std::filesystem::path AssignedRoot,AssignedCache;
std::filesystem::path LogFile;std::mutex LogMutex;
void RS_PLUGIN_CALL Log(uint32_t level,const char*,const char* message){
    if(!message)return;OutputDebugStringA((std::string("[Helpy.exe] ")+message+"\n").c_str());
    try{std::lock_guard lock(LogMutex);if(LogFile.empty())return;std::filesystem::create_directories(LogFile.parent_path());std::error_code error;
        if(std::filesystem::file_size(LogFile,error)>2*1024*1024&&!error){auto previous=LogFile;previous+=L".previous";std::filesystem::remove(previous,error);error.clear();std::filesystem::rename(LogFile,previous,error);}
        SYSTEMTIME time{};GetSystemTime(&time);char stamp[40]{};std::snprintf(stamp,sizeof(stamp),"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds);
        std::ofstream output(LogFile,std::ios::app);output<<'['<<stamp<<"] ["<<(level==RS_LOG_ERROR?"error":level==RS_LOG_WARNING?"warning":"info")<<"] [Helpy] "<<std::string_view(message).substr(0,16384)<<'\n';
    }catch(...){}
}
void Notify(const char* service,const Json& value){
    Service target;{std::lock_guard lock(ServicesMutex);const auto found=Services.find(service);if(found==Services.end())return;target=found->second;}
    const auto text=value.dump();uint32_t ignored=0;target.Callback(target.Context,text.c_str(),nullptr,0,&ignored);
}
void Connection(uint32_t pid){if(pid==ConnectedPid)return;ConnectedPid=pid;Notify("helpy.desktop.connection",{{"Pid",pid}});Log(RS_LOG_INFO,"RuneSchema.Helpy",pid?("Attached to game PID "+std::to_string(pid)).c_str():"Game disconnected; cached browsing remains available.");}
void Deliver(const Json& reply){
    Connection(Bridge.Pid.load());if(reply.value("Reset",false)){Connection(0);Connection(Bridge.Pid.load());}
    if(reply.contains("Events")&&reply["Events"].is_array())for(const auto& event:reply["Events"])Notify("helpy.tools.push",event);
}
int32_t RS_PLUGIN_CALL Capability(const char*,const char*){return RS_PLUGIN_OK;}
int32_t RS_PLUGIN_CALL Register(const char* plugin,const char* name,RuneSchemaServiceFn callback,void* context){
    if(!plugin||std::string_view(plugin)!="RuneSchema.Helpy"||!name||!callback)return RS_PLUGIN_INVALID_ARGUMENT;
    std::lock_guard lock(ServicesMutex);return Services.emplace(name,Service{callback,context}).second?RS_PLUGIN_OK:RS_PLUGIN_DUPLICATE;
}
int32_t Reply(const Json& value,char* response,uint32_t capacity,uint32_t* size){
    const auto text=value.dump();if(text.size()+1>RUNESCHEMA_PLUGIN_MAX_MESSAGE)return RS_PLUGIN_FAILED;
    *size=static_cast<uint32_t>(text.size()+1);if(!response||capacity<*size)return RS_PLUGIN_BUFFER_TOO_SMALL;
    std::memcpy(response,text.c_str(),*size);return RS_PLUGIN_OK;
}
int32_t RS_PLUGIN_CALL Call(const char* caller,const char* service,const char* input,char* response,uint32_t capacity,uint32_t* size){
    if(!caller||std::string_view(caller)!="RuneSchema.Helpy"||!service||std::string_view(service)!="runeschema.tools"||!input||!size||capacity>RUNESCHEMA_PLUGIN_MAX_MESSAGE||(!response&&capacity))return RS_PLUGIN_INVALID_ARGUMENT;
    try{
        if(strnlen_s(input,RUNESCHEMA_PLUGIN_MAX_MESSAGE+1)>RUNESCHEMA_PLUGIN_MAX_MESSAGE)return RS_PLUGIN_INVALID_ARGUMENT;
        const auto request=PS::HelpyDesktop::Parse(input);const auto action=request.value("Action",std::string{});
        if(action=="HelpyVisibility")Visible=request.at("Visible").get<bool>();
        std::lock_guard lock(RpcMutex);Json result;
        const bool attempted=Bridge.Pid.load()!=0;
        const bool online=attempted&&Bridge.Exchange({{"Version",1},{"Op","Call"},{"Request",request},{"Capacity",capacity}},result);
        if(online)Deliver(result);else Connection(0);
        if(action=="Environment"){
            auto environment=online&&result.contains("Body")?result["Body"]:OfflineEnvironment;
            environment["ExternalDesktop"]=true;environment["GameProcessId"]=Bridge.Pid.load();environment["DesktopStartHidden"]=OfflineEnvironment.value("DesktopStartHidden",false);return Reply(environment,response,capacity,size);
        }
        if(!online){
            if(action=="IconCacheSources")return Reply(PS::HelpyOfflineRuntime::Sources(AssignedRoot,AssignedCache/"item-icons/sources.json"),response,capacity,size);
            if(action=="SettingsRead"||action=="SettingsSave"||action=="ModsRead"||action=="ModsSave"||action=="ModsImport"){
                // A submitted live mutation with an unknown outcome is never retried offline.
                if(!attempted){
                    if((action=="SettingsSave"||action=="ModsSave"||action=="ModsImport")&&capacity<RUNESCHEMA_PLUGIN_MAX_MESSAGE){*size=RUNESCHEMA_PLUGIN_MAX_MESSAGE;return RS_PLUGIN_BUFFER_TOO_SMALL;}
                    return Reply(PS::HelpyOfflineRuntime::Call(AssignedRoot,request),response,capacity,size);
                }
            }
            *size=0;return RS_PLUGIN_NOT_FOUND;
        }
        *size=result.value("Size",uint32_t{});const auto code=result.value("Code",int32_t(RS_PLUGIN_FAILED));
        if(code==RS_PLUGIN_OK&&result.contains("Body"))return Reply(result["Body"],response,capacity,size);
        return code;
    }catch(...){*size=0;return RS_PLUGIN_FAILED;}
}
RuneSchemaHostApi Host{sizeof(RuneSchemaHostApi),RUNESCHEMA_PLUGIN_API_VERSION,Log,Capability,Register,Call};
Json DesktopStatus(){
    Service target;{std::lock_guard lock(ServicesMutex);const auto found=Services.find("helpy.desktop.status");if(found==Services.end())return {{"Ready",false}};target=found->second;}
    char output[256]{};uint32_t size=0;if(target.Callback(target.Context,"{}",output,sizeof(output),&size)!=RS_PLUGIN_OK)return {{"Ready",false}};return Json::parse(output);
}
bool Ready(){return DesktopStatus().value("Ready",false);}

}
#if !defined(HELPY_DESKTOP_HOST_TEST)
extern "C" int32_t RS_PLUGIN_CALL RuneSchemaPlugin_Initialize(const RuneSchemaHostApi*,void**) noexcept;
extern "C" int32_t RS_PLUGIN_CALL RuneSchemaPlugin_OnUnrealInit(void*) noexcept;
extern "C" void RS_PLUGIN_CALL RuneSchemaPlugin_Shutdown(void*) noexcept;
extern "C" const RuneSchemaPluginDescriptor* RS_PLUGIN_CALL RuneSchemaPlugin_Query() noexcept;
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    HMODULE module{};HANDLE singleton{},activate{};void* instance{};RuneSchemaPluginShutdownFn shutdown{};std::jthread worker;bool smoke=false;
    try{
        wchar_t file[32768]{};if(!GetModuleFileNameW(nullptr,file,32768))throw std::runtime_error("Cannot locate Helpy.exe");
        const auto executable=std::filesystem::path(file);auto root=executable.parent_path().parent_path().parent_path().parent_path();
        std::string platform;bool startHidden=false,explicitRoot=false;int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        if(!argv)throw std::runtime_error("Cannot read desktop arguments");
        for(int i=1;i<argc;++i){const std::wstring_view option=argv[i];
            if(option==L"--runtime-root"&&i+1<argc){root=argv[++i];explicitRoot=true;}
            else if(option==L"--platform"&&i+1<argc){const auto value=std::wstring_view(argv[++i]);platform=std::string(value.begin(),value.end());}
            else if(option==L"--smoke-test")smoke=true;
            else if(option==L"--start-hidden")startHidden=true;
            else{LocalFree(argv);throw std::runtime_error("Use --runtime-root <RuneSchema folder> and --platform steam|gamepass|epic|server");}
        }LocalFree(argv);
        const auto binding=executable.parent_path()/"runtime.json";
        if(!explicitRoot&&std::filesystem::is_regular_file(binding)){const auto value=Json::parse(PS::ConfigFiles::Read(binding,16384));root=std::filesystem::u8path(value.at("Root").get<std::string>());if(platform.empty())platform=value.at("Platform").get<std::string>();}
        root=std::filesystem::weakly_canonical(root);
        AssignedRoot=root;
        const auto plugin=root/"plugins/RuneSchema.Helpy/dll/RuneSchema.Helpy.dll";
        if(!std::filesystem::is_regular_file(plugin)||!std::filesystem::is_regular_file(root/"dlls/main.dll"))throw std::runtime_error("Assign Helpy to a complete RuneSchema installation using --runtime-root");
        if(platform.empty()){
            if(root.wstring().find(L"WinGDK")!=std::wstring::npos)platform="gamepass";
            else if(std::filesystem::exists(root.parent_path().parent_path().parent_path()/"RSDragonwildsServer-Win64-Shipping.exe"))platform="server";
            else if(PS::HelpyDesktop::Identity(root).find(L"epic")!=std::wstring::npos)platform="epic";else platform="steam";
        }
        if(platform!="steam"&&platform!="gamepass"&&platform!="epic"&&platform!="server")throw std::runtime_error("Unknown assigned runtime");
        const auto settings=PS::RuneSchemaSettingsEditor::Read(root/"settings");
        PS::DiagnosticLogPaths::UseUE4SS=settings.value("Ok",false)&&settings.at("Document").value("logsInUE4SS",false);
        AssignedCache=PS::RuneSchemaData::PlatformDirectory(platform.c_str())/"cache";
        LogFile=(PS::DiagnosticLogPaths::UseUE4SS?PS::DiagnosticLogPaths::UE4SS(root):PS::RuneSchemaData::PlatformDirectory(platform.c_str())/"diagnostics")/"Helpy-Desktop.log";
        const auto hosted=PS::HelpyDesktopLaunch::Application(root,platform,PS::RuneSchemaData::LocalDirectory());
        if(!std::filesystem::exists(hosted)||!std::filesystem::equivalent(executable,hosted)){
            PS::HelpyDesktopLaunch::Start(root,platform,startHidden);return 0;
        }
        if(PS::HelpyDesktopLaunch::InGameContext())throw std::runtime_error("Helpy still belongs to a game session; exit it and open it from Windows Explorer");
        const auto local=PS::HelpyDesktopLaunch::InstanceName(root);
        activate=CreateEventW(nullptr,FALSE,FALSE,(local+L".activate").c_str());if(!activate)throw std::runtime_error("Cannot create desktop activation event");
        singleton=CreateMutexW(nullptr,FALSE,local.c_str());
        if(!singleton)throw std::runtime_error("Cannot reserve this runtime's Helpy window");
        if(GetLastError()==ERROR_ALREADY_EXISTS){if(!startHidden)SetEvent(activate);CloseHandle(activate);CloseHandle(singleton);return 0;}
        if(!smoke)try{PS::HelpyDesktopShortcut::Ensure(root/"plugins/RuneSchema.Helpy/dll/Helpy.exe",platform,executable.parent_path());}catch(const std::exception& error){Log(RS_LOG_WARNING,"RuneSchema.Helpy",error.what());}
        OfflineEnvironment={{"Storefront",platform=="gamepass"?"Game Pass/WinGDK":"Steam/Epic"},{"Platform",platform=="epic"?"Epic Games":"Steam"},{"DedicatedServer",platform=="server"},{"NativeIconCaptureAvailable",false},{"UpdateChecksAvailable",true},{"DesktopStartHidden",startHidden}};
        Bridge.Connect(root);
        PS::HelpyPaths::Configure(root/"plugins/RuneSchema.Helpy");
        OfflineEnvironment["RuneSchemaVersion"]=RuneSchemaPlugin_Query()->Version;
        shutdown=RuneSchemaPlugin_Shutdown;
        if(RuneSchemaPlugin_Initialize(&Host,&instance)!=RS_PLUGIN_OK)throw std::runtime_error("Helpy application initialization failed");
        const auto unreal=RuneSchemaPlugin_OnUnrealInit;
        if(unreal(instance)!=RS_PLUGIN_OK||!Ready())throw std::runtime_error("Helpy desktop window could not start; check its plugin settings");
        worker=std::jthread([root](std::stop_token stop){
            PS::HelpyPushPacing pushPacing;std::string lastFailure;const auto stopped=CreateEventW(nullptr,TRUE,FALSE,nullptr);
            {std::stop_callback cancel(stop,[stopped]{if(stopped)SetEvent(stopped);});bool update=true;
            while(!stop.stop_requested()){
                try{
                    const bool attached=Bridge.Pid.load()!=0,notifications=attached&&Bridge.Notifications();
                    const auto heartbeat=Visible.load()?2000ull:5000ull;
                    const auto now=GetTickCount64(),last=Bridge.LastExchangeMs.load();
                    if(!attached||update||!notifications||now-last>=heartbeat){
                        std::lock_guard lock(RpcMutex);
                        if(!Bridge.Pid.load()&&Bridge.Connect(root)){
                            Connection(Bridge.Pid.load());Json ignored;if(Bridge.Exchange({{"Op","Call"},{"Request",{{"Action","HelpyVisibility"},{"Visible",Visible.load()}}},{"Capacity",4096}},ignored))Deliver(ignored);
                        }
                        Json reply;if(Bridge.Pid.load()&&Bridge.Exchange({{"Op","Poll"}},reply)){Deliver(reply);pushPacing.Polled(GetTickCount64());}else Connection(0);
                    }
                }catch(const std::exception& error){std::lock_guard lock(RpcMutex);Bridge.Disconnect();Connection(0);if(lastFailure!=error.what()){lastFailure=error.what();Log(RS_LOG_WARNING,"RuneSchema.Helpy",("[RS-UI-002] "+lastFailure).c_str());}}
                if(stop.stop_requested())break;
                const bool attached=Bridge.Pid.load()!=0,notifications=attached&&Bridge.Notifications();
                const auto interval=!attached?2000u:notifications?(Visible.load()?2000u:5000u):(Visible.load()?200u:2000u);
                update=Bridge.WaitForUpdates(stopped?interval:std::min(interval,100u),stopped);
                if(update)if(const auto delay=pushPacing.Delay(GetTickCount64());delay){if(stopped){if(WaitForSingleObject(stopped,delay)==WAIT_OBJECT_0)break;}else Sleep(delay);}
            }}if(stopped)CloseHandle(stopped);

        });
        const auto requestQuit=[](){EnumWindows([](HWND window,LPARAM){DWORD pid=0;const auto thread=GetWindowThreadProcessId(window,&pid);wchar_t name[128]{};GetClassNameW(window,name,128);if(pid==GetCurrentProcessId()&&std::wstring_view(name)==L"RuneSchema.Helpy.Window")PostThreadMessageW(thread,WM_QUIT,0,0);return TRUE;},0);};
        if(smoke){Sleep(1500);std::ofstream(root/"desktop-smoke.json")<<Json{{"Ready",Ready()},{"TrayReady",DesktopStatus().value("TrayReady",false)},{"WindowVisible",DesktopStatus().value("WindowVisible",false)},{"DesktopPid",GetCurrentProcessId()},{"GamePid",Bridge.Pid.load()},{"RuntimeRoot",root.string()},{"StartHidden",startHidden}}.dump(2);requestQuit();}
        PS::HelpyGameLifetime game;
        while(Ready()){
            game.Observe(Bridge.Pid.load());
            if(game.Exited()&&PS::HelpySettings::ExitWhenGameCloses()){Log(RS_LOG_INFO,"RuneSchema.Helpy","Attached game process ended; exiting Helpy by preference.");requestQuit();}
            const HANDLE wake[]{activate,game.WaitHandle()};
            if(WaitForMultipleObjects(wake[1]?2:1,wake,FALSE,250)==WAIT_OBJECT_0)EnumWindows([](HWND window,LPARAM){DWORD pid=0;GetWindowThreadProcessId(window,&pid);wchar_t name[128]{};GetClassNameW(window,name,128);if(pid==GetCurrentProcessId()&&std::wstring_view(name)==L"RuneSchema.Helpy.Window"){ShowWindowAsync(window,SW_RESTORE);SetForegroundWindow(window);}return TRUE;},0);
        }
        worker.request_stop();worker.join();shutdown(instance);instance=nullptr;Bridge.Disconnect();module=nullptr;CloseHandle(activate);CloseHandle(singleton);return 0;
    }catch(const std::exception& error){
        worker.request_stop();if(worker.joinable())worker.join();
        if(instance&&shutdown)shutdown(instance);Bridge.Disconnect();if(module)FreeLibrary(module);if(activate)CloseHandle(activate);if(singleton)CloseHandle(singleton);
        Log(RS_LOG_ERROR,"RuneSchema.Helpy",("[RS-UI-002] "+std::string(error.what())).c_str());
        if(!smoke)MessageBoxA(nullptr,error.what(),"Helpy desktop",MB_OK|MB_ICONERROR);return 1;
    }
}

#endif
