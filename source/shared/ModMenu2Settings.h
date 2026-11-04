#pragma once
#include "Core/ConfigFiles.h"
#include "Core/CacheBundleIO.h"
#include <nlohmann/json.hpp>

namespace PS::ModMenu2Settings {
struct Settings {bool Enabled=true;int Hotkey=0;};
inline std::filesystem::path File(const std::filesystem::path& root){return root/"settings/modmenu2.json";}
inline Settings Read(const std::filesystem::path& root){const auto file=File(root);CacheBundleIO::Safe(file);if(!std::filesystem::exists(file))return {};const auto doc=nlohmann::json::parse(ConfigFiles::Read(file,16384));Settings s;s.Enabled=doc.value("Enabled",true);s.Hotkey=doc.value("Hotkey",0);if(s.Hotkey!=0&&(s.Hotkey<VK_F1||s.Hotkey>VK_F24))throw std::runtime_error("Invalid Mod Menu shortcut.");return s;}
inline void Save(const std::filesystem::path& root,const Settings& s){const auto file=File(root);CacheBundleIO::Safe(file);ConfigFiles::Write(file,nlohmann::json{{"Enabled",s.Enabled},{"Hotkey",s.Hotkey}}.dump(2));}
}
