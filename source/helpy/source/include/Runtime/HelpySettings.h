#pragma once
#include "Core/ConfigFiles.h"
#include "Core/HelpyShortcut.h"
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string_view>
#include <nlohmann/json.hpp>
namespace PS::HelpySettings {
inline std::mutex Mutex;
inline bool Loaded=false,Writable=true;
inline bool Enabled=true;
inline bool AutoOpen=false;
inline std::atomic<int> Hotkey=HelpyShortcut::DefaultKey;
inline void ApplyHotkey(std::string_view name){const auto* binding=HelpyShortcut::Find(name);Hotkey=binding?binding->Key:HelpyShortcut::DefaultKey;}
inline std::string HotkeyName(){return std::string(HelpyShortcut::Name(Hotkey.load()));}
inline std::string HotkeyHint(){return HotkeyName()+" for Helpy";}
inline std::string ExpectedContents;
inline std::string Status;
inline std::filesystem::path File;
inline nlohmann::json Document=nlohmann::json::object();
inline void Configure(const std::filesystem::path& pluginRoot) {
    std::lock_guard lock(Mutex);
    File=pluginRoot/"settings"/"settings.jsonc";Loaded=false;Writable=true;Enabled=true;AutoOpen=false;Hotkey=HelpyShortcut::DefaultKey;ExpectedContents.clear();Status.clear();Document=nlohmann::json::object();
}
inline void EnsureLoaded() {
    std::lock_guard lock(Mutex);if(Loaded)return;Loaded=true;
    try {
        if(File.empty())throw std::runtime_error("RSMK plugin settings path was not configured");
        if(std::filesystem::is_regular_file(File)){ExpectedContents=ConfigFiles::Read(File);Document=nlohmann::json::parse(ExpectedContents,nullptr,true,true);}
        else Document={{"enabled",true},{"openOnGameStart",false}};
        if(!Document.is_object())throw std::runtime_error("Helpy settings must be an object; existing file retained.");
        Enabled=Document.value("enabled",true);
        AutoOpen=Document.value("openOnGameStart",false);
        // Favorites are part of Helpy; the legacy opt-in field is ignored.
        // Old Canvas hotkey fields are ignored, not rewritten on every launch.
        // Desktop shortcut input belongs to the window, not to the game's viewport.
        if(!std::filesystem::exists(File)){ExpectedContents="// Helpy desktop window. The shortcut is set in RuneSchema/settings/settings.ini. Reload the theme in Quick settings.\n"+Document.dump(2)+"\n";ConfigFiles::Write(File,ExpectedContents);}
        Status="RSMK plugin settings loaded from "+File.string();
    }catch(const std::exception& e){Writable=false;Status=e.what();}
}
inline bool IsEnabled(){EnsureLoaded();std::lock_guard lock(Mutex);return Enabled;}
inline bool OpenOnGameStart(){EnsureLoaded();std::lock_guard lock(Mutex);return Writable&&Enabled&&AutoOpen;}
inline bool ExtendFavoritesToGame(){EnsureLoaded();std::lock_guard lock(Mutex);return Writable&&Enabled;}
inline bool SetBool(const char* key,bool enabled){
    EnsureLoaded();std::lock_guard lock(Mutex);
    try{
        if(!Writable||File.empty())throw std::runtime_error("Helpy settings are not writable; previous preference retained.");
        if(ConfigFiles::Read(File)!=ExpectedContents)throw std::runtime_error("Helpy settings changed on disk. Restart Helpy before saving this preference; no file was overwritten.");
        if(std::string_view(key)!="openOnGameStart")throw std::runtime_error("Unsupported Helpy preference");
        auto next=Document;next[key]=enabled;const auto text="// Helpy desktop window. The shortcut is set in RuneSchema/settings/settings.ini. Reload the theme in Quick settings.\n"+next.dump(2)+"\n";
        ConfigFiles::Write(File,text);Document=std::move(next);ExpectedContents=text;AutoOpen=enabled;
        Status=(enabled?"Saved. Helpy will open automatically next game launch.":"Saved. Automatic opening is disabled for the next game launch.");return true;
    }catch(const std::exception& e){Status=e.what();return false;}
}
inline bool SetOpenOnGameStart(bool enabled){return SetBool("openOnGameStart",enabled);}
inline std::string Message(){std::lock_guard lock(Mutex);return Status;}
}
