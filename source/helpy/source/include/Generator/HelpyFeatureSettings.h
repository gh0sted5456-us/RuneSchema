#pragma once
#include "ModMenu2Key.h"
#include "ModMenu2Settings.h"
#include "HelpyHudLayout.h"
#include "Runtime/HostServices.h"

namespace PS::HelpyFeatureSettings {
inline ModMenu2Settings::Settings Menu;
inline HelpyHudLayout::Settings Hud;
inline std::filesystem::path Root;
inline bool Key=false;inline std::string Status;
inline void CheckKey(){try{Key=ModMenu2Key::Present(HostServices::ModDirectory());Status=Key?"Mod Menu 2 activation key accepted.":"Mod Menu 2 is locked; add a valid modmenu2.key to the Helpy plugin folder.";}catch(const std::exception& error){Key=false;Status=error.what();}}
inline void Load(){const auto root=HostServices::ModDirectory();if(root==Root)return;Root=root;CheckKey();try{Menu=ModMenu2Settings::Read(root);}catch(const std::exception& error){Menu={false,0};Status=error.what();}try{Hud=HelpyHudLayout::Read(root);}catch(const std::exception& error){Hud={};Status=error.what();}}
inline void SaveMenu(){try{ModMenu2Settings::Save(HostServices::ModDirectory(),Menu);Status="Saved. Mod Menu activation changes take effect after restarting Dragonwilds.";}catch(const std::exception& error){Status=error.what();}}
inline void SaveHud(const HelpyHudLayout::Settings& draft){try{HelpyHudLayout::Save(HostServices::ModDirectory(),draft);Hud=draft;Hud.Custom=true;Status="HUD layout saved and applied.";}catch(const std::exception& error){Status=error.what();}}
}
