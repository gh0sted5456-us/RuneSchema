#pragma once
#include "Utility/ModFolderLayout.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#endif

namespace PS::UE4SSPackages {
namespace fs=std::filesystem;
struct Package {std::string Owner,Name;fs::path Root,Main,Native;};
inline bool Linked(const fs::path& path){
    std::error_code ec;if(fs::is_symlink(fs::symlink_status(path,ec))||ec)return true;
#if defined(_WIN32)
    const auto attributes=GetFileAttributesW(path.c_str());
    return attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT)!=0;
#else
    return false;
#endif
}
inline bool NameSafe(const std::string& name){return !name.empty()&&name!="."&&name!=".."&&name.find_first_of("/\\:<>\"|?*\t\r\n")==std::string::npos&&std::none_of(name.begin(),name.end(),[](unsigned char c){return c<32;});}
inline fs::path EntryFile(const fs::path& root,const std::string& wanted){
    fs::path found;std::size_t count=0;
    for(const auto& entry:fs::directory_iterator(root)){
        if(++count>256)throw std::runtime_error("UE4SS package exceeds 256 immediate entries");
        if(!ModFolderLayout::EqualsInsensitive(entry.path().filename().string(),wanted))continue;
        if(!found.empty())throw std::runtime_error("Ambiguous UE4SS entry filename case");
        if(!entry.is_regular_file()||Linked(entry.path()))throw std::runtime_error("UE4SS entry must be an ordinary local file");
        found=entry.path();
    }return found;
}
inline Package Inspect(const fs::path& root,const std::string& owner,const std::string& name){
    if(Linked(root)||!NameSafe(owner)||!NameSafe(name))throw std::runtime_error("Unsafe UE4SS package root/name");
    Package result{owner,name,root,{},{}};
    const auto scripts=ModFolderLayout::FindChildDirectory(root,"scripts"),dlls=ModFolderLayout::FindChildDirectory(root,"dlls");
    if(!scripts.empty()){if(Linked(scripts))throw std::runtime_error("Linked UE4SS scripts directory refused");result.Main=EntryFile(scripts,"main.lua");}
    if(!dlls.empty()){
        if(Linked(dlls))throw std::runtime_error("Linked UE4SS DLL directory refused");
        result.Native=EntryFile(dlls,"main.dll");if(result.Native.empty())result.Native=EntryFile(dlls,name+".dll");
        if(result.Native.empty())for(const auto& entry:fs::directory_iterator(dlls)){
            if(ModFolderLayout::EqualsInsensitive(entry.path().extension().string(),".dll")){
                result.Native=entry.path();break; // Lua/native hybrids are unsupported even with a custom DLL filename.
            }
        }
    }
    return result;
}
inline std::vector<Package> Discover(const fs::path& mod,const std::string& owner){
    if(!NameSafe(owner)||Linked(mod))throw std::runtime_error("Unsafe RuneSchema owner directory");
    const auto container=ModFolderLayout::FindChildDirectory(mod,"ue4ss");if(container.empty())return {};
    if(Linked(container))throw std::runtime_error("Linked UE4SS container refused");
    const auto direct=Inspect(container,owner,owner);std::vector<Package> children;std::set<std::string> names;size_t count=0;
    for(const auto& entry:fs::directory_iterator(container)){
        if(++count>256)throw std::runtime_error("UE4SS container exceeds 256 entries");
        if(!entry.is_directory())continue;
        const auto name=entry.path().filename().string();
        if(ModFolderLayout::EqualsInsensitive(name,"scripts")||ModFolderLayout::EqualsInsensitive(name,"dlls"))continue;
        const auto package=Inspect(entry.path(),owner,name);
        if(package.Main.empty()&&package.Native.empty())continue;
        if(!names.insert(ModFolderLayout::FoldAscii(name)).second)throw std::runtime_error("UE4SS package names differ only by case");
        children.push_back(package);
    }
    if(!direct.Main.empty()||!direct.Native.empty()){
        if(!children.empty())throw std::runtime_error("Do not mix direct legacy UE4SS entry points with packaged child mods");
        return {direct};
    }
    std::sort(children.begin(),children.end(),[](const auto& a,const auto& b){return ModFolderLayout::FoldAscii(a.Name)<ModFolderLayout::FoldAscii(b.Name);});
    return children;
}
inline std::string Manifest(const fs::path& mods,const std::vector<Package>& packages){
    std::ostringstream out;out<<"# RuneSchema UE4SS packages v1; generated from enabled parent mods.\n";
    for(const auto& package:packages){
        const auto relative=package.Root.lexically_relative(mods).generic_string();
        if(relative.empty()||relative.starts_with("../")||relative.find_first_of("\t\r\n")!=std::string::npos)throw std::runtime_error("UE4SS manifest path escaped its mod root");
        out<<package.Owner<<'\t'<<package.Name<<'\t'<<relative<<'\t';
        if(!package.Main.empty())out<<package.Main.lexically_relative(package.Root).generic_string();
        out<<'\t'<<(!package.Native.empty()?"native":"lua")<<'\n';
    }return out.str();
}
}
