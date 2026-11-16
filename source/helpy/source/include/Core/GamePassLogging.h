#pragma once
#include "Core/LiveLog.h"
#include "Core/ConfigFiles.h"
#include "Core/RuneSchemaDataRoot.h"
#include <shlobj.h>
#include <wrl/client.h>

namespace PS::GamePassLogging {
namespace fs=std::filesystem;
struct Setup{fs::path Executable,Log;};
inline Setup For(const fs::path& root,const fs::path& local,bool centralized=DiagnosticLogPaths::UseUE4SS){
    auto log=LiveLog::GamePassLog(root,local);if(log.empty()||!root.is_absolute()||!local.is_absolute())throw std::runtime_error("Assign Helpy to a Game Pass WinGDK installation first");
    log=centralized?DiagnosticLogPaths::UE4SS(root)/"RSDragonwilds.log":local/"RSDragonwilds/RuneSchema/gamepass/diagnostics/RSDragonwilds.log";
    const auto project=root.parent_path().parent_path().parent_path().parent_path().parent_path();
    if(LiveLog::Lower(project.filename().string())!="rsdragonwilds")throw std::runtime_error("Game Pass project directory is unrecognized");
    return {project.parent_path()/"RSDragonwilds.exe",log};
}
inline std::wstring Arguments(const Setup& setup){
    const auto path=setup.Log.wstring();if(path.empty()||path.size()>30000||path.find_first_of(L"\"\r\n")!=path.npos||path.find(L'\0')!=path.npos)throw std::runtime_error("Invalid game log destination");
    return L"-log -ABSLOG=\""+path+L"\"";
}
inline std::string Quote(const std::wstring& text){
    if(text.find_first_of(L"\r\n")!=text.npos||text.find(L'\0')!=text.npos)throw std::runtime_error("Invalid launch command path");
    const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    if(!count&&!text.empty())throw std::runtime_error("Cannot encode launch command");std::string encoded(count,'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),encoded.data(),count,nullptr,nullptr);
    std::string quoted="'";for(char c:encoded){quoted+=c;if(c=='\'')quoted+=c;}return quoted+"'";
}
inline std::string Command(const Setup& setup){return "Start-Process -FilePath "+Quote(setup.Executable.wstring())+" -ArgumentList "+Quote(Arguments(setup))+" -WorkingDirectory "+Quote(setup.Executable.parent_path().wstring());}
inline void Prepare(const Setup& setup){
    if(!fs::is_regular_file(setup.Executable))throw std::runtime_error("Game Pass RSDragonwilds.exe was not found in this installation");
    RuneSchemaData::Safe(setup.Log.parent_path());fs::create_directories(setup.Log.parent_path());
}
inline void SaveShortcut(const Setup& setup,const fs::path& destination){
    if(!destination.is_absolute()||LiveLog::Lower(destination.extension().string())!=".lnk")throw std::runtime_error("Choose a Windows .lnk shortcut filename");Prepare(setup);
    const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);struct Apartment{HRESULT Result;~Apartment(){if(SUCCEEDED(Result))CoUninitialize();}} apartment{initialized};
    if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)throw std::runtime_error("Windows shortcut creation is unavailable");
    const auto check=[](HRESULT result){if(FAILED(result))throw std::runtime_error("Could not save the Game Pass logging shortcut");};
    Microsoft::WRL::ComPtr<IShellLinkW> link;check(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link)));
    check(link->SetPath(setup.Executable.c_str()));check(link->SetArguments(Arguments(setup).c_str()));check(link->SetWorkingDirectory(setup.Executable.parent_path().c_str()));check(link->SetDescription(L"Launch Game Pass Dragonwilds with native game logging"));
    Microsoft::WRL::ComPtr<IPersistFile> file;check(link.As(&file));GUID id{};check(CoCreateGuid(&id));wchar_t suffix[40]{};StringFromGUID2(id,suffix,40);
    const auto temporary=fs::path(destination.wstring()+suffix+L".tmp");struct Cleanup{fs::path File;~Cleanup(){std::error_code error;fs::remove(File,error);}} cleanup{temporary};
    if(fs::exists(temporary))throw std::runtime_error("Shortcut staging path is already in use");check(file->Save(temporary.c_str(),TRUE));
    const auto bytes=ConfigFiles::Read(temporary,1024*1024);const bool existed=fs::exists(destination);const auto previous=existed?ConfigFiles::Read(destination,1024*1024):std::string{};
    if(existed)ConfigFiles::Backup(destination);
    if(fs::exists(destination)!=existed||(existed&&ConfigFiles::Read(destination,1024*1024)!=previous))throw std::runtime_error("Shortcut changed while saving; choose a new filename");
    ConfigFiles::Write(destination,bytes);
}
}
