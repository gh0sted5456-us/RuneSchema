#pragma once
#include "Utility/ConfigIni.h"
#include "Core/ConfigFiles.h"
#include <mutex>
#include <cmath>

namespace PS::RuneSchemaSettingsEditor {
inline std::mutex Gate;
inline constexpr size_t MaximumBytes=256*1024;
inline void Fields(const nlohmann::json& object,const std::string& prefix,nlohmann::json& fields){
    for(const auto& [key,value]:object.items()){
        const auto path=prefix+"/"+key;
        if(path=="/persistence/recovery"||path=="/worldSaveCleanup/backupBeforePrune")continue;
        if(value.is_object())Fields(value,path,fields);else fields[path]=value;
    }
}
inline nlohmann::json ReadUnlocked(const std::filesystem::path& directory){
    const auto file=directory/"settings.ini";
    const auto contents=ConfigFiles::Read(file,MaximumBytes);
    const auto source=DecodeIniDocument(contents);
    const auto document=SettingsDocument(DecodeSettings(source.dump()),source);
    nlohmann::json editable=nlohmann::json::object();Fields(SettingsDocument(PSConfigSettings{}),"",editable);
    return {{"Ok",true},{"Document",document},{"Editable",editable},{"ExpectedContents",contents},
        {"Path",file.string()},{"RestartRequired",true}};
}
inline nlohmann::json Read(const std::filesystem::path& directory){
    std::lock_guard lock(Gate);
    try{return ReadUnlocked(directory);}catch(const std::exception& error){return {{"Ok",false},{"Status",error.what()}};}
}
inline nlohmann::json Save(const std::filesystem::path& directory,const nlohmann::json& request){
    std::lock_guard lock(Gate);
    try{
        const auto before=ReadUnlocked(directory);
        if(!request.contains("ExpectedContents")||!request["ExpectedContents"].is_string()
            ||request["ExpectedContents"].get_ref<const std::string&>()!=before["ExpectedContents"].get_ref<const std::string&>())
            throw std::runtime_error("Settings changed on disk. Reload before saving; no changes were written.");
        const auto& patch=request.at("Patch");
        if(!patch.is_object()||patch.empty()||patch.size()>128||patch.dump().size()>MaximumBytes)
            throw std::runtime_error("Settings changes must be a bounded nonempty object.");
        auto document=before.at("Document");const auto& fields=before.at("Editable");
        for(const auto& [path,value]:patch.items()){
            if(!fields.contains(path))throw std::runtime_error("Setting is not editable: "+path);
            const auto& type=fields.at(path);
            if(value.type()!=type.type()&&!(value.is_number()&&type.is_number()))throw std::runtime_error("Incorrect setting type: "+path);
            if(value.is_number()){
                const auto number=value.get<double>();
                if(!std::isfinite(number)||number<0||number>1000000000.0)throw std::runtime_error("Setting must be between 0 and 1000000000: "+path);
                if(type.is_number_integer()&&std::floor(number)!=number)throw std::runtime_error("Setting requires a whole number: "+path);
            }
            if(value.is_array()){
                if(value.size()>256)throw std::runtime_error("Setting list exceeds 256 entries: "+path);
                for(const auto& item:value)if(!item.is_string()||item.get_ref<const std::string&>().size()>512)throw std::runtime_error("Setting list requires bounded strings: "+path);
            }
            document[nlohmann::json::json_pointer(path)]=value;
        }
        const auto validated=DecodeSettings(document.dump());
        const auto normalized=SettingsDocument(validated,document);
        // Reject values the codec would clamp or normalize rather than silently
        // saving something different from the requested UI value.
        for(const auto& [path,value]:patch.items())if(normalized.at(nlohmann::json::json_pointer(path))!=value)
            throw std::runtime_error("Setting value is outside the supported range: "+path);
        const auto text=EncodeIniDocument(normalized);if(text.size()>MaximumBytes)throw std::runtime_error("Settings exceed the size limit.");
        const auto file=directory/"settings.ini";
        const auto backup=ConfigFiles::Backup(file);
        if(ConfigFiles::Read(file,MaximumBytes)!=before.at("ExpectedContents").get<std::string>())
            throw std::runtime_error("Settings changed while preparing the backup. Reload before saving.");
        ConfigFiles::Write(file,text);
        auto result=ReadUnlocked(directory);result["Backup"]=backup.string();
        bool cleanupOnly=true;
        for(const auto& [path,value]:patch.items())cleanupOnly&=path=="/persistence/blanketCleanup"||path=="/persistence/cleanupOnWorldEntry";
        const bool nextEntry=validated.persistence.cleanupOnWorldEntry;
        result["RestartRequired"]=!cleanupOnly||!nextEntry;
        result["Status"]=cleanupOnly
            ?(nextEntry?"Saved with a backup. Cleanup changes apply at the next character/world entry.":"Saved with a backup. Cleanup changes apply after restarting Dragonwilds; the current session policy is retained.")
            :"Saved settings.ini with a backup. Startup settings require a restart; cleanup uses its selected activation timing.";
        return result;
    }catch(const std::exception& error){return {{"Ok",false},{"Status",error.what()}};}
}
inline nlohmann::json PrepareModMenuFields(const std::filesystem::path& directory){
    const auto snapshot=Read(directory);if(!snapshot.value("Ok",false))return snapshot;
    try{
        const auto original=DecodeIniDocument(snapshot.at("ExpectedContents").get<std::string>());
        nlohmann::json patch=nlohmann::json::object();
        for(const auto* path:{"/persistence/blanketCleanup","/persistence/cleanupOnWorldEntry",
                "/persistence/allowCorruptCharacterEntry","/helpyHotkey","/autoUpdateMappings","/advancedLogging","/archiveLogs"}){
            const auto pointer=nlohmann::json::json_pointer(path);
            if(!original.contains(pointer))patch[path]=snapshot.at("Document").at(pointer);
        }
        if(patch.empty())return snapshot;
        return Save(directory,{{"ExpectedContents",snapshot.at("ExpectedContents")},{"Patch",patch}});
    }catch(const std::exception& error){return {{"Ok",false},{"Status",error.what()}};}
}
}
