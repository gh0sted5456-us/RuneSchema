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
    Check(argc==5);const auto module=LoadLibraryExW(fs::u8path(argv[1]).c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);Check(module!=nullptr);
    const auto resource=FindResourceW(module,MAKEINTRESOURCEW(IDR_CACHE_BUILDER),RT_RCDATA);Check(resource!=nullptr);
    const auto loaded=LoadResource(module,resource);Check(loaded!=nullptr);const auto data=static_cast<const char*>(LockResource(loaded));
    const auto expected=PS::ConfigFiles::Read(fs::u8path(argv[2]),2*1024*1024);Check(data&&std::string_view(data,SizeofResource(module,resource))==expected);
    const auto repair=FindResourceW(module,MAKEINTRESOURCEW(IDR_RUNESCHEMA_REPAIR),RT_RCDATA);Check(repair!=nullptr);const auto repairData=static_cast<const char*>(LockResource(LoadResource(module,repair)));
    Check(repairData&&std::string_view(repairData,SizeofResource(module,repair))==PS::ConfigFiles::Read(fs::u8path(argv[3]),2*1024*1024));
    const auto wheel=FindResourceW(module,MAKEINTRESOURCEW(IDR_LOADING_WHEEL),RT_RCDATA);Check(wheel!=nullptr);
    const auto wheelData=static_cast<const char*>(LockResource(LoadResource(module,wheel)));
    Check(wheelData&&std::string_view(wheelData,SizeofResource(module,wheel))==PS::ConfigFiles::Read(fs::u8path(argv[4]),1024*1024));FreeLibrary(module);
    const auto root=fs::temp_directory_path()/("RS-cache-builder-"+std::to_string(GetCurrentProcessId()));fs::create_directories(root/"mods/Mod (test)");
    try{
        // Spaces, shell metacharacters and Unicode remain literal process arguments.
        const auto command=L"powershell.exe"+Arguments(root/L"builder.ps1",root/L"source & files.json",true,root/L"progress.json",root/L"control.json",true,root/L"output.zip","WinGDK");
        int count=0;auto** args=CommandLineToArgvW(command.c_str(),&count);Check(args&&count>12);
        bool input=false,output=false,setup=false;for(int i=0;i<count;++i){input|=std::wstring(args[i])==(root/L"source & files.json").wstring();output|=std::wstring(args[i])==(root/L"output.zip").wstring();setup|=std::wstring(args[i])==L"-InstallDotNet";}LocalFree(args);Check(input&&output&&setup);
        Check(command.find(L"-AllowRunningGame")==command.npos);
        const auto installed=Arguments(root/L"builder.ps1",root/L"mods/Example",false,root/L"progress.json",root/L"control.json",false);
        Check(installed.find(L"-AllowRunningGame")!=installed.npos&&installed.find(L"-CreationRequest")==installed.npos);
        const auto vanilla=Arguments(root/L"builder.ps1",root,false,root/L"progress.json",root/L"control.json",false,root/L"AppData/cache","Win64",root/L"backups",true);
        Check(vanilla.find(L"-BaseGame -OutputDirectory")!=vanilla.npos&&vanilla.find(L"-AllowRunningGame")!=vanilla.npos);
        fs::create_directories(root/"mods/Example");Check(InstalledFolder(root,root/"mods/Example","Example")==fs::absolute(root/"mods/Example"));
        bool refused=false;try{InstalledFolder(root,root/"mods/Example","../Example");}catch(...){refused=true;}Check(refused);
        refused=false;try{InstalledFolder(root,root,"Example");}catch(...){refused=true;}Check(refused);
        Check(!StartInstalled(root,root/"mods/Missing","Missing",false)&&!Current().Running);
        Check(!StartRepair(root/"Missing","0.8.1.0")&&Current().Repair&&Current().State=="Failed");
        Check(ReadyBundle().empty());Shutdown();
        const auto row=[&](const char* name,bool enabled,bool cooked){return nlohmann::json{{"Name",name},{"Enabled",enabled},{"Present",true},{"Folder",(root/"mods"/name).string()},{"Loaders",cooked?nlohmann::json::array({"Unreal PAK"}):nlohmann::json::array()}};};
        Check(StartEnabled(root,nlohmann::json::array({row("MissingA",true,true),row("MissingB",true,true),row("JsonOnly",true,false),row("Disabled",false,true)}),false));
        Check(CurrentBatch().Total==3&&CurrentBatch().Skipped==1&&CurrentBatch().Active);
        Control("pause");Poll();Check(CurrentBatch().Failed==0&&CurrentBatch().Paused);
        Control("run");Poll();Check(CurrentBatch().Failed==1&&!Current().Running);Poll();Check(CurrentBatch().Failed==2);Poll();Check(!CurrentBatch().Active);
        Check(StartEnabled(root,nlohmann::json::array({row("MissingA",true,true)}),false));Control("stop");Poll();Check(!CurrentBatch().Active&&CurrentBatch().Failed==0);Shutdown();
    }catch(...){fs::remove_all(root);throw;}fs::remove_all(root);
}
