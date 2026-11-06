#pragma once
#include "Core/ConfigFiles.h"
#include "Core/CacheBundleIO.h"
#include <nlohmann/json.hpp>
#include "InputBindings.h"

namespace PS::ModMenu2Settings {
struct Settings {bool Enabled=true;int Hotkey=0;std::string Keyboard,Controller="None";};
inline std::string Keyboard(const Settings& settings){return settings.Keyboard.empty()?(settings.Hotkey?"F"+std::to_string(settings.Hotkey-0x70+1):"None"):settings.Keyboard;}
inline std::filesystem::path File(const std::filesystem::path& root){return root/"settings/modmenu2.json";}
inline Settings Read(const std::filesystem::path& root){const auto file=File(root);CacheBundleIO::Safe(file);if(!std::filesystem::exists(file))return {};const auto doc=nlohmann::json::parse(ConfigFiles::Read(file,16384));Settings s;s.Enabled=doc.value("Enabled",true);s.Hotkey=doc.value("Hotkey",0);if(s.Hotkey!=0&&(s.Hotkey<VK_F1||s.Hotkey>VK_F24))throw std::runtime_error("Invalid Mod Menu shortcut.");s.Keyboard=doc.value("Keyboard",std::string{});s.Controller=InputBindings::PadText(InputBindings::ParsePad(doc.value("Controller",std::string("None"))));if(!s.Keyboard.empty())s.Keyboard=InputBindings::Text(InputBindings::Parse(s.Keyboard));return s;}
inline void Save(const std::filesystem::path& root,const Settings& s){const auto file=File(root);CacheBundleIO::Safe(file);ConfigFiles::Write(file,nlohmann::json{{"Enabled",s.Enabled},{"Hotkey",s.Hotkey},{"Keyboard",InputBindings::Text(InputBindings::Parse(Keyboard(s)))},{"Controller",InputBindings::PadText(InputBindings::ParsePad(s.Controller))}}.dump(2));}
}
