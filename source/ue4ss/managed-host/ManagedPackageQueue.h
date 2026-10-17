#pragma once
// Included by the generated UE4SSProgram.cpp overlay only. The upstream source
// checkout and its public class layout remain untouched.
#include "../../raw/include/Core/ManagedModStartup.h"
#include <filesystem>
#include <cwchar>
#include <optional>

namespace RC::RuneSchemaManagedPackages {
inline PS::ManagedModStartup::Queue Startup;
inline std::optional<bool> ExplicitState(std::wstring_view name){
    for(const auto& [candidate,enabled]:UE4SSProgram::get_program().get_mods_txt_entries())
        if(PS::ManagedModStartup::Fold(ensure_str(candidate))==PS::ManagedModStartup::Fold(std::wstring(name)))return enabled;
    return {};
}
inline int QueuePackage(const wchar_t* path,const wchar_t* name,const wchar_t* owner) noexcept {
    try {
        if(!path||!name||!owner||wcsnlen_s(path,32768)>=32768||wcsnlen_s(name,256)>=256||wcsnlen_s(owner,256)>=256||!PS::ManagedModStartup::SafeName(name)||!PS::ManagedModStartup::SafeName(owner))return 1;
        const auto root=std::filesystem::canonical(path);
        const auto modsRoot=std::filesystem::canonical(std::filesystem::path(UE4SSProgram::get_program().get_working_directory())/L"Mods"/L"RuneSchema"/L"mods");
        const auto mod=std::filesystem::canonical(modsRoot/owner);
        if(PS::ManagedModStartup::Fold(mod.lexically_relative(modsRoot).wstring())!=PS::ManagedModStartup::Fold(owner))return 1;
        const auto relative=root.lexically_relative(mod);
        if(!PS::ManagedModStartup::PackageRelative(relative,name,owner))return 1;
        for(const auto& mods:UE4SSProgram::get_program().get_mods_directories())
            if(std::filesystem::exists(mods/name))return 2; // Never hijack an existing native UE4SS mod, enabled or not.
        const bool lua=std::filesystem::is_regular_file(root/L"Scripts"/L"main.lua");
        const bool native=std::filesystem::is_regular_file(root/L"dlls"/L"main.dll")||std::filesystem::is_regular_file(root/L"dlls"/(std::wstring(name)+L".dll"));
        const auto disabled=ExplicitState(name);if(disabled&&!*disabled)return 4;
        return static_cast<int>(Startup.Add({owner,name,root,lua,native}));
    }catch(...){return 1;}
}
}
extern "C" __declspec(dllexport) int __cdecl RuneSchema_UE4SS_QueuePackageV1(const wchar_t* path,const wchar_t* name,const wchar_t* owner) noexcept {
    return RC::RuneSchemaManagedPackages::QueuePackage(path,name,owner);
}
