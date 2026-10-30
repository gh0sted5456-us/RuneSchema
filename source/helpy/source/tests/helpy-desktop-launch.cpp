#include "HelpyDesktopLaunch.h"
#include "Core/RsmkResources.h"
#include <winver.h>
#include <cstring>
#include <cassert>
#include <iostream>
int main(int argc,char** argv)try{
    if(argc==5&&std::string_view(argv[1])=="--stage"){
        const auto file=PS::HelpyDesktopLaunch::Stage(std::filesystem::u8path(argv[2]),argv[3],std::filesystem::u8path(argv[4])).u8string();
        std::cout<<PS::HelpyDesktop::Json{{"Executable",std::string(file.begin(),file.end())}}.dump()<<'\n';return 0;
    }
    assert(argc==3);namespace fs=std::filesystem;using namespace PS;using namespace HelpyDesktopLaunch;
    const auto executable=fs::u8path(argv[2]);DWORD ignored=0;
    std::vector<char> version(GetFileVersionInfoSizeW(executable.c_str(),&ignored));assert(!version.empty()&&GetFileVersionInfoW(executable.c_str(),0,static_cast<DWORD>(version.size()),version.data()));
    UINT length=0;VS_FIXEDFILEINFO* fixed=nullptr;assert(VerQueryValueW(version.data(),L"\\",reinterpret_cast<void**>(&fixed),&length)&&length>=sizeof(*fixed)&&fixed->dwFileType==VFT_APP);
    wchar_t* product=nullptr;assert(VerQueryValueW(version.data(),L"\\StringFileInfo\\040904b0\\ProductName",reinterpret_cast<void**>(&product),&length)&&std::wstring_view(product)==L"RuneSchema Helpy");
    const auto resources=LoadLibraryExW(executable.c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE);assert(resources);
    const auto group=FindResourceW(resources,MAKEINTRESOURCEW(IDI_RSMK),RT_GROUP_ICON);assert(group);
    const auto bytes=SizeofResource(resources,group);const auto* icons=static_cast<const unsigned char*>(LockResource(LoadResource(resources,group)));assert(icons&&bytes>=6);
    uint16_t count=0;std::memcpy(&count,icons+4,2);assert(count>=10&&bytes>=6+14u*count);
    bool large=false;for(unsigned i=0;i<count;++i)large|=icons[6+14*i]==0&&icons[7+14*i]==0;assert(large);FreeLibrary(resources);
    const auto fixture=fs::temp_directory_path()/("HelpyLaunch-"+std::to_string(GetCurrentProcessId()));assert(fs::create_directory(fixture));
    struct Cleanup{fs::path Root;~Cleanup(){std::error_code error;fs::remove_all(Root,error);}} cleanup{fixture};
    const auto root=fixture/"Game O'Brien/RSDragonwilds/Binaries/WinGDK/ue4ss/Mods/RuneSchema",local=fixture/"User";
    const auto source=root/"plugins/RuneSchema.Helpy/dll/Helpy.exe";fs::create_directories(source.parent_path());fs::copy_file(argv[1],source);
    const auto app=Stage(root,"gamepass",local);assert(app!=source&&ConfigFiles::Read(app)==ConfigFiles::Read(source));
    const auto stamp=fs::last_write_time(app);assert(Stage(root,"gamepass",local)==app&&fs::last_write_time(app)==stamp);
    assert(Application(root,"steam",local)!=app);const auto binding=HelpyDesktop::Json::parse(ConfigFiles::Read(app.parent_path()/"runtime.json"));assert(binding.at("Platform")=="gamepass");
    assert(Arguments(root,"gamepass",true).find(L"--start-hidden")!=std::wstring::npos);
    const auto name=InstanceName(root);const auto instance=CreateMutexW(nullptr,FALSE,name.c_str()),activate=CreateEventW(nullptr,FALSE,FALSE,(name+L".activate").c_str());assert(instance&&activate);
    assert(ActivateExisting(root,false)&&WaitForSingleObject(activate,0)==WAIT_TIMEOUT);assert(ActivateExisting(root,true)&&WaitForSingleObject(activate,0)==WAIT_OBJECT_0);CloseHandle(instance);CloseHandle(activate);
    wchar_t previous[32768]{};const auto previousSize=GetEnvironmentVariableW(L"SteamGameId",previous,32768);SetEnvironmentVariableW(L"SteamGameId",L"RuneSchema-launch-test");assert(InGameContext());
    const auto report=fixture/"child.json";
    try{FromDesktop(app,L"\""+report.wstring()+L"\"");}catch(...){SetEnvironmentVariableW(L"SteamGameId",previousSize?previous:nullptr);throw;}
    SetEnvironmentVariableW(L"SteamGameId",previousSize?previous:nullptr);
    for(int i=0;i<200&&!fs::exists(report);++i)Sleep(25);assert(fs::is_regular_file(report));
    const auto result=HelpyDesktop::Json::parse(ConfigFiles::Read(report));assert(result.at("PackageResult")==APPMODEL_ERROR_NO_PACKAGE&&!result.at("InheritedSteamContext").get<bool>());
    std::cout<<"Desktop launch is outside the game package and does not inherit Steam session state.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
