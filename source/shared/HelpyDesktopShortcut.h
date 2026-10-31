#pragma once
#include "Core/ConfigFiles.h"
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <filesystem>
#include <stdexcept>

namespace PS::HelpyDesktopShortcut {
inline bool CreateOnce(const std::filesystem::path& executable,const std::filesystem::path& desktop,const std::string& platform,const std::filesystem::path& marker,const std::filesystem::path& hosted={},const std::wstring& arguments={}){
    namespace fs=std::filesystem;if(fs::exists(marker))return false;
    const wchar_t* label=platform=="steam"?L"Steam":platform=="gamepass"?L"Game Pass":platform=="epic"?L"Epic Games":platform=="server"?L"Server":nullptr;
    if(!label||!fs::is_regular_file(executable)||executable.filename()!=L"Helpy.exe"||!fs::is_directory(desktop))throw std::runtime_error("Helpy desktop shortcut location is unavailable");
    const auto target=desktop/(std::wstring(L"RuneSchema Helpy (")+label+L").lnk");
    const auto launch=hosted.empty()?executable:hosted;
    if(!fs::is_regular_file(launch))throw std::runtime_error("Helpy desktop launcher is unavailable");
    const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)throw std::runtime_error("Windows shortcut service unavailable");
    struct Apartment{bool Owned;~Apartment(){if(Owned)CoUninitialize();}} apartment{SUCCEEDED(initialized)};
    bool replace=false;
    if(fs::exists(target)){
        Microsoft::WRL::ComPtr<IShellLinkW> old;Microsoft::WRL::ComPtr<IPersistFile> saved;wchar_t path[32768]{},description[256]{};
        if(!hosted.empty()&&SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&old)))&&SUCCEEDED(old.As(&saved))&&SUCCEEDED(saved->Load(target.c_str(),STGM_READ))&&SUCCEEDED(old->GetPath(path,32768,nullptr,SLGP_RAWPATH))&&SUCCEEDED(old->GetDescription(description,256))){std::error_code error;replace=fs::equivalent(path,executable,error)&&!error&&std::wstring_view(description)==L"Open RuneSchema Helpy with or without Dragonwilds running.";}
        if(!replace){ConfigFiles::Write(marker,"Existing desktop shortcut preserved.\n");return false;}
    }
    Microsoft::WRL::ComPtr<IShellLinkW> link;Microsoft::WRL::ComPtr<IPersistFile> file;
    if(FAILED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link)))||FAILED(link.As(&file))
        ||FAILED(link->SetPath(launch.c_str()))||FAILED(link->SetArguments(arguments.c_str()))||FAILED(link->SetWorkingDirectory(executable.parent_path().c_str()))
        ||FAILED(link->SetIconLocation(executable.c_str(),0))||FAILED(link->SetDescription(L"Open RuneSchema Helpy with or without Dragonwilds running.")))throw std::runtime_error("Cannot prepare Helpy desktop shortcut");
    GUID id{};if(FAILED(CoCreateGuid(&id)))throw std::runtime_error("Cannot reserve Helpy shortcut filename");wchar_t token[40]{};StringFromGUID2(id,token,40);const auto pending=desktop/(std::wstring(L".Helpy-")+token+L".tmp");
    try{if(FAILED(file->Save(pending.c_str(),TRUE)))throw std::runtime_error("Cannot save Helpy desktop shortcut");
        if(!(replace?MoveFileExW(pending.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH):MoveFileW(pending.c_str(),target.c_str()))){if(replace||(GetLastError()!=ERROR_ALREADY_EXISTS&&GetLastError()!=ERROR_FILE_EXISTS))throw std::runtime_error("Cannot publish Helpy desktop shortcut");fs::remove(pending);}
        ConfigFiles::Write(marker,"Desktop shortcut setup completed.\n");return true;
    }catch(...){std::error_code error;fs::remove(pending,error);throw;}
}
inline bool Ensure(const std::filesystem::path& executable,const std::string& platform,const std::filesystem::path& stateDirectory){
    const auto marker=stateDirectory/"desktop-shortcut-runtime-v2.done";if(std::filesystem::exists(marker))return false;
    PWSTR value=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_Desktop,0,nullptr,&value)))throw std::runtime_error("Windows Desktop folder unavailable");const std::filesystem::path desktop(value);CoTaskMemFree(value);
    // Launching an EXE inside Game Pass's package can activate Gaming Services instead.
    if(platform=="gamepass"){
        const auto root=executable.parent_path().parent_path().parent_path().parent_path();
        return CreateOnce(executable,desktop,platform,marker,stateDirectory/"Helpy.exe",L"--runtime-root \""+root.wstring()+L"\" --platform gamepass");
    }
    return CreateOnce(executable,desktop,platform,marker);
}
}
