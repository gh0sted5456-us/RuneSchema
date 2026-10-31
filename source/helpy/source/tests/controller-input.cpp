#include "Generator/HelpyControllerInput.h"
#include <stdexcept>
static void Check(bool value){if(!value)throw std::runtime_error("Controller mode regression");}
int main(){using namespace PS::HelpyInputMode;Selection selected;selected.Update(Mode::Automatic,true,false);Check(selected.Controller);selected.Update(Mode::Automatic,false,true);Check(!selected.Controller);selected.Update(Mode::Controller,false,true);Check(selected.Controller);selected.Update(Mode::MouseKeyboard,true,false);Check(!selected.Controller);Check(Confirm(Glyphs::PlayStation)=="Cross"&&Cancel(Glyphs::Xbox)=="B"&&Confirm(Glyphs::Generic)=="South");
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={800,600};io.Fonts->Build();XINPUT_GAMEPAD pad{};pad.wButtons=XINPUT_GAMEPAD_A;pad.sThumbLX=20000;
    PS::HelpyControllerInput::Poll(0,&pad);ImGui::NewFrame();Check(ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown)&&ImGui::IsKeyDown(ImGuiKey_GamepadLStickRight));PS::HelpyControllerInput::Observe(0);Check(PS::HelpyControllerInput::Selection.Controller);ImGui::EndFrame();
    pad={};PS::HelpyControllerInput::Poll(0,&pad);ImGui::NewFrame();Check(!ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown)&&!ImGui::IsKeyDown(ImGuiKey_GamepadLStickRight));ImGui::EndFrame();PS::HelpyControllerInput::Poll(1,&pad);Check(!(io.ConfigFlags&ImGuiConfigFlags_NavEnableGamepad));Check(!PS::HelpyControllerInput::Initialized);ImGui::DestroyContext();
}
