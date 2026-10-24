#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <Windows.h>
#include <algorithm>

namespace PS::RuneSchemaData {
namespace fs=std::filesystem;
inline void Safe(const fs::path& path){
    for(auto current=fs::absolute(path);!current.empty();){
        const auto attributes=GetFileAttributesW(current.c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("RuneSchema data refuses redirected paths");
        const auto parent=current.parent_path();if(parent==current)break;current=parent;
    }
}
inline fs::path LocalDirectory(){
    const DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",nullptr,0);
    if(!length||length>32768)throw std::runtime_error("LOCALAPPDATA is unavailable");
    std::vector<wchar_t> value(length);
    if(GetEnvironmentVariableW(L"LOCALAPPDATA",value.data(),length)+1!=length)throw std::runtime_error("LOCALAPPDATA changed while being read");
    return fs::path(value.data());
}
inline fs::path GameSavedDirectory(){return LocalDirectory()/L"RSDragonwilds"/L"Saved";}
inline fs::path Resolve(const fs::path& local){
    const auto destination=local/L"RSDragonwilds"/L"RuneSchema";
    Safe(destination);return destination;
}
inline const fs::path& Directory(){static const fs::path directory=Resolve(LocalDirectory());return directory;}
inline fs::path SharedDirectory(const fs::path& root,const std::string& name){
    if(name!="HELPY"&&name!="runeschema.profile"&&name!="health")throw std::runtime_error("Unknown RuneSchema shared folder");
    const auto destination=root/"core"/name,existing=root/name;
    // Reuse established AppData profiles/settings in place, without moving them.
    const auto path=!fs::exists(destination)&&fs::is_directory(existing)?existing:destination;
    Safe(path);return path;
}
inline fs::path SharedDirectory(const std::string& name){return SharedDirectory(Directory(),name);}
inline std::string RuntimeKey(std::string platform){
    std::transform(platform.begin(),platform.end(),platform.begin(),[](unsigned char c){return c>='A'&&c<='Z'?char(c-'A'+'a'):char(c);});
    if(platform=="steam"||platform=="epic"||platform=="gamepass"||platform=="server")return platform;
    if(platform=="unknown")return "core/runtime/unknown";
    throw std::runtime_error("Unknown RuneSchema runtime folder");
}
inline fs::path PlatformDirectory(const std::string& platform){return Directory()/RuntimeKey(platform);}
inline fs::path MappingDirectory(const fs::path& root,const std::string& platform){
    const auto path=root/RuntimeKey(platform)/"cache"/"mappings";Safe(path);return path;
}
inline fs::path MappingDirectory(const std::string& platform){return MappingDirectory(Directory(),platform);}
}
