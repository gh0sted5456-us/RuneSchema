#pragma once
#include <windows.h>
#include <filesystem>
namespace PS::HelpyModManager {
inline std::filesystem::path FindVortex(){
    for(const auto hive:{HKEY_CURRENT_USER,HKEY_LOCAL_MACHINE}){
        wchar_t value[32768]{};DWORD bytes=sizeof(value);
        if(RegGetValueW(hive,L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Vortex.exe",nullptr,RRF_RT_REG_SZ,nullptr,value,&bytes)==ERROR_SUCCESS){std::error_code error;if(std::filesystem::is_regular_file(value,error))return value;}
    }
    for(const auto variable:{L"LOCALAPPDATA",L"ProgramFiles",L"ProgramFiles(x86)"}){
        wchar_t base[32768]{};const auto length=GetEnvironmentVariableW(variable,base,32768);if(!length||length>=32768)continue;
        for(const auto suffix:{L"Programs\\Vortex\\Vortex.exe",L"Black Tree Gaming Ltd\\Vortex\\Vortex.exe",L"Vortex\\Vortex.exe"}){
            const auto file=std::filesystem::path(base)/suffix;std::error_code error;if(std::filesystem::is_regular_file(file,error))return file;
        }
    }return {};
}
}
