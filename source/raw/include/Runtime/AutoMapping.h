#pragma once
#include "Runtime/MappingBackbone.h"
#include "Runtime/ExecutableVersion.h"
#include "Runtime/HostServices.h"
#include "Core/ConfigFiles.h"
#include "Core/UsmapIndex.h"
#include "Core/MappingRetention.h"
#include "Core/RuntimeModPath.h"
#include "Core/MappingCompatibility.h"
#include "Utility/Logging.h"
#include "Utility/Config.h"
#include "Unreal/Hooks.hpp"
#include <windows.h>
#include <map>
#include <cmath>
namespace PS::AutoMapping {
namespace fs=std::filesystem;using Json=nlohmann::json;
inline fs::path Root,Host,Request;inline std::string Build,Nonce;inline bool Pending=false;inline double Elapsed=0;inline ULONGLONG Started=0;
inline Json Previous=Json::object();
inline std::map<fs::path,std::pair<uintmax_t,fs::file_time_type>> Before;
inline std::string Key(std::string_view text){uint64_t hash=14695981039346656037ull;for(unsigned char c:text){hash^=c;hash*=1099511628211ull;}std::ostringstream out;out<<std::hex<<std::setfill('0')<<std::setw(16)<<hash;return out.str();}
inline bool Hex(const std::string& value){return value.size()==16&&value.find_first_not_of("0123456789abcdef")==value.npos;}
inline void Safe(const fs::path& path){for(auto current=fs::absolute(path);!current.empty();){
    const auto attr=GetFileAttributesW(current.c_str());if(attr!=INVALID_FILE_ATTRIBUTES&&(attr&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Automatic mapping refuses redirected paths");
    auto parent=current.parent_path();if(parent==current)break;current=parent;
}}
inline MappingBackbone::Mapping Validate(const fs::path& file){
    Safe(file);const auto bytes=fs::file_size(file);if(bytes<16||bytes>32*1024*1024)throw std::runtime_error("Automatic mapping exceeds size bounds");
    const auto stamp=fs::last_write_time(file);const auto fingerprint=MappingBackbone::Fingerprint(file);
    for(const char* type:{"/Script/Dominion.ItemData","/Script/Dominion.RecipeData","/Script/Dominion.JournalComponent"}){
        const auto result=Usmap::Index::Shared().Handle(file,fingerprint,Root/"index",{{"Action","DescribeType"},{"Type",type}});
        if(!result.value("Found",false))throw std::runtime_error("Generated mapping is missing required native types");
    }
    if(fs::last_write_time(file)!=stamp||fs::file_size(file)!=bytes)throw std::runtime_error("Mapping changed during validation");
    return {file,bytes,fingerprint,true};
}
inline void Prune(const Json& receipt)noexcept{try{
    // Only exact hash-addressed generated files from a previous committed
    // receipt are eligible. No recursive delete, host dump, shipped fallback,
    // index directory, custom mapping or neighbouring file is ever targeted.
    const auto removed=MappingRetention::RemoveRetired(Root,receipt,[](const fs::path& file,const std::string& fingerprint){if(Validate(file).Fingerprint!=fingerprint)throw std::runtime_error("Current generated mapping did not validate; old files retained");},[](const fs::path& file){Safe(file);});
    if(removed)ConsolidatedLog<RC::LogLevel::Normal>(STR("[RuneSchema] [MAPPINGS][CLEANUP] Removed {} superseded generated .usmap file(s) after validating the active replacement. Shipped/custom mappings retained.\n"),removed);
}catch(const std::exception& error){Log<RC::LogLevel::Warning>(STR("[MAPPINGS][CLEANUP] New mapping is retained; old-file cleanup deferred: {}\n"),ToWideSafe(error.what()));}}
inline void Prepare()noexcept{try{
    if(!PSConfig::Get()->GetSettings().autoUpdateMappings||GetEnvironmentVariableW(L"RUNESCHEMA_DISABLE_AUTO_MAPPING",nullptr,0))return;
    wchar_t executable[32768]{};const auto length=GetModuleFileNameW(nullptr,executable,_countof(executable));if(!length||length>=_countof(executable))return;
    const fs::path binary(executable);
    // Resolve the managed package alias once; redirects inside the cache remain forbidden.
    const auto [root,host]=RuntimeModPath::MappingRoots(HostServices::ModDirectory(),HostServices::WorkingDirectory(),HostServices::CacheDirectory());
    Root=root;Host=host;Safe(Root);Safe(Host);
    Build=Key(MappingCompatibility::Identity(binary,Host,Host/"Mods/RuneSchema",ExecutableVersion::Current()));
    const auto receipt=Root/"current.json";
    if(fs::is_regular_file(receipt))try{Safe(receipt);const auto saved=Json::parse(ConfigFiles::Read(receipt,8192));const auto fingerprint=saved.value("Fingerprint",std::string{});if(saved.value("Version",0)==1&&MappingRetention::Identity(saved))Previous=saved;
        if(saved.value("Version",0)==1&&saved.value("Build",std::string{})==Build&&Hex(fingerprint)){
            auto mapping=Validate(Root/Build/(fingerprint+".usmap"));if(mapping.Fingerprint!=fingerprint)throw std::runtime_error("Cached generated mapping fingerprint changed");
            MappingBackbone::Activate(std::move(mapping));Prune(saved);return;
        }
    }catch(const std::exception&){/* Retain shipped fallback and regenerate; never trust a broken receipt. */}
    fs::create_directories(Root/"requests");Nonce=Key(Build+"|"+std::to_string(GetCurrentProcessId())+"|"+std::to_string(GetTickCount64()));Request=Root/"requests"/(Nonce+".request");Safe(Request);
    size_t count=0;for(const auto& entry:fs::directory_iterator(Host))if(entry.is_regular_file()&&entry.path().extension()==L".usmap"){
        if(++count>64)throw std::runtime_error("Automatic mapping candidate limit exceeded");Safe(entry.path());Before[entry.path()]={entry.file_size(),entry.last_write_time()};
    }
    ConfigFiles::Write(Request,Nonce+"\n");
    const auto utf8=Request.u8string();const std::string requestPath(utf8.begin(),utf8.end());
    if(!SetEnvironmentVariableW(L"RUNESCHEMA_AUTO_MAPPING_REQUEST",Request.c_str())||_putenv_s("RUNESCHEMA_AUTO_MAPPING_REQUEST",requestPath.c_str())!=0)
        throw std::runtime_error("Automatic mapping Lua/CRT handoff failed");
    Pending=true;Started=GetTickCount64();
    ConsolidatedLog<RC::LogLevel::Normal>(STR("[RuneSchema] [MAPPINGS][UPDATE] Game build needs a fresh reflection mapping; native UE4SS dump queued. Existing mapping and native safety checks remain active.\n"));
}catch(const std::exception& error){Pending=false;Log<RC::LogLevel::Warning>(STR("[MAPPINGS][DEFERRED] {}. Last working mapping retained.\n"),ToWideSafe(error.what()));}}
inline void Start(){if(!Pending)return;
    RC::Unreal::Hook::FCallbackOptions options{};options.OwnerModName=STR("RuneSchema");options.HookName=STR("RuneSchemaAutoMapping");
    RC::Unreal::Hook::RegisterEngineTickPostCallback([](auto& iteration,RC::Unreal::UEngine*,float delta,bool){
        if(!Pending){iteration.RemoveSelf();return;}Elapsed=double(GetTickCount64()-Started)/1000.0;
        static double next=0;if(Elapsed<next)return;next=Elapsed+1;
        try{
            const fs::path resultFile=Request.wstring()+L".result";Safe(resultFile);
            if(!fs::is_regular_file(resultFile)){if(Elapsed>180)throw std::runtime_error("Native Lua mapping dump did not complete within three minutes");return;}
            const auto result=ConfigFiles::Read(resultFile,1024);if(result!=Nonce+"\nok\n")throw std::runtime_error("Native DumpUSMAP capability unavailable or dump failed");
            fs::path source;size_t changed=0,count=0;
            for(const auto& entry:fs::directory_iterator(Host))if(entry.is_regular_file()&&entry.path().extension()==L".usmap"){
                if(++count>64)throw std::runtime_error("Mapping candidate limit exceeded");Safe(entry.path());const auto old=Before.find(entry.path());
                if(old==Before.end()||old->second!=std::pair{entry.file_size(),entry.last_write_time()}){source=entry.path();++changed;}
            }
            if(changed!=1)throw std::runtime_error("Native mapping output is missing or ambiguous");
            const auto mapping=Validate(source);const auto folder=Root/Build;Safe(folder);fs::create_directories(folder);
            const auto destination=folder/(mapping.Fingerprint+".usmap");Safe(destination);
            bool destinationValid=false;if(fs::is_regular_file(destination))try{destinationValid=Validate(destination).Fingerprint==mapping.Fingerprint;}catch(const std::exception&){}
            if(!destinationValid){const auto temporary=folder/(Nonce+".tmp");Safe(temporary);fs::copy_file(source,temporary);
                const auto copied=Validate(temporary);if(copied.Fingerprint!=mapping.Fingerprint)throw std::runtime_error("Generated mapping copy differs");
                if(destination.parent_path().lexically_normal()!=(Root/Build).lexically_normal()||destination.extension()!=L".usmap")throw std::runtime_error("Generated mapping replacement escaped its owned cache");
                if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Generated mapping could not be published");
            }
            auto published=Validate(destination);if(published.Fingerprint!=mapping.Fingerprint)throw std::runtime_error("Published mapping differs");
            const auto receipt=Root/"current.json";Safe(receipt);if(fs::exists(receipt))ConfigFiles::Backup(receipt);
            Json nextReceipt={{"Version",1},{"Build",Build},{"Fingerprint",published.Fingerprint},{"Coverage","Native and currently loaded Blueprint reflection; not an unloaded-asset sweep"}};nextReceipt["Retired"]=MappingRetention::Retired(Previous,nextReceipt);
            ConfigFiles::Write(receipt,nextReceipt.dump(2));
            MappingBackbone::Activate(std::move(published));Pending=false;Prune(nextReceipt);
            ConsolidatedLog<RC::LogLevel::Normal>(STR("[RuneSchema] [MAPPINGS][UPDATED] Fresh UE4SS reflection mapping validated and activated; shipped fallback and native hook guards retained.\n"));
        }catch(const std::exception& error){Pending=false;Log<RC::LogLevel::Warning>(STR("[MAPPINGS][DEFERRED] {}. Last working mapping retained.\n"),ToWideSafe(error.what()));}
        if(!Pending)iteration.RemoveSelf();
    },options);
}
}
