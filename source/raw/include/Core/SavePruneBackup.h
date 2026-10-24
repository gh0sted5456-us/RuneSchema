#pragma once
#include "Core/CacheBundleIO.h"
#include "Core/CharacterSaveEditor.h"
#include "Core/RuneSchemaDataRoot.h"
#include <cstring>

namespace PS::SavePruneBackup {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline constexpr size_t MaximumBytes=32*1024*1024;
inline Json Decode(const std::string& bytes){
    if(bytes.empty()||bytes.size()>MaximumBytes)throw std::runtime_error("Character backup exceeds its bounds");
    if(bytes.size()>=2&&static_cast<unsigned char>(bytes[0])==0xff&&static_cast<unsigned char>(bytes[1])==0xfe){
        if(bytes.size()%2)throw std::runtime_error("Character backup has invalid UTF-16 length");
        std::u16string text((bytes.size()-2)/2,u'\0');std::memcpy(text.data(),bytes.data()+2,bytes.size()-2);return Json::parse(text);
    }
    return Json::parse(bytes);
}
inline fs::path Directory(const std::string& runtime){
    const auto key=RuneSchemaData::RuntimeKey(runtime);
    if(key!="steam"&&key!="gamepass"&&key!="epic"&&key!="server")throw std::runtime_error("Save backup requires an identified runtime");
    return RuneSchemaData::Directory()/"core"/"backups"/key/"characters";
}
inline fs::path Write(const fs::path& directory,const std::string& original,const std::string& expectedGuid){
    const auto source=Decode(original);const auto guid=CharacterSaveEditor::Identity(source);
    if(guid.empty()||guid!=CharacterSaveEditor::Guid(expectedGuid)||!source.contains("GameProgress")||!source["GameProgress"].is_object())
        throw std::runtime_error("Backup is not the exact native gameplay character");
    const auto file=directory/guid/(CacheBundleIO::Hash(original)+".json.bak");CacheBundleIO::Safe(file);
    if(!fs::exists(file))CacheBundleIO::Publish(file,original);
    if(CacheBundleIO::Read(file,MaximumBytes)!=original)throw std::runtime_error("Pre-prune save backup did not verify; save unchanged");
    return file;
}
inline size_t Provider(const fs::path& directory,const std::vector<fs::path>& candidates,const std::string& expectedGuid){
    if(candidates.size()>8192)throw std::runtime_error("Character backup candidate bound exceeded");
    size_t read=0,matched=0;
    for(const auto& file:candidates){
        const auto size=fs::file_size(file);if(!size||size>MaximumBytes)continue;
        if(read+size>64*1024*1024)throw std::runtime_error("Character backup scan budget exceeded");read+=size;
        const auto original=CacheBundleIO::Read(file,MaximumBytes);Json source;
        try{source=Decode(original);}catch(...){continue;}
        if(CharacterSaveEditor::Identity(source)!=CharacterSaveEditor::Guid(expectedGuid)||!source.contains("GameProgress")||!source["GameProgress"].is_object())continue;
        Write(directory,original,expectedGuid);
        if(CacheBundleIO::Read(file,MaximumBytes)!=original)throw std::runtime_error("Character save changed during backup; pruning deferred");
        ++matched;
    }
    if(!matched)throw std::runtime_error("Exact character save backup is unavailable; native quest state unchanged");
    return matched;
}
}
