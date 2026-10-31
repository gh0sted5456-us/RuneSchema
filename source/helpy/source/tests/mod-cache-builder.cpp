#include "Runtime/HelpyModCacheBuilder.h"
#include "Core/RsmkResources.h"
#include "Core/ConfigFiles.h"
#include <windows.h>
#include <shellapi.h>
#include <fstream>
#include <stdexcept>
static void Check(bool value){if(!value)throw std::runtime_error("Mod cache builder regression");}
int main(int argc,char** argv){
    namespace fs=std::filesystem;using namespace PS::HelpyModCacheBuilder;
    Check(argc==4);const auto module=LoadLibraryExW(fs::u8path(argv[1]).c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);Check(module!=nullptr);
    const auto resource=FindResourceW(module,MAKEINTRESOURCEW(IDR_CACHE_BUILDER),RT_RCDATA);Check(resource!=nullptr);
    const auto loaded=LoadResource(module,resource);Check(loaded!=nullptr);const auto data=static_cast<const char*>(LockResource(loaded));
    const auto expected=PS::ConfigFiles::Read(fs::u8path(argv[2]),2*1024*1024);Check(data&&std::string_view(data,SizeofResource(module,resource))==expected);
    const auto repair=FindResourceW(module,MAKEINTRESOURCEW(IDR_RUNESCHEMA_REPAIR),RT_RCDATA);Check(repair!=nullptr);const auto repairData=static_cast<const char*>(LockResource(LoadResource(module,repair)));
    Check(repairData&&std::string_view(repairData,SizeofResource(module,repair))==PS::ConfigFiles::Read(fs::u8path(argv[3]),2*1024*1024));FreeLibrary(module);
    const auto root=fs::temp_directory_path()/("RS-cache-builder-"+std::to_string(GetCurrentProcessId()));fs::create_directories(root/"mods/Mod (test)");
    try{
        // Spaces, shell metacharacters and Unicode remain literal process arguments.
        const auto command=L"powershell.exe"+Arguments(root/L"builder.ps1",root/L"source & files.json",true,root/L"progress.json",root/L"control.json",true,root/L"output.zip","WinGDK");
        int count=0;auto** args=CommandLineToArgvW(command.c_str(),&count);Check(args&&count>12);
        bool input=false,output=false,setup=false;for(int i=0;i<count;++i){input|=std::wstring(args[i])==(root/L"source & files.json").wstring();output|=std::wstring(args[i])==(root/L"output.zip").wstring();setup|=std::wstring(args[i])==L"-InstallDotNet";}LocalFree(args);Check(input&&output&&setup);
        fs::create_directories(root/"mods/Example");Check(InstalledFolder(root,root/"mods/Example","Example")==fs::absolute(root/"mods/Example"));
        bool refused=false;try{InstalledFolder(root,root/"mods/Example","../Example");}catch(...){refused=true;}Check(refused);
        refused=false;try{InstalledFolder(root,root,"Example");}catch(...){refused=true;}Check(refused);
        Check(!StartInstalled(root,root/"mods/Missing","Missing",false)&&!Current().Running);
        Check(!StartRepair(root/"Missing","0.8.1.0")&&Current().Repair&&Current().State=="Failed");
        Check(ReadyBundle().empty());Shutdown();
    }catch(...){fs::remove_all(root);throw;}fs::remove_all(root);
}
