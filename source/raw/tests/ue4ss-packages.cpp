#include "Core/UE4SSPackages.h"
#include <cassert>
#include <chrono>
int main(int argc,char** argv){
    assert(argc==2);namespace fs=std::filesystem;using namespace PS::UE4SSPackages;
    const auto root=fs::absolute(argv[1])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());fs::create_directories(root);
    const auto file=[](const fs::path& p){fs::create_directories(p.parent_path());std::ofstream out(p);out<<"fixture";};
    file(root/"MaxCape/ue4ss/MaxCape/Scripts/main.lua");file(root/"MaxCape/ue4ss/MaxCape/config.txt");file(root/"MaxCape/ue4ss/MaxCape/modmenu.json");
    auto packages=Discover(root/"MaxCape","MaxCape");assert(packages.size()==1&&packages[0].Name=="MaxCape"&&packages[0].Main.filename()=="main.lua");
    assert(fs::is_regular_file(packages[0].Root/"config.txt")&&fs::is_regular_file(packages[0].Root/"modmenu.json"));
    file(root/"MaxCape/ue4ss/NativePart/dlls/main.dll");packages=Discover(root/"MaxCape","MaxCape");assert(packages.size()==2&&!packages[1].Native.empty());
    const auto manifest=Manifest(root,packages);assert(manifest.find("MaxCape/ue4ss/MaxCape\tScripts/main.lua\tlua")!=std::string::npos&&manifest.find("\tnative\n")!=std::string::npos);
    file(root/"Legacy/ue4ss/scripts/main.lua");assert(Discover(root/"Legacy","Legacy").size()==1);
    file(root/"Legacy/ue4ss/Extra/Scripts/main.lua");bool rejected=false;try{Discover(root/"Legacy","Legacy");}catch(...){rejected=true;}assert(rejected);
    assert(!NameSafe("../escape")&&!NameSafe("bad\tname"));
    file(root/"NativeOnly/ue4ss/A/dlls/A.dll");packages=Discover(root/"NativeOnly","NativeOnly");assert(packages.size()==1&&packages[0].Main.empty()&&!packages[0].Native.empty());
    file(root/"Hybrid/ue4ss/A/Scripts/main.lua");file(root/"Hybrid/ue4ss/A/dlls/main.dll");
    packages=Discover(root/"Hybrid","Hybrid");assert(packages.size()==1&&!packages[0].Main.empty()&&!packages[0].Native.empty());
    assert(Manifest(root,packages).find("\tnative\n")!=std::string::npos);
    assert(Manifest(root,{}).find("\tlua\n")==std::string::npos); // Disabled parents produce no executable rows.
    file(root/"CustomDll/ue4ss/A/Scripts/main.lua");file(root/"CustomDll/ue4ss/A/dlls/custom.DLL");
    packages=Discover(root/"CustomDll","CustomDll");assert(packages.size()==1&&!packages[0].Native.empty());
}
