#pragma once
#include "Generator/HelpyBindingPicker.h"
#include "Generator/HelpyFeatureSettings.h"
#include "Runtime/HelpySettings.h"
#include <map>
namespace PS::HelpyBindingActions {
struct Action{std::string Id,Name,Keyboard,Controller;};
inline std::map<std::string,Action> Extensions;
inline std::string Keyboard(){const auto& value=HelpyImGuiTheme::Current.helpyKeyboardBinding;return value.empty()?HelpySettings::HotkeyName():value;}
inline std::string Controller(){const auto& t=HelpyImGuiTheme::Current;return t.controllerBinding.empty()?(t.controllerShortcut==1?"Guide":t.controllerShortcut==2?"View+Menu":"None"):t.controllerBinding;}
inline std::vector<Action> Actions(){
    const auto& t=HelpyImGuiTheme::Current;std::vector<Action> result{{"helpy","Helpy",Keyboard(),Controller()},{"fps","FPS display",t.fpsHotkey,t.fpsControllerBinding}};
    if(HelpyFeatureSettings::Key&&HelpyFeatureSettings::Menu.Enabled)result.push_back({"mod-menu2","Mod Menu 2",ModMenu2Settings::Keyboard(HelpyFeatureSettings::Menu),HelpyFeatureSettings::Menu.Controller});
    for(const auto& [id,action]:Extensions)result.push_back(action);return result;
}
inline std::string Conflict(const std::string& id,const std::string& keyboard,const std::string& controller){
    const auto keys=InputBindings::Parse(keyboard);const auto pad=InputBindings::ParsePad(controller);
    for(const auto& other:Actions())if(other.Id!=id&&(InputBindings::Overlap(keys,InputBindings::Parse(other.Keyboard))||InputBindings::Overlap(pad,InputBindings::ParsePad(other.Controller))))return "This combination overlaps "+other.Name+". Choose another binding.";
    return {};
}
inline HelpyBindingPicker::Check Check(const char* id){return [id=std::string(id)](const std::string& keys,const std::string& pad){return Conflict(id,keys,pad);};}
struct Runtime {std::string Text;InputBindings::Keys Keys;InputBindings::Press KeyPress,PadPress;};
inline std::map<std::string,Runtime> States;
inline bool KeyPressed(const char* id,const std::string& text,bool allowed){
    auto& state=States[id];try{if(state.Text!=text){state.Keys=InputBindings::Parse(text);state.Text=text;}if(auto* context=ImGui::GetCurrentContext();context&&context->IO.WantTextInput)allowed&=std::all_of(state.Keys.begin(),state.Keys.end(),[](auto key){return key==VK_CONTROL||key==VK_SHIFT||key==VK_MENU||(key>=VK_F1&&key<=VK_F24);});const bool down=InputBindings::Down(state.Keys,[](int key){return (GetAsyncKeyState(key)&0x8000)!=0;});return state.KeyPress.Poll(text,down,allowed);}catch(const std::exception&){state.KeyPress.Reset();return false;}
}
inline bool PadPressed(const char* id,const std::string& text,uint32_t down,bool allowed){
    auto& state=States[id];try{const auto mask=InputBindings::ParsePad(text);return state.PadPress.Poll(text,mask&&(down&mask)==mask,allowed);}catch(const std::exception&){state.PadPress.Reset();return false;}
}
}
