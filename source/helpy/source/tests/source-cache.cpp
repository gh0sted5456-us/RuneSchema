#include "Core/HelpySourceCache.h"
#include <cassert>
#include <iostream>
#undef assert
#define assert(value) ((value)?static_cast<void>(0):throw std::runtime_error(#value))
int main(int argc,char** argv)try{namespace fs=std::filesystem;using namespace PS::HelpySourceCache;
    if(argc==2){const auto start=std::chrono::steady_clock::now();try{const auto source=Inspect(argv[1]);std::cout<<source.Key<<" files="<<source.Files.size()<<" ms="<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()<<'\n';return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
    const auto base=fs::temp_directory_path()/("helpy-source-"+std::to_string(GetCurrentProcessId()));
    const auto root=base/"Binaries"/"WinGDK"/"ue4ss"/"Mods"/"RuneSchema";
    fs::create_directories(root/"mods"/"Custom"/"assets");const auto file=root/"mods"/"Custom"/"assets"/"item.jsonc";
    {std::ofstream f(file);f<<"{\"PowerLevel\":1}";}const auto stamp=fs::last_write_time(file);const auto a=Inspect(root),same=Inspect(root);assert(a.Key==same.Key&&a.Files.size()==1);
    // Physical installation aliases/copies must agree in both storefront lanes.
    for(const auto* lane:{"WinGDK","Win64"}) {
        const auto original=base/"original"/"Binaries"/lane/"ue4ss"/"Mods"/"RuneSchema";
        const auto alias=base/"alias"/"Binaries"/lane/"ue4ss"/"Mods"/"RuneSchema";
        for(const auto& candidate:{original,alias}) {
            fs::create_directories(candidate/"mods"/"Custom"/"assets");
            const auto copy=candidate/"mods"/"Custom"/"assets"/"item.jsonc";
            fs::copy_file(file,copy);fs::last_write_time(copy,stamp);
        }
        const auto identity=Inspect(original);assert(identity.Key==Inspect(alias).Key);
        if(std::string(lane)=="WinGDK")assert(identity.Key==a.Key);else assert(identity.Key!=a.Key);
    }
    const auto longRoot=NativePath(base/std::string(120,'a')/std::string(120,'b')/"Binaries"/"WinGDK"/"ue4ss"/"Mods"/"RuneSchema");
    fs::create_directories(longRoot/"mods"/"Custom"/"assets");
    const auto longFile=longRoot/"mods"/"Custom"/"assets"/"item.jsonc";
    fs::copy_file(file,longFile);fs::last_write_time(longFile,stamp);
    assert(Inspect(longRoot).Key==a.Key);
    {std::ofstream f(file);f<<"{\"PowerLevel\":2}";}fs::last_write_time(file,stamp);const auto edit=Inspect(root);assert(edit.Key!=a.Key);
    fs::create_directories(root/"runtime"/"saved");{std::ofstream f(root/"runtime"/"saved"/"ignored.json");f<<"{}";}assert(Inspect(root).Key==edit.Key);
    fs::remove(file);const auto removed=Inspect(root);assert(removed.Key!=edit.Key&&removed.Files.empty());
    const auto list=root/"runeschema.txt";
    {std::ofstream f(list);f<<"Dye:1";}const auto enabled=Inspect(root);const auto listStamp=fs::last_write_time(list);
    {std::ofstream f(list);f<<"Dye:0";}fs::last_write_time(list,listStamp);assert(Inspect(root).Key!=enabled.Key);fs::remove(list);
    fs::create_directories(base/"other"/"Mods"/"RuneSchema");assert(Inspect(base/"other"/"Mods"/"RuneSchema").Key!=removed.Key);
    fs::remove_all(NativePath(base));
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
