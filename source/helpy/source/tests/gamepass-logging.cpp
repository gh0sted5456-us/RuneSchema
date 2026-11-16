#include "Core/GamePassLogging.h"
#include <cassert>
#include <iostream>
int main()try{
    namespace fs=std::filesystem;using namespace PS;
    const auto fixture=fs::temp_directory_path()/("GamePassLogging-"+std::to_string(GetCurrentProcessId()));assert(fs::create_directory(fixture));
    struct Cleanup{fs::path Root;~Cleanup(){std::error_code error;fs::remove_all(Root,error);}} cleanup{fixture};
    const auto root=fixture/"Game O'Brien/RSDragonwilds/Binaries/WinGDK/ue4ss/Mods/RuneSchema";fs::create_directories(root);
    const auto setup=GamePassLogging::For(root,fixture/"Local User");ConfigFiles::Write(setup.Executable,"test fixture; never executed");
    assert(GamePassLogging::Arguments(setup).find(L"-ABSLOG=\"")!=std::wstring::npos&&GamePassLogging::Command(setup).find("O''Brien")!=std::string::npos);
    const auto shortcut=fixture/"Game log.lnk";GamePassLogging::SaveShortcut(setup,shortcut);assert(fs::is_regular_file(shortcut));
    const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);assert(SUCCEEDED(initialized));
    {Microsoft::WRL::ComPtr<IShellLinkW> link;assert(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link))));Microsoft::WRL::ComPtr<IPersistFile> file;assert(SUCCEEDED(link.As(&file))&&SUCCEEDED(file->Load(shortcut.c_str(),STGM_READ)));
        wchar_t target[32768]{},arguments[32768]{};assert(SUCCEEDED(link->GetPath(target,32768,nullptr,SLGP_RAWPATH))&&fs::path(target)==setup.Executable);assert(SUCCEEDED(link->GetArguments(arguments,32768))&&arguments==GamePassLogging::Arguments(setup));}
    CoUninitialize();GamePassLogging::SaveShortcut(setup,shortcut);
    bool invalid=false;try{GamePassLogging::SaveShortcut(setup,fixture/"wrong.exe");}catch(...){invalid=true;}assert(invalid&&!fs::exists(fixture/"wrong.exe"));
    const auto standard=fixture/"Local User/RSDragonwilds/Saved/Logs/RSDragonwilds.log";ConfigFiles::Write(standard,"Steam log");ConfigFiles::Write(setup.Log,"Game Pass log");
    fs::last_write_time(standard,fs::file_time_type::clock::now()+std::chrono::seconds(5));assert(LiveLog::ResolveGameLog(root,fixture/"Local User")==setup.Log);
    assert(LiveLog::ProtectedExport(setup.Log,root));
    fs::remove(setup.Log);assert(LiveLog::ResolveGameLog(root,fixture/"Local User")==standard);
    const auto centralized=GamePassLogging::For(root,fixture/"Local User",true);
    assert(centralized.Log==root.parent_path().parent_path()/"RSDragonwilds.log");
    DiagnosticLogPaths::UseUE4SS=true;
    assert(LiveLog::LogPath(root,LiveLog::Stream::RuneSchema)==centralized.Log.parent_path()/"RuneSchema.log");
    assert(LiveLog::ArchivePath(root)==centralized.Log.parent_path()/"logs");
    assert(LiveLog::ProtectedExport(centralized.Log,root)&&LiveLog::ProtectedExport(centralized.Log.parent_path()/"Helpy-Desktop.log.previous",root));
    GamePassLogging::SaveShortcut(centralized,fixture/"Central log.lnk");ConfigFiles::Write(centralized.Log,"centralized native log");
    assert(LiveLog::ResolveGameLog(root,fixture/"Local User")==centralized.Log);
    assert(GamePassLogging::For(root,fixture/"Local User",false).Log==setup.Log);
    DiagnosticLogPaths::UseUE4SS=false;
    std::cout<<"Game Pass logging shortcut, quoting, backups and runtime log selection passed.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
