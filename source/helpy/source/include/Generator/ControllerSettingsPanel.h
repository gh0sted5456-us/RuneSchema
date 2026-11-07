#pragma once
#include "Generator/HelpyBindingPicker.h"
#include "Core/HelpyControllerBindings.h"

namespace PS::ControllerSettingsPanel {
inline void Render(const std::function<void()>& shortcuts){
 auto& theme=HelpyImGuiTheme::Current;bool changed=false;
 ImGui::SeparatorText("Detected controller");ImGui::TextWrapped("%s",HelpyControllerDevice::Status().c_str());
 ImGui::TextUnformatted("Input mode");ImGui::SetNextItemWidth(-1);changed|=ImGui::Combo("##controller-mode",&theme.inputMode,"Auto detect\0Disabled (mouse & keyboard)\0Always on\0");
 ImGui::TextUnformatted("Glyphs");ImGui::SetNextItemWidth(-1);changed|=ImGui::Combo("##controller-glyphs",&theme.controllerGlyphs,"Automatic (device labels)\0Xbox\0PlayStation\0Generic\0Nintendo\0");
 changed|=HelpyImGuiTheme::Toggle("Right-stick cursor",&theme.controllerCursor);
 ImGui::TextUnformatted("Cursor speed");ImGui::SetNextItemWidth(-1);changed|=ImGui::SliderFloat("##controller-cursor-speed",&theme.controllerCursorSpeed,200.f,1800.f,"%.0f px/s");
 ImGui::TextWrapped("Triggers change main sections. Bumpers change panels. The D-pad moves focus within the active panel. The left stick scrolls the focused panel or the panel under the pointer. Right stick enters pointer mode: the bottom face button left-clicks or drags and the left face button right-clicks. The D-pad returns to focus navigation. Right-stick click grabs the item or selected group; click again over the cart to drop. Left-stick click toggles multi-select in Item Bay. On the map, left stick pans, left-stick press zooms in and the top face button zooms out.");
 changed|=HelpyImGuiTheme::Toggle("PlayStation touchpad pointer",&theme.controllerTouchpad);
 ImGui::TextWrapped("Swipe with one finger to move; tap to left-click, or physically press to click and drag. Two fingers right-click. PlayStation glyphs stay active.");
 if(changed)HelpyImGuiTheme::SaveCurrent();
 const auto family=HelpyControllerBindings::Family(HelpyControllerInput::ReportedGlyphs);auto profile=HelpyControllerBindings::Read(theme.controllerBindings,family);
 ImGui::SeparatorText("Controller navigation");ImGui::TextWrapped("Profile: %s. Assigning an occupied button swaps the two actions. Bindings apply after held buttons are released and are saved with your Helpy settings. Confirm and Item actions also control left-click and right-click in pointer mode.",family);
 if(HelpyImGuiTheme::SoftButton("Reset this controller profile")){theme.controllerBindings.erase(family);profile=HelpyControllerBindings::Defaults();HelpyImGuiTheme::SaveCurrent();}
 for(size_t i=0;i<profile.size();++i){const auto& action=HelpyControllerBindings::Actions[i];const auto id=std::string("controller-")+family+"-"+action.Id;auto keyboard=std::string("None"),button=InputBindings::PadText(profile[i]);
  const auto check=[](const std::string&,const std::string& value){try{if(HelpyControllerBindings::Supported(InputBindings::ParsePad(value)))return std::string{};}catch(...){}return std::string("Choose one face, shoulder, trigger or direction button.");};
  if(HelpyBindingPicker::Edit(id.c_str(),action.Label,keyboard,button,check,"None",InputBindings::PadText(action.Button).c_str(),true)){
   HelpyControllerBindings::Rebind(profile,i,InputBindings::ParsePad(button));theme.controllerBindings[family]=HelpyControllerBindings::Write(profile);HelpyImGuiTheme::SaveCurrent();
  }ImGui::Spacing();
 }
 ImGui::SeparatorText("Game and app shortcuts");ImGui::TextWrapped("Guide, touchpad presses and multi-button shortcuts use the shared binding editor.");if(shortcuts&&HelpyImGuiTheme::SoftButton("Helpy shortcut bindings"))shortcuts();
}
}
