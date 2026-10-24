#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <utility>
#include <Windows.h>

namespace PS::RuntimeModPath {
// Resolve only the host-provided root. WindowsApps uses a managed junction into
// XboxGames; links inside a mod payload remain forbidden by ModManager::Safe.
inline std::filesystem::path Resolve(const std::filesystem::path& activeRoot){
    const auto handle=CreateFileW(activeRoot.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
    if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open the connected runtime's RuneSchema directory.");
    struct Close {HANDLE Value;~Close(){CloseHandle(Value);}} close{handle};
    BY_HANDLE_FILE_INFORMATION before{};
    if(!GetFileInformationByHandle(handle,&before)||!(before.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))throw std::runtime_error("Connected RuneSchema root is not an accessible directory.");
    const auto required=GetFinalPathNameByHandleW(handle,nullptr,0,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!required||required>32768)throw std::runtime_error("Cannot resolve the connected runtime's RuneSchema directory.");
    std::vector<wchar_t> text(required+1);const auto size=GetFinalPathNameByHandleW(handle,text.data(),static_cast<DWORD>(text.size()),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!size||size>=text.size())throw std::runtime_error("Runtime directory changed while resolving its path.");
    std::wstring value(text.data(),size);if(value.starts_with(L"\\\\?\\UNC\\"))value=L"\\\\"+value.substr(8);else if(value.starts_with(L"\\\\?\\"))value.erase(0,4);
    const auto result=std::filesystem::path(value).lexically_normal();
    const auto check=CreateFileW(result.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
    if(check==INVALID_HANDLE_VALUE)throw std::runtime_error("Canonical runtime directory is inaccessible.");Close checkClose{check};BY_HANDLE_FILE_INFORMATION after{};
    if(!GetFileInformationByHandle(check,&after)||before.dwVolumeSerialNumber!=after.dwVolumeSerialNumber||before.nFileIndexHigh!=after.nFileIndexHigh||before.nFileIndexLow!=after.nFileIndexLow)throw std::runtime_error("Runtime directory identity changed; nothing was written.");
    return result;
}
inline std::string Text(const std::filesystem::path& path){const auto value=path.u8string();return {value.begin(),value.end()};}
inline std::pair<std::filesystem::path,std::filesystem::path> MappingRoots(const std::filesystem::path& activeMod,const std::filesystem::path& activeHost,const std::filesystem::path& activeCache){
    const auto mod=Resolve(activeMod),host=Resolve(activeHost);
    if(mod.parent_path().parent_path()!=host)throw std::runtime_error("Automatic mapping runtime directory identity differs");
    const auto cache=activeCache.lexically_relative(activeMod);
    if(cache.empty()||cache.is_absolute()||std::find(cache.begin(),cache.end(),std::filesystem::path(".."))!=cache.end())throw std::runtime_error("Automatic mapping cache escaped the active runtime");
    return {mod/cache/"mappings"/"automatic",host};
}
inline void Expected(const std::filesystem::path& root,const std::string& expected){
    const auto provided=std::filesystem::u8path(expected).lexically_normal();
    if(expected.empty()||_wcsicmp(root.c_str(),provided.c_str())!=0)throw std::runtime_error("Connected runtime directory changed. Refresh Mods before applying changes or importing.");
}
}
