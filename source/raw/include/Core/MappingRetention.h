#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>
#include "Core/ConfigFiles.h"
namespace PS::MappingRetention {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline bool Hex(const std::string& value){return value.size()==16&&value.find_first_not_of("0123456789abcdef")==value.npos;}
inline bool Identity(const Json& value){return value.is_object()&&value.contains("Build")&&value["Build"].is_string()&&Hex(value["Build"].get<std::string>())&&value.contains("Fingerprint")&&value["Fingerprint"].is_string()&&Hex(value["Fingerprint"].get<std::string>());}
inline fs::path File(const fs::path& root,const Json& value){if(!Identity(value))throw std::runtime_error("Invalid generated mapping identity");return root/value.at("Build").get<std::string>()/(value.at("Fingerprint").get<std::string>()+".usmap");}
template<class Validate,class Safe>bool Migrate(const fs::path& legacy,const fs::path& root,Validate validate,Safe safe){
    safe(legacy);safe(root);
    if(fs::exists(root/"current.json")||!fs::is_regular_file(legacy/"current.json"))return false;
    safe(legacy/"current.json");const auto receipt=Json::parse(ConfigFiles::Read(legacy/"current.json",8192));
    if(receipt.value("Version",0)!=1||!Identity(receipt))throw std::runtime_error("Invalid legacy mapping receipt; original retained");
    const auto source=File(legacy,receipt),destination=File(root,receipt);const auto fingerprint=receipt.at("Fingerprint").get<std::string>();
    safe(source);validate(source,fingerprint);safe(destination);fs::create_directories(destination.parent_path());
    if(!fs::exists(destination)){
        auto temporary=destination;temporary+=".migrating";safe(temporary);
        if(fs::exists(temporary))throw std::runtime_error("Unfinished mapping migration retained for review");
        try{fs::copy_file(source,temporary);validate(temporary,fingerprint);fs::rename(temporary,destination);}
        catch(...){std::error_code ignored;fs::remove(temporary,ignored);throw;}
    }
    validate(destination,fingerprint);safe(root/"current.json");ConfigFiles::Write(root/"current.json",receipt.dump(2));return true;
}
inline bool BackupName(const std::string& name){
    constexpr std::string_view prefix="current.json.invalid-";
    return name.starts_with(prefix)&&name.ends_with(".bak")&&name.size()>prefix.size()+4&&name.size()<128
        &&name.substr(prefix.size(),name.size()-prefix.size()-4).find_first_not_of("0123456789-")==std::string::npos;
}
template<class Validate,class Safe>size_t Cleanup(const fs::path& root,const Json& receipt,const fs::path& active,Validate validate,Safe safe){
    if(receipt.value("Version",0)!=1||!Identity(receipt)||!fs::is_directory(root))return 0;
    const auto fingerprint=receipt.at("Fingerprint").get<std::string>();
    safe(active);validate(active,fingerprint);safe(root);
    struct Candidate{fs::path Path;uintmax_t Bytes;fs::file_time_type Stamp;};
    std::vector<Candidate> files;std::vector<fs::path> directories;size_t visited=0;
    const auto add=[&](const fs::directory_entry& entry){safe(entry.path());if(entry.is_regular_file())files.push_back({entry.path(),entry.file_size(),entry.last_write_time()});};
    const auto stale=[](const fs::directory_entry& entry){return fs::file_time_type::clock::now()-entry.last_write_time()>std::chrono::hours(24);};
    const bool legacy=root.lexically_normal()!=active.parent_path().parent_path().lexically_normal();
    for(const auto& entry:fs::directory_iterator(root)){
        if(++visited>8192)throw std::runtime_error("Mapping cleanup scan exceeds its bound");
        const auto name=entry.path().filename().string();
        if(entry.is_regular_file()&&(BackupName(name)||(legacy&&name=="current.json"))){add(entry);continue;}
        if(!entry.is_directory()||(!Hex(name)&&name!="index"&&name!="requests"))continue;
        safe(entry.path());directories.push_back(entry.path());
        for(const auto& child:fs::directory_iterator(entry.path())){
            if(++visited>8192)throw std::runtime_error("Mapping cleanup scan exceeds its bound");
            const auto file=child.path().filename().string();bool remove=false;
            if(Hex(name))remove=(file.size()==22&&file.ends_with(".usmap")&&Hex(file.substr(0,16))&&child.path().lexically_normal()!=active.lexically_normal())
                ||(((file.size()==20&&file.ends_with(".tmp"))||(file.size()==32&&file.ends_with(".usmap.migrating")))&&Hex(file.substr(0,16))&&stale(child));
            else if(name=="index")remove=file.size()==27&&file.starts_with("usmap-")&&file.ends_with(".json")&&Hex(file.substr(6,16))&&(legacy||file.substr(6,16)!=fingerprint);
            else remove=((file.size()==24&&file.ends_with(".request"))||(file.size()==31&&file.ends_with(".request.result")))&&Hex(file.substr(0,16))&&stale(child);
            if(remove)add(child);
        }
    }
    // Plan the bounded, non-recursive cleanup before deleting anything; unknown files survive.
    size_t removed=0;
    for(const auto& file:files){safe(file.Path);if(fs::is_regular_file(file.Path)&&fs::file_size(file.Path)==file.Bytes&&fs::last_write_time(file.Path)==file.Stamp&&fs::remove(file.Path))++removed;}
    for(const auto& directory:directories){safe(directory);if(fs::is_empty(directory))fs::remove(directory);}
    return removed;
}
}
