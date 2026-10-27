#include "Runtime/HelpySettings.h"
#include <cassert>
#include <fstream>
#include <windows.h>
#include <iostream>
#undef assert
#define assert(x) do{if(!(x))throw std::runtime_error("line "+std::to_string(__LINE__)+": "+#x+"; "+PS::HelpySettings::Message());}while(false)
int main(){
    try{
    using namespace PS;namespace fs=std::filesystem;
    assert(HelpyShortcut::DefaultKey==VK_F2);
    for(const auto& binding:HelpyShortcut::Choices){
        assert(HelpyShortcut::Find(binding.Name)==&binding&&HelpyShortcut::Name(binding.Key)==binding.Name);
        for(const auto& other:HelpyShortcut::Choices)assert(&binding==&other||binding.Key!=other.Key);
    }
    assert(HelpyShortcut::Find("f5")->Key==VK_F5&&HelpyShortcut::Find("pageup")->Key==VK_PRIOR);
    for(const auto* unsupported:{"","Escape","Ctrl+F2","F25","Mouse1"})assert(!HelpyShortcut::Find(unsupported));
    HelpySettings::ApplyHotkey("X");assert(HelpySettings::Hotkey=='X'&&HelpySettings::HotkeyHint()=="X for Helpy");
    HelpySettings::ApplyHotkey("f5");assert(HelpySettings::Hotkey==VK_F5&&HelpySettings::HotkeyHint()=="F5 for Helpy");
    HelpySettings::ApplyHotkey("unsupported");assert(HelpySettings::Hotkey==VK_F2);
    std::ifstream panelFile(fs::path(__FILE__).parent_path().parent_path()/"include"/"Generator"/"RuneSchemaSettingsPanel.h");
    const std::string panel((std::istreambuf_iterator<char>(panelFile)),{});
    const auto toggle=panel.find("ImGui::Checkbox(\"Open Helpy at startup\"");
    assert(toggle!=std::string::npos&&toggle<panel.find("ImGui::TreeNode(\"Advanced\""));
    assert(panel.find("ImGui::Checkbox(\"Open Helpy at startup\"",toggle+1)==std::string::npos);
    const auto root=fs::temp_directory_path()/("helpy-startup-preference-"+std::to_string(GetCurrentProcessId()));fs::create_directories(root);
    HelpySettings::Configure(root);assert(!HelpySettings::OpenOnGameStart());
    assert(HelpySettings::ExtendFavoritesToGame());
    assert(HelpySettings::SetOpenOnGameStart(true));HelpySettings::Configure(root);assert(HelpySettings::OpenOnGameStart());
    const auto file=root/"settings"/"settings.jsonc";
    auto doc=nlohmann::json::parse(ConfigFiles::Read(file),nullptr,true,true);doc["ownerField"]="preserved";doc["extendFavoritesToGame"]=false;ConfigFiles::Write(file,doc.dump());
    // An external edit is never overwritten by an old in-memory draft.
    assert(!HelpySettings::SetOpenOnGameStart(false));assert(nlohmann::json::parse(ConfigFiles::Read(file),nullptr,true,true)["ownerField"]=="preserved");
    HelpySettings::Configure(root);assert(HelpySettings::ExtendFavoritesToGame());assert(HelpySettings::SetOpenOnGameStart(false));assert(nlohmann::json::parse(ConfigFiles::Read(file),nullptr,true,true)["ownerField"]=="preserved");
    HelpySettings::Configure(root);assert(!HelpySettings::OpenOnGameStart()&&HelpySettings::IsEnabled());
    doc["enabled"]=false;doc["openOnGameStart"]=true;ConfigFiles::Write(file,doc.dump());HelpySettings::Configure(root);assert(!HelpySettings::OpenOnGameStart()&&!HelpySettings::ExtendFavoritesToGame());
    ConfigFiles::Write(file,"invalid");HelpySettings::Configure(root);assert(!HelpySettings::OpenOnGameStart()&&!HelpySettings::SetOpenOnGameStart(true));assert(ConfigFiles::Read(file)=="invalid");
    fs::remove_all(root);
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}

