#pragma once
#include "Core/ConfigFiles.h"
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
        if(File.empty())throw std::runtime_error("RSMK plugin settings path was not configured");
        if(std::filesystem::is_regular_file(File))Document=nlohmann::json::parse(ConfigFiles::Read(File),nullptr,true,true);
        else Document={{"enabled",true}};
        Enabled=Document.value("enabled",true);
        // Old Canvas hotkey fields are ignored, not rewritten on every launch.
        // Desktop F2 input belongs to the window, not to the game's viewport.
        if(!std::filesystem::exists(File))ConfigFiles::Write(File,"// RSMK desktop window. F2 opens/closes; F5 reloads the skin.\n"+Document.dump(2)+"\n");
        Status="RSMK plugin settings loaded from "+File.string();
    }catch(const std::exception& e){Writable=false;Status=e.what();}
}
inline bool IsEnabled(){EnsureLoaded();std::lock_guard lock(Mutex);return Enabled;}
inline std::string Message(){std::lock_guard lock(Mutex);return Status;}
}
