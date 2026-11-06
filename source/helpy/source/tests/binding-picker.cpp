#include "Generator/HelpyBindingPicker.h"
#include <cassert>
#include <iostream>
#include <set>
namespace B=PS::HelpyBindingPicker;namespace H=PS::HelpyPreviewHooks;
namespace PS::HelpyControllerDevice {Reading TestReading;Reading Read(bool){return TestReading;}void Shutdown(){}}
static bool FeedPadInput=false;
static std::set<int> Held;
static bool KeyState(int key){return Held.contains(key);}
static void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int main()try{
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={1000,720};io.DeltaTime=1.f/60;io.Fonts->Build();io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad|ImGuiConfigFlags_NavEnableKeyboard;io.BackendFlags|=ImGuiBackendFlags_HasGamepad;B::ReadKey=KeyState;
    std::string keyboard="F8",controller="Guide",forcedError;
    const auto frame=[&]{B::NextRead=0;B::Tick();if(FeedPadInput)PS::HelpyControllerInput::Feed(PS::HelpyControllerDevice::TestReading.Pad);ImGui::NewFrame();PS::HelpyControllerInput::Observe(0);ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({1000,720});ImGui::Begin("Fixture");B::Edit("test","Open interface",keyboard,controller,[&](const std::string&,const std::string&){return forcedError;});ImGui::End();ImGui::Render();};
    const auto click=[&](ImVec4 rect){io.AddMousePosEvent((rect.x+rect.z)*.5f,(rect.y+rect.w)*.5f);io.AddMouseButtonEvent(0,false);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();};
    for(int i=0;i<3;++i)frame();click(H::BindingEditors.at("test"));frame();Check(B::Active(),"Visual binding editor did not open");
    click(H::BindingActions.at("Clear"));click(H::BindingKeys.at("Ctrl"));click(H::BindingKeys.at("Shift"));click(H::BindingKeys.at("F2"));Check(keyboard=="F8","Selection saved before Lock in");click(H::BindingLock);Check(keyboard=="Ctrl+Shift+F2"&&controller=="Guide"&&!B::Active(),"Multi-key binding did not lock in");
    click(H::BindingEditors.at("test"));frame();click(H::BindingActions.at("Clear"));click(H::BindingKeys.at("A"));click(H::BindingActions.at("Cancel"));Check(keyboard=="Ctrl+Shift+F2"&&!B::Active(),"Cancel changed the saved binding");
    click(H::BindingEditors.at("test"));frame();click(H::BindingActions.at("Clear"));click(H::BindingActions.at("Record"));frame();Held={VK_CONTROL,'K'};frame();frame();Held.clear();frame();Check(!B::Listening&&B::JustRecorded==false,"Recorded combination did not complete");click(H::BindingLock);Check(keyboard=="Ctrl+K","Physical combination did not save");
    click(H::BindingEditors.at("test"));frame();click(H::BindingActions.at("Clear"));click(H::BindingKeys.at("Alt"));click(H::BindingKeys.at("F4"));click(H::BindingLock);Check(B::Active()&&!B::Error.empty()&&keyboard=="Ctrl+K","Reserved OS combination was accepted");click(H::BindingActions.at("Cancel"));Check(!B::Active(),"Cancel is not reachable after a validation error");
    click(H::BindingEditors.at("test"));frame();
    const auto tab=H::MenuFocus.at("binding-device/Keyboard");auto* window=ImGui::FindWindowByID(tab.second);Check(window!=nullptr,"Binding tab window is missing");
    ImGui::ClearActiveID();ImGui::FocusWindow(window);ImGui::SetFocusID(tab.first,window);ImGui::GetCurrentContext()->NavInputSource=ImGuiInputSource_Gamepad;
    io.AddKeyEvent(ImGuiKey_GamepadR1,true);frame();io.AddKeyEvent(ImGuiKey_GamepadR1,false);frame();
    Check(B::Device==0,"A bumper unexpectedly changed the binding device tab");
    io.AddKeyEvent(ImGuiKey_GamepadDpadRight,true);frame();io.AddKeyEvent(ImGuiKey_GamepadDpadRight,false);frame();frame();
    io.AddKeyEvent(ImGuiKey_GamepadFaceDown,true);frame();io.AddKeyEvent(ImGuiKey_GamepadFaceDown,false);frame();
    Check(B::Device==1&&B::Active(),"Controller Confirm cannot enter the other binding device tab inside a modal");
    click(H::MenuTabs.at("binding-device/Keyboard"));Check(B::Active()&&B::Device==0,"Clicking a binding tab closed its parent popup");
    click(H::MenuTabs.at("binding-device/Controller"));Check(B::Active()&&B::Device==1,"Controller tab is not clickable");
    Check(H::BindingPad.contains("Guide")&&H::BindingPad.contains("South"),"Controller diagram is missing clickable buttons");
    click(H::BindingPad.at("Guide"));click(H::BindingPad.at("View"));click(H::BindingPad.at("West"));Check(controller=="Guide","Controller selection saved before Lock in");if(B::DraftPad!=(0x20|0x4000))throw std::runtime_error("Controller diagram selection mismatch: "+std::to_string(B::DraftPad));click(H::BindingLock);Check(PS::InputBindings::ParsePad(controller)==(0x20|0x4000),("Controller chord did not save: "+controller+" error="+B::Error).c_str());
    click(H::BindingEditors.at("test"));B::Device=1;PS::HelpyImGuiTheme::Current.controllerGlyphs=2;H::BindingPad.clear();frame();Check(H::BindingPad.contains("Touchpad"),"Controller diagram ignored the manual PlayStation glyph setting");click(H::BindingActions.at("Cancel"));PS::HelpyImGuiTheme::Current.controllerGlyphs=0;
    click(H::BindingEditors.at("test"));B::Device=1;frame();FeedPadInput=true;click(H::BindingActions.at("Record"));frame();
    PS::HelpyControllerDevice::TestReading={true,{},PS::HelpyControllerDevice::PlayStation};PS::HelpyControllerDevice::TestReading.Pad.wButtons=XINPUT_GAMEPAD_RIGHT_SHOULDER|XINPUT_GAMEPAD_X;frame();frame();Check(B::Device==1&&B::Listening,"Recording a shoulder button changed the device tab");
    PS::HelpyControllerDevice::TestReading.Pad={};frame();frame();Check(!B::Listening&&B::DraftPad==(XINPUT_GAMEPAD_RIGHT_SHOULDER|XINPUT_GAMEPAD_X),"Physical controller combination was not recorded");click(H::BindingLock);Check(PS::InputBindings::ParsePad(controller)==(XINPUT_GAMEPAD_RIGHT_SHOULDER|XINPUT_GAMEPAD_X)&&keyboard=="Ctrl+K","Recording a controller combination changed the keyboard binding");FeedPadInput=false;PS::HelpyControllerDevice::TestReading={};
    io.DisplaySize={540,500};forcedError=std::string(192,'W');click(H::BindingEditors.at("test"));frame();const auto lock=H::BindingLock;click(lock);Check(B::Active()&&H::BindingLock.y==lock.y,"A long binding warning moved Lock in");click(H::BindingActions.at("Cancel"));Check(!B::Active(),"Long warnings hid Cancel");
    B::Cancel();ImGui::DestroyContext();std::cout<<"Visual keyboard selection, recording, Lock in, cancel and reserved combinations passed.\n";
}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
