#pragma once
#include "HelpyDesktopTransport.h"
#include "Core/ConfigFiles.h"
#include "Core/RuneSchemaDataRoot.h"
#include <shlobj.h>
#include <exdisp.h>
#include <shldisp.h>
#include <shlguid.h>
#include <wrl/client.h>
#include <memory>
#include <appmodel.h>

namespace PS::HelpyDesktopLaunch {
namespace fs=std::filesystem;
inline std::wstring InstanceName(const fs::path& root){const auto pipe=HelpyDesktop::PipeName(root);return L"Local\\"+pipe.substr(pipe.find_last_of(L'\\')+1);}
inline bool ActivateExisting(const fs::path& root,bool show){
    const auto name=InstanceName(root);const auto instance=OpenMutexW(SYNCHRONIZE,FALSE,name.c_str());if(!instance)return false;CloseHandle(instance);
    if(show){const auto event=OpenEventW(EVENT_MODIFY_STATE,FALSE,(name+L".activate").c_str());if(event){SetEvent(event);CloseHandle(event);}}return true;
}
inline fs::path Application(const fs::path& root,const std::string& platform,const fs::path& local){
    const auto name=HelpyDesktop::PipeName(root);const auto folder=RuneSchemaData::Resolve(local)/RuneSchemaData::RuntimeKey(platform)/"apps"/name.substr(name.find_last_of(L'.')+1);
    RuneSchemaData::Safe(folder);return folder/"Helpy.exe";
}
inline fs::path Stage(const fs::path& root,const std::string& platform,const fs::path& local){
    const auto source=root/"plugins/RuneSchema.Helpy/dll/Helpy.exe",target=Application(root,platform,local);
    const auto bytes=ConfigFiles::Read(source,8*1024*1024);if(bytes.empty())throw std::runtime_error("Helpy.exe is empty");
    if(!fs::exists(target)||ConfigFiles::Read(target,8*1024*1024)!=bytes)ConfigFiles::Write(target,bytes);
    if(ConfigFiles::Read(target,8*1024*1024)!=bytes)throw std::runtime_error("Helpy application copy did not verify");
    const auto encoded=root.u8string();const auto binding=target.parent_path()/"runtime.json";const auto text=HelpyDesktop::Json{{"Version",1},{"Root",std::string(encoded.begin(),encoded.end())},{"Platform",platform}}.dump();
    if(!fs::exists(binding)||ConfigFiles::Read(binding,16384)!=text)ConfigFiles::Write(binding,text);return target;
}
inline bool InGameContext(){
    UINT32 length=0;const auto package=GetCurrentPackageFullName(&length,nullptr);
    if(package==ERROR_INSUFFICIENT_BUFFER)return true;
    if(package!=APPMODEL_ERROR_NO_PACKAGE)throw std::runtime_error("Cannot verify Helpy's Windows package identity");
    return GetEnvironmentVariableW(L"SteamAppId",nullptr,0)||GetEnvironmentVariableW(L"SteamGameId",nullptr,0)||GetEnvironmentVariableW(L"SteamOverlayGameId",nullptr,0);
}
inline std::wstring Arguments(const fs::path& root,const std::string& platform,bool hidden){
    const auto text=root.wstring();if(text.find_first_of(L"\"\r\n")!=text.npos)throw std::runtime_error("Invalid assigned runtime path");
    const auto key=RuneSchemaData::RuntimeKey(platform);if(key=="core/runtime/unknown")throw std::runtime_error("Choose a Helpy runtime");
    return L"--runtime-root \""+text+L"\" --platform "+std::wstring(platform.begin(),platform.end())+(hidden?L" --start-hidden":L"");
}
inline void FromDesktop(const fs::path& executable,const std::wstring& arguments){
    // Explorer supplies its own package/job/environment instead of inheriting the game session.
    const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    struct Apartment{HRESULT Result;~Apartment(){if(SUCCEEDED(Result))CoUninitialize();}} apartment{initialized};
    if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)throw std::runtime_error("Windows desktop automation is unavailable");
    const auto check=[](HRESULT result){if(FAILED(result))throw std::runtime_error("Could not ask the Windows desktop to launch Helpy");};
    using Microsoft::WRL::ComPtr;
    ComPtr<IShellWindows> windows;check(CoCreateInstance(CLSID_ShellWindows,nullptr,CLSCTX_LOCAL_SERVER,IID_PPV_ARGS(&windows)));
    VARIANT location{},empty{};location.vt=VT_I4;location.lVal=CSIDL_DESKTOP;long window=0;ComPtr<IDispatch> desktop;
    check(windows->FindWindowSW(&location,&empty,SWC_DESKTOP,&window,SWFO_NEEDDISPATCH,&desktop));if(!desktop)throw std::runtime_error("Windows Explorer desktop is unavailable");
    ComPtr<IServiceProvider> services;check(desktop.As(&services));ComPtr<IShellBrowser> browser;check(services->QueryService(SID_STopLevelBrowser,IID_PPV_ARGS(&browser)));
    ComPtr<IShellView> view;check(browser->QueryActiveShellView(&view));ComPtr<IDispatch> background;check(view->GetItemObject(SVGIO_BACKGROUND,IID_PPV_ARGS(&background)));
    ComPtr<IShellFolderViewDual> folder;check(background.As(&folder));ComPtr<IDispatch> application;check(folder->get_Application(&application));ComPtr<IShellDispatch2> shell;check(application.As(&shell));
    using Text=std::unique_ptr<OLECHAR,decltype(&SysFreeString)>;
    Text file(SysAllocString(executable.c_str()),SysFreeString),args(SysAllocString(arguments.c_str()),SysFreeString),directory(SysAllocString(executable.parent_path().c_str()),SysFreeString),operation(SysAllocString(L"open"),SysFreeString);
    if(!file||!args||!directory||!operation)throw std::bad_alloc();
    VARIANT a{},d{},o{},show{};a.vt=d.vt=o.vt=VT_BSTR;a.bstrVal=args.get();d.bstrVal=directory.get();o.bstrVal=operation.get();show.vt=VT_I4;show.lVal=SW_SHOWNOACTIVATE;
    check(shell->ShellExecute(file.get(),a,d,o,show));
}
inline void Start(const fs::path& root,const std::string& platform,bool hidden){
    if(ActivateExisting(root,!hidden))return;
    const auto executable=Stage(root,platform,RuneSchemaData::LocalDirectory());FromDesktop(executable,Arguments(root,platform,hidden));
}
}
