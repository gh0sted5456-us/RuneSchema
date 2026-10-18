#pragma once
#include "Core/UE4SSPackages.h"
#include "Runtime/BPModLoaderIntegration.h"
#include <windows.h>
#include <cstdlib>

namespace PS::UE4SSPackageIntegration {
inline std::vector<std::string> Prepare(const std::filesystem::path& ue4ss){
    namespace fs=std::filesystem;std::vector<std::string> diagnostics;std::vector<UE4SSPackages::Package> packages;
    const auto rune=ue4ss/"Mods"/"RuneSchema",mods=rune/"mods",manifest=rune/"settings"/"ue4ss.generated.txt";
    const auto session=std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64());
    if(!SetEnvironmentVariableA("RUNESCHEMA_UE4SS_PACKAGE_SESSION",session.c_str())
        ||_putenv_s("RUNESCHEMA_UE4SS_PACKAGE_SESSION",session.c_str())!=0)throw std::runtime_error("Cannot publish UE4SS package session to the host CRT");
    const auto header="# Session\t"+session+"\n";
    if(!BPModLoaderIntegration::AtomicWrite(manifest,header))throw std::runtime_error("Cannot clear the UE4SS package manifest; Lua companions will not run");
    std::vector<RC::StringType> discovered;
    if(fs::is_directory(mods))for(const auto& entry:fs::directory_iterator(mods))if(ModFolderLayout::LooksLikeRuneSchemaMod(entry.path()))discovered.push_back(entry.path().filename().native());
    std::set<std::string> identities;
    for(const auto& owner:DragonWilds::ModLoadOrder::Resolve(mods,discovered))try {
        for(auto package:UE4SSPackages::Discover(mods/owner,RC::to_string(owner))) {
            const auto id=ModFolderLayout::FoldAscii(package.Name);
            if(!identities.insert(id).second){diagnostics.push_back("[REJECTED] Duplicate UE4SS package identity: "+package.Name);continue;}
            if(fs::exists(ue4ss/"Mods"/RC::to_generic_string(package.Name))){diagnostics.push_back("[REJECTED] UE4SS sibling already claims '"+package.Name+"'; remove the duplicate installation deliberately");continue;}
            if(!package.Native.empty()){
                diagnostics.push_back("[NATIVE-UNSUPPORTED] '"+package.Name+"' from '"+package.Owner+"': RuneSchema supports Lua-only UE4SS add-ons; the entire native/hybrid package was skipped. Install native mods through stock UE4SS separately.");
                continue;
            }
            packages.push_back(std::move(package));
        }
    }catch(const std::exception& error){diagnostics.push_back("[REJECTED] '"+RC::to_string(owner)+"': "+error.what());}
    if(!BPModLoaderIntegration::AtomicWrite(manifest,header+UE4SSPackages::Manifest(mods,packages)))throw std::runtime_error("UE4SS package manifest could not be committed; Lua startup remains blocked");
    return diagnostics;
}
}
