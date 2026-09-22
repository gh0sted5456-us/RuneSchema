#pragma once
#include "Core/ConfigFiles.h"
#include "Generator/HelpyHotkeys.h"
#include <filesystem>
#include <mutex>
#include <nlohmann/json.hpp>
namespace PS::HelpySettings {
inline std::mutex Mutex;
inline bool Loaded=false,Writable=true;
inline bool Enabled=true;
inline std::string Status;
inline std::filesystem::path File;
inline nlohmann::json Document=nlohmann::json::object();
inline void Configure(const std::filesystem::path& pluginRoot) {
    std::lock_guard lock(Mutex);
    File=pluginRoot/"settings"/"settings.jsonc";Loaded=false;Writable=true;Enabled=true;Status.clear();Document=nlohmann::json::object();
}
inline void EnsureLoaded() {
    std::lock_guard lock(Mutex);if(Loaded)return;Loaded=true;
    try {
        if(File.empty())throw std::runtime_error("Helpy plugin settings path was not configured");
        if(std::filesystem::is_regular_file(File))Document=nlohmann::json::parse(ConfigFiles::Read(File),nullptr,true,true);
        else Document={{"enabled",true},{"toggleKey","F2"}};
        Enabled=Document.value("enabled",true);
        const auto name=Document.value("toggleKey",std::string{"F2"});
        const auto key=HelpyHotkeys::Find(name);
        if(!key)throw std::runtime_error("Invalid toggleKey; use F1..F24, Insert, Home, End, Pause or ScrollLock");
        Document["enabled"]=Enabled;Document["toggleKey"]=name;
        if(!std::filesystem::is_regular_file(File))ConfigFiles::Write(File,"// RuneSchema.Helpy settings. Edit with the game closed.\n"+Document.dump(2)+"\n");
        HelpyHotkeys::Active=key->code;Status="Helpy plugin settings loaded from "+File.string();
    }catch(const std::exception& e){Writable=false;Status=e.what();}
}
inline bool IsEnabled(){EnsureLoaded();std::lock_guard lock(Mutex);return Enabled;}
inline bool SetKey(const std::string& name) {
    EnsureLoaded();std::lock_guard lock(Mutex);
    try {
        const auto key=HelpyHotkeys::Find(name);if(!key)throw std::runtime_error("Unsupported Helpy hotkey");
        if(!Writable)throw std::runtime_error("Helpy settings are protected after a read/write error; repair the plugin settings.jsonc with the game closed");
        Document["enabled"]=Enabled;Document["toggleKey"]=name;
        ConfigFiles::Write(File,"// RuneSchema.Helpy settings. Edit with the game closed.\n"+Document.dump(2)+"\n");
        HelpyHotkeys::Active=key->code;HelpyHotkeys::SuppressedUntilRelease=key->code;
        Status="Helpy hotkey saved: "+name;return true;
    }catch(const std::exception& e){Status=e.what();return false;}
}
inline std::string Message(){std::lock_guard lock(Mutex);return Status;}
}
