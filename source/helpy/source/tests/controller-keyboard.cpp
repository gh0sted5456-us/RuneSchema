#include "Generator/HelpyTextInput.h"
#include <stdexcept>
#include <vector>
#include <iostream>
static void Check(bool value,const char* reason="Controller text entry regression"){if(!value){std::cerr<<reason<<'\n';throw std::runtime_error(reason);}}
int main(){
    namespace K=PS::HelpyTextInput;namespace H=PS::HelpyPreviewHooks;
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={800,600};io.DeltaTime=1.f/60;io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad;io.BackendFlags|=ImGuiBackendFlags_HasGamepad;io.Fonts->Build();
    std::string model="old";bool focus=true;ImGuiInputTextFlags flags=0;bool nested=false;size_t capacity=65;
    const auto frame=[&]{ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({800,600});ImGui::Begin("Fixture");
        if(nested){ImGui::OpenPopup("PIN form");ImGui::BeginPopupModal("PIN form");}
        std::vector<char> ephemeral(capacity);std::strcpy(ephemeral.data(),model.c_str());if(focus){ImGui::SetKeyboardFocusHere();focus=false;}
        ImGui::GetCurrentContext()->NavInputSource=ImGuiInputSource_Gamepad;
        if(K::InputText("Value",ephemeral.data(),ephemeral.size(),flags))model=ephemeral.data();
        if(nested)ImGui::EndPopup();ImGui::End();ImGui::Render();};
    const auto click=[&](const char* label){const auto rect=H::ControllerKeys.at(label);io.AddMousePosEvent((rect.x+rect.z)*.5f,(rect.y+rect.w)*.5f);io.AddMouseButtonEvent(0,false);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();};
    for(unsigned i=0;i<4;++i)frame();Check(K::Owner!=0);click("Clear");click("q");click("Done");frame();Check(model=="q"&&K::Owner==0&&K::Draft.empty());
    io.AddKeyEvent(ImGuiKey_Escape,true);frame();io.AddKeyEvent(ImGuiKey_Escape,false);frame();focus=true;flags=ImGuiInputTextFlags_Password;nested=true;
    for(unsigned i=0;i<4;++i)frame();Check(K::Owner!=0);click("Clear");click("1");click("2");click("Done");frame();Check(model=="12"&&K::Draft.empty());
    Check(K::Allowed('1',ImGuiInputTextFlags_CharsDecimal)&&!K::Allowed('q',ImGuiInputTextFlags_CharsDecimal)&&!K::Allowed(' ',ImGuiInputTextFlags_CharsNoBlank));
    io.AddKeyEvent(ImGuiKey_Escape,true);frame();io.AddKeyEvent(ImGuiKey_Escape,false);frame();focus=true;for(unsigned i=0;i<4;++i)frame();io.AddKeyEvent(ImGuiKey_GamepadFaceUp,true);frame();io.AddKeyEvent(ImGuiKey_GamepadFaceUp,false);frame();Check(K::Owner!=0,"Capacity test did not open the keyboard");click("1");click("2");capacity=3;frame();Check(K::Owner==0&&K::Draft.empty()&&model=="12","A smaller field must cancel its pending keyboard draft");
    K::Clear();ImGui::DestroyContext();
}
