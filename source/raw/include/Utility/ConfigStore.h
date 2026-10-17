#pragma once
#include "Utility/ConfigIni.h"
#include "Core/ConfigFiles.h"
#include <atomic>
#include <chrono>

namespace PS::SettingsStore {
    namespace fs=std::filesystem;
    inline constexpr size_t MaximumBytes=1024*1024;
    struct Loaded {
        PSConfigSettings Settings;
        nlohmann::json Document;
        fs::path File;
        std::string Status;
    };
    inline Loaded ParseContents(const fs::path& file,bool ini,const std::string& text) {
        auto document=ini?DecodeIniDocument(text):nlohmann::json::parse(StripJsonComments(text));
        return {DecodeSettings(document.dump()),std::move(document),file,"Configuration loaded."};
    }
    inline Loaded Parse(const fs::path& file,bool ini) {
        return ParseContents(file,ini,ConfigFiles::Read(file,MaximumBytes));
    }
    inline fs::path Preserve(const fs::path& file,std::string_view reason) {
        static std::atomic<unsigned> sequence{0};
        auto backup=file;backup+='.'+std::string(reason)+'-'+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+'-'+std::to_string(sequence++)+".bak";
        if(!fs::copy_file(file,backup,fs::copy_options::none)||ConfigFiles::Read(backup,MaximumBytes)!=ConfigFiles::Read(file,MaximumBytes))
            throw std::runtime_error("Could not verify original settings backup");
        return backup;
    }
    inline Loaded Load(const fs::path& directory) {
        const auto ini=directory/"settings.ini",legacy=directory/"settings.jsonc",receipt=directory/"settings.ini.migration";
        if(!fs::exists(legacy)){
            if(fs::exists(ini))return Parse(ini,true);
            auto document=SettingsDocument(PSConfigSettings{});
            ConfigFiles::Write(ini,EncodeIniDocument(document));
            return {PSConfigSettings{},std::move(document),ini,"Configuration created with default settings."};
        }
        std::string source;
        Loaded old;
        try{
            source=ConfigFiles::Read(legacy,MaximumBytes);
            old=ParseContents(legacy,false,source);
        }
        catch(const std::exception&){
            if(!fs::exists(ini))throw;
            auto current=Parse(ini,true);current.Status="INI loaded; invalid legacy JSONC left untouched.";return current;
        }
        // A receipt proves that a prior launch completed translation. Merely
        // shipping defaults beside JSONC must never qualify it for deletion.
        if(fs::exists(receipt)&&fs::exists(ini)){
            nlohmann::json previous;
            try{
                previous=nlohmann::json::parse(ConfigFiles::Read(receipt,8*MaximumBytes));
                if(previous.value("Version",0)!=1||!previous.at("Source").is_string()||!previous.at("Ini").is_string())throw std::runtime_error("Invalid migration receipt");
            }catch(const std::exception&){old.Status="JSONC loaded; invalid migration receipt and both settings files left untouched.";return old;}
            Loaded current;
            std::string iniContents;
            try{iniContents=ConfigFiles::Read(ini,MaximumBytes);current=ParseContents(ini,true,iniContents);}
            catch(const std::exception&){old.Status="JSONC loaded; invalid INI left untouched. Migration cleanup deferred.";return old;}
            if(source==previous.at("Source").get<std::string>()){
                try{
                    const auto backup=Preserve(legacy,"migrated");
                    if(ConfigFiles::Read(legacy,MaximumBytes)!=source)throw std::runtime_error("Legacy settings changed during cleanup");
                    if(ConfigFiles::Read(ini,MaximumBytes)!=iniContents)throw std::runtime_error("INI changed during cleanup");
                    if(!fs::remove(legacy))throw std::runtime_error("Could not retire legacy settings");
                    // Receipt removal is optional after the durable backup and
                    // validated INI. Never turn cleanup failure into defaults.
                    std::error_code ignored;fs::remove(receipt,ignored);
                    current.Status="INI loaded; legacy JSONC archived at: "+backup.string();
                }catch(const std::exception& error){current.Status=std::string("INI loaded; legacy cleanup deferred: ")+error.what();}
                return current;
            }
            if(ConfigFiles::Read(ini,MaximumBytes)!=previous.at("Ini").get<std::string>()){
                current.Status="INI loaded; both settings files changed since translation. JSONC kept for manual review.";return current;
            }
            // Only JSONC changed: translate the latest edits, then wait for
            // another launch before retiring that version of the source.
        }
        try{
            auto document=SettingsDocument(old.Settings,old.Document);
            const auto translated=EncodeIniDocument(document);
            const auto record=nlohmann::json{{"Version",1},{"Source",source},{"Ini",translated}}.dump();
            if(translated.size()>MaximumBytes||record.size()>8*MaximumBytes)throw std::runtime_error("Translated configuration exceeds size limit");
            if(ConfigFiles::Read(legacy,MaximumBytes)!=source)throw std::runtime_error("Legacy settings changed during translation");
            if(fs::exists(ini)&&ConfigFiles::Read(ini,MaximumBytes)!=translated)(void)Preserve(ini,"before-jsonc-migration");
            ConfigFiles::Write(ini,translated);
            auto check=Parse(ini,true);
            if(check.Document!=document)throw std::runtime_error("Translated INI verification failed");
            ConfigFiles::Write(receipt,record);
            old.File=ini;old.Document=std::move(document);
            old.Status="JSONC loaded and translated to INI. Legacy cleanup will be checked next launch.";
        }catch(const std::exception& error){old.Status=std::string("JSONC loaded; INI migration deferred: ")+error.what();}
        return old;
    }
}
