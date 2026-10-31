#pragma once
#include "Core/ConfigFiles.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <filesystem>
#include <algorithm>

namespace PS::HelpyVulkanSetup {
inline constexpr wchar_t RegistryPath[]=L"Software\\Khronos\\Vulkan\\ImplicitLayers";
inline std::string Utf8(const std::filesystem::path& value){const auto text=value.u8string();return {reinterpret_cast<const char*>(text.data()),text.size()};}
inline nlohmann::json Bind(nlohmann::json entries,const std::filesystem::path& game,const std::filesystem::path& bridge,bool enabled){
    if(!entries.is_array()||entries.size()>64)throw std::runtime_error("Vulkan startup bindings are invalid");
    entries.erase(std::remove_if(entries.begin(),entries.end(),[&](const auto& item){if(!item.is_object()||!item.contains("Executable")||!item.at("Executable").is_string()||!item.contains("Bridge")||!item.at("Bridge").is_string())throw std::runtime_error("Vulkan startup binding is malformed");return _wcsicmp(std::filesystem::u8path(item.at("Executable").template get<std::string>()).c_str(),game.c_str())==0;}),entries.end());
    if(enabled){if(!game.is_absolute()||!bridge.is_absolute()||game.extension()!=L".exe"||bridge.filename()!=L"RuneSchema.Helpy.dll")throw std::runtime_error("Vulkan startup requires an absolute game and bridge path");entries.push_back({{"Executable",Utf8(game)},{"Bridge",Utf8(bridge)}});}return entries;
}
inline std::filesystem::path GameExecutable(const std::filesystem::path& modRoot){
    const auto binaries=modRoot.parent_path().parent_path().parent_path();std::filesystem::path executable;
    for(const auto& entry:std::filesystem::directory_iterator(binaries)){const auto name=entry.path().filename().wstring();if(entry.is_regular_file()&&name.starts_with(L"RSDragonwilds")&&name.ends_with(L"Shipping.exe")){if(!executable.empty())throw std::runtime_error("More than one Dragonwilds executable was found");executable=entry.path();}}
    if(executable.empty())throw std::runtime_error("Dragonwilds' shipping executable was not found beside UE4SS");return std::filesystem::absolute(executable).lexically_normal();
}
inline bool Enrolled(const std::filesystem::path& directory,const std::filesystem::path& game){
    const auto file=directory/"targets.json";std::error_code error;if(!std::filesystem::is_regular_file(file,error))return false;const auto list=nlohmann::json::parse(ConfigFiles::Read(file,65536));if(!list.is_array())return false;
    return std::any_of(list.begin(),list.end(),[&](const auto& item){return item.is_object()&&item.contains("Executable")&&item.at("Executable").is_string()&&_wcsicmp(std::filesystem::u8path(item.at("Executable").template get<std::string>()).c_str(),game.c_str())==0;});
}
inline void Configure(const std::filesystem::path& directory,const std::filesystem::path& game,const std::filesystem::path& bridge,const std::filesystem::path& selector,bool enable){
    const auto config=directory/"targets.json",manifest=directory/"RuneSchema.Helpy.Vulkan.json";
    const auto existing=std::filesystem::is_regular_file(config)?nlohmann::json::parse(ConfigFiles::Read(config,65536)):nlohmann::json::array();const auto entries=Bind(existing,game,bridge,enable);
    std::filesystem::create_directories(directory);
    if(enable){
        if(!std::filesystem::is_regular_file(bridge)||!std::filesystem::is_regular_file(selector))throw std::runtime_error("Install the matching Helpy bridge and Vulkan startup selector first");
        const auto source=ConfigFiles::Read(selector,2*1024*1024);const auto destination=directory/"RuneSchema.Helpy.Vulkan.dll";
        if(!std::filesystem::is_regular_file(destination)||ConfigFiles::Read(destination,2*1024*1024)!=source)ConfigFiles::Write(destination,source);
        const nlohmann::json declaration{{"file_format_version","1.2.0"},{"layer",{{"name","VK_LAYER_RUNESCHEMA_helpy"},{"type","GLOBAL"},{"library_path",Utf8(destination)},{"api_version","1.3.0"},{"implementation_version","1"},{"description","RuneSchema Helpy opt-in game overlay"},{"disable_environment",{{"DISABLE_RUNESCHEMA_HELPY_OVERLAY","1"}}}}}};ConfigFiles::Write(manifest,declaration.dump(2));
    }
    ConfigFiles::Write(config,entries.dump(2));
    HKEY key{};if(RegCreateKeyExW(HKEY_CURRENT_USER,RegistryPath,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)throw std::runtime_error("Cannot access the current user's Vulkan layer registration");
    const DWORD enabled=0;const auto result=entries.empty()?RegDeleteValueW(key,manifest.c_str()):RegSetValueExW(key,manifest.c_str(),0,REG_DWORD,reinterpret_cast<const BYTE*>(&enabled),sizeof(enabled));RegCloseKey(key);
    if(result!=ERROR_SUCCESS&&result!=ERROR_FILE_NOT_FOUND)throw std::runtime_error("Cannot update Helpy's Vulkan startup registration");
}
}
