#include "HelpyDesktopShortcut.h"
#include <fstream>
#include <stdexcept>
static void Check(bool value){if(!value)throw std::runtime_error("Offline desktop shortcut regression");}
int main(){namespace fs=std::filesystem;const auto root=fs::temp_directory_path()/("RS-shortcut-"+std::to_string(GetCurrentProcessId()));fs::create_directories(root/"apps/bound");fs::create_directories(root/"runtime/plugins/RuneSchema.Helpy/dll");fs::create_directories(root/"Desktop");
    const auto executable=root/"runtime/plugins/RuneSchema.Helpy/dll/Helpy.exe",marker=root/"apps/bound/desktop-shortcut.done";std::ofstream(executable)<<"inert";
    try{Check(PS::HelpyDesktopShortcut::CreateOnce(executable,root/"Desktop","gamepass",marker));const auto path=root/"Desktop/RuneSchema Helpy (Game Pass).lnk";Check(fs::is_regular_file(path));
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);Microsoft::WRL::ComPtr<IShellLinkW> link;Microsoft::WRL::ComPtr<IPersistFile> file;
        Check(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link)))&&SUCCEEDED(link.As(&file))&&SUCCEEDED(file->Load(path.c_str(),STGM_READ)));
        wchar_t target[32768]{},args[1024]{};Check(SUCCEEDED(link->GetPath(target,32768,nullptr,SLGP_RAWPATH))&&fs::equivalent(target,executable));Check(SUCCEEDED(link->GetArguments(args,1024))&&!*args);file.Reset();link.Reset();CoUninitialize();
        fs::remove(marker);const auto hosted=root/"apps/bound/Helpy.exe";std::ofstream(hosted)<<"inert";const std::wstring assigned=L"--runtime-root \""+(root/"runtime").wstring()+L"\" --platform gamepass";
        Check(PS::HelpyDesktopShortcut::CreateOnce(executable,root/"Desktop","gamepass",marker,hosted,assigned));
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);Check(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link)))&&SUCCEEDED(link.As(&file))&&SUCCEEDED(file->Load(path.c_str(),STGM_READ)));
        Check(SUCCEEDED(link->GetPath(target,32768,nullptr,SLGP_RAWPATH))&&fs::equivalent(target,hosted));Check(SUCCEEDED(link->GetArguments(args,1024))&&assigned==args);file.Reset();link.Reset();CoUninitialize();
        const auto stamp=fs::last_write_time(path);Check(!PS::HelpyDesktopShortcut::CreateOnce(executable,root/"Desktop","gamepass",marker,hosted,assigned)&&fs::last_write_time(path)==stamp);
        fs::remove(marker);std::ofstream(path,std::ios::binary|std::ios::trunc)<<"user shortcut";Check(!PS::HelpyDesktopShortcut::CreateOnce(executable,root/"Desktop","gamepass",marker));Check(PS::ConfigFiles::Read(path,100)=="user shortcut");
    }catch(...){fs::remove_all(root);throw;}fs::remove_all(root);
}
