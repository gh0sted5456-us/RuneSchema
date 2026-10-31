#pragma once
#include "Core/HelpyInputMode.h"
#include "Generator/HelpyImGuiTheme.h"
#include <xinput.h>

namespace PS::HelpyControllerInput {
using StateFn=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);using CapsFn=DWORD(WINAPI*)(DWORD,DWORD,XINPUT_CAPABILITIES*);
inline HMODULE Library=nullptr;inline StateFn GetState=nullptr;inline CapsFn GetCaps=nullptr;
inline bool Initialized=false,Refresh=true;inline unsigned Connected=0;inline int Slot=0;
inline HelpyInputMode::Selection Selection;
inline void Changed(){Refresh=true;}
inline void Initialize(){if(Initialized)return;Initialized=true;Library=LoadLibraryExW(L"xinput1_4.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(Library){GetState=reinterpret_cast<StateFn>(GetProcAddress(Library,"XInputGetState"));GetCaps=reinterpret_cast<CapsFn>(GetProcAddress(Library,"XInputGetCapabilities"));}}
inline void Feed(const XINPUT_GAMEPAD& pad){
    auto& io=ImGui::GetIO();io.BackendFlags|=ImGuiBackendFlags_HasGamepad;
    for(const auto& [key,button]:std::array<std::pair<ImGuiKey,WORD>,14>{{{ImGuiKey_GamepadStart,XINPUT_GAMEPAD_START},{ImGuiKey_GamepadBack,XINPUT_GAMEPAD_BACK},{ImGuiKey_GamepadFaceDown,XINPUT_GAMEPAD_A},{ImGuiKey_GamepadFaceRight,XINPUT_GAMEPAD_B},{ImGuiKey_GamepadFaceLeft,XINPUT_GAMEPAD_X},{ImGuiKey_GamepadFaceUp,XINPUT_GAMEPAD_Y},{ImGuiKey_GamepadDpadLeft,XINPUT_GAMEPAD_DPAD_LEFT},{ImGuiKey_GamepadDpadRight,XINPUT_GAMEPAD_DPAD_RIGHT},{ImGuiKey_GamepadDpadUp,XINPUT_GAMEPAD_DPAD_UP},{ImGuiKey_GamepadDpadDown,XINPUT_GAMEPAD_DPAD_DOWN},{ImGuiKey_GamepadL1,XINPUT_GAMEPAD_LEFT_SHOULDER},{ImGuiKey_GamepadR1,XINPUT_GAMEPAD_RIGHT_SHOULDER},{ImGuiKey_GamepadL3,XINPUT_GAMEPAD_LEFT_THUMB},{ImGuiKey_GamepadR3,XINPUT_GAMEPAD_RIGHT_THUMB}}})io.AddKeyEvent(key,(pad.wButtons&button)!=0);
    const auto analog=[&](ImGuiKey key,float value,float minimum,float maximum){const float amount=std::clamp((value-minimum)/(maximum-minimum),0.f,1.f);io.AddKeyAnalogEvent(key,amount>.1f,amount);};
    analog(ImGuiKey_GamepadL2,pad.bLeftTrigger,30,255);analog(ImGuiKey_GamepadR2,pad.bRightTrigger,30,255);
    analog(ImGuiKey_GamepadLStickLeft,pad.sThumbLX,-7849,-32768);analog(ImGuiKey_GamepadLStickRight,pad.sThumbLX,7849,32767);analog(ImGuiKey_GamepadLStickUp,pad.sThumbLY,7849,32767);analog(ImGuiKey_GamepadLStickDown,pad.sThumbLY,-7849,-32768);
    analog(ImGuiKey_GamepadRStickLeft,pad.sThumbRX,-8689,-32768);analog(ImGuiKey_GamepadRStickRight,pad.sThumbRX,8689,32767);analog(ImGuiKey_GamepadRStickUp,pad.sThumbRY,8689,32767);analog(ImGuiKey_GamepadRStickDown,pad.sThumbRY,-8689,-32768);
}
inline void Poll(int mode,const XINPUT_GAMEPAD* native=nullptr){
    auto& io=ImGui::GetIO();if(mode==1){io.ConfigFlags&=~ImGuiConfigFlags_NavEnableGamepad;io.BackendFlags&=~ImGuiBackendFlags_HasGamepad;return;}
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad;
    if(native){Feed(*native);return;}Initialize();
    if(Refresh){Connected=0;if(GetCaps)for(unsigned i=0;i<4;++i){XINPUT_CAPABILITIES caps{};if(GetCaps(i,XINPUT_FLAG_GAMEPAD,&caps)==ERROR_SUCCESS)Connected|=1u<<i;}Refresh=false;for(int i=0;i<4;++i)if(Connected&(1u<<i)){Slot=i;break;}}
    XINPUT_STATE state{};if((Connected&(1u<<Slot))&&GetState&&GetState(Slot,&state)==ERROR_SUCCESS)Feed(state.Gamepad);
    else {Connected&=~(1u<<Slot);Feed({});io.BackendFlags&=~ImGuiBackendFlags_HasGamepad;}
}
inline void Observe(int mode){
    bool pad=false,keys=false;for(int key=ImGuiKey_GamepadStart;key<=ImGuiKey_GamepadRStickDown;++key)pad|=ImGui::IsKeyPressed(ImGuiKey(key),false)||ImGui::IsKeyDown(ImGuiKey(key));
    for(int key=ImGuiKey_Tab;key<=ImGuiKey_KeypadEqual;++key)keys|=ImGui::IsKeyPressed(ImGuiKey(key),false);
    const auto& io=ImGui::GetIO();keys|=io.MouseDelta.x!=0||io.MouseDelta.y!=0||io.MouseWheel!=0||ImGui::IsMouseClicked(0)||ImGui::IsMouseClicked(1);
    Selection.Update(HelpyInputMode::Mode(mode),pad,keys);
}
inline void Glyph(const char* label,bool confirm,int style){
    const float size=ImGui::GetFrameHeight();const auto at=ImGui::GetCursorScreenPos();const ImVec2 center{at.x+size*.5f,at.y+size*.5f};const float radius=size*.35f;auto* draw=ImGui::GetWindowDrawList();const auto color=ImGui::GetColorU32(HelpyImGuiTheme::Current.accent);
    draw->AddCircle(center,radius,color,24,1.5f);
    if(style==2){if(confirm){const float r=radius*.5f;draw->AddLine({center.x-r,center.y-r},{center.x+r,center.y+r},color,1.5f);draw->AddLine({center.x-r,center.y+r},{center.x+r,center.y-r},color,1.5f);}else draw->AddCircle(center,radius*.5f,color,24,1.5f);}
    else {const char* text=style==3?(confirm?"S":"E"):(confirm?"A":"B");const auto bounds=ImGui::CalcTextSize(text);draw->AddText({center.x-bounds.x*.5f,center.y-bounds.y*.5f},color,text);}
    ImGui::Dummy({size,size});ImGui::SameLine();ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(label);
}
inline void Hints(int style){Glyph("Select",true,style);ImGui::SameLine();Glyph("Back / hide",false,style);if(ImGui::GetContentRegionAvail().x>200){ImGui::SameLine();ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(style==2?"L1 / R1  Sections":"LB / RB  Sections");}}
inline void Shutdown(){if(Library)FreeLibrary(Library);Library=nullptr;GetState=nullptr;GetCaps=nullptr;Initialized=false;Refresh=true;Connected=0;}
}
