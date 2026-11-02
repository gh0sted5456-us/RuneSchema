#pragma once
#include "Core/HelpyInputMode.h"
#include "Core/HelpyControllerDevice.h"
#include "Generator/HelpyImGuiTheme.h"
#include <xinput.h>
#include <imgui_internal.h>
#include <cstring>
#include <utility>

namespace PS::HelpyControllerInput {
using StateFn=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);using CapsFn=DWORD(WINAPI*)(DWORD,DWORD,XINPUT_CAPABILITIES*);
inline HMODULE Library=nullptr;inline StateFn GetState=nullptr;inline CapsFn GetCaps=nullptr;
inline bool Initialized=false,Refresh=true;inline unsigned Connected=0;inline int Slot=0;
inline HelpyInputMode::Selection Selection;
inline HelpyControllerDevice::Labels ReportedGlyphs=HelpyControllerDevice::Xbox;
inline bool JustActivated=false;
inline bool KeepShortcuts=false;
inline bool ShortcutActivated=false;
inline auto DeviceRead=&HelpyControllerDevice::Read;
#if defined(HELPY_PREVIEW)
inline HelpyControllerDevice::Reading PreviewReading;
#endif
inline void Changed(){Refresh=true;}
inline void Initialize(){if(Initialized)return;Initialized=true;Refresh=true;Library=LoadLibraryExW(L"xinput1_4.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(Library){GetState=reinterpret_cast<StateFn>(GetProcAddress(Library,"XInputGetState"));GetCaps=reinterpret_cast<CapsFn>(GetProcAddress(Library,"XInputGetCapabilities"));}}
inline void Feed(const XINPUT_GAMEPAD& pad){
    auto& io=ImGui::GetIO();io.BackendFlags|=ImGuiBackendFlags_HasGamepad;
    for(const auto& [key,button]:std::array<std::pair<ImGuiKey,WORD>,14>{{{ImGuiKey_GamepadStart,XINPUT_GAMEPAD_START},{ImGuiKey_GamepadBack,XINPUT_GAMEPAD_BACK},{ImGuiKey_GamepadFaceDown,XINPUT_GAMEPAD_A},{ImGuiKey_GamepadFaceRight,XINPUT_GAMEPAD_B},{ImGuiKey_GamepadFaceLeft,XINPUT_GAMEPAD_X},{ImGuiKey_GamepadFaceUp,XINPUT_GAMEPAD_Y},{ImGuiKey_GamepadDpadLeft,XINPUT_GAMEPAD_DPAD_LEFT},{ImGuiKey_GamepadDpadRight,XINPUT_GAMEPAD_DPAD_RIGHT},{ImGuiKey_GamepadDpadUp,XINPUT_GAMEPAD_DPAD_UP},{ImGuiKey_GamepadDpadDown,XINPUT_GAMEPAD_DPAD_DOWN},{ImGuiKey_GamepadL1,XINPUT_GAMEPAD_LEFT_SHOULDER},{ImGuiKey_GamepadR1,XINPUT_GAMEPAD_RIGHT_SHOULDER},{ImGuiKey_GamepadL3,XINPUT_GAMEPAD_LEFT_THUMB},{ImGuiKey_GamepadR3,XINPUT_GAMEPAD_RIGHT_THUMB}}}){
        const bool stick=(key==ImGuiKey_GamepadDpadLeft&&pad.sThumbLX<-12000)||(key==ImGuiKey_GamepadDpadRight&&pad.sThumbLX>12000)||(key==ImGuiKey_GamepadDpadUp&&pad.sThumbLY>12000)||(key==ImGuiKey_GamepadDpadDown&&pad.sThumbLY<-12000);
        io.AddKeyEvent(key,(pad.wButtons&button)!=0||stick);
    }
    const auto analog=[&](ImGuiKey key,float value,float minimum,float maximum){const float amount=std::clamp((value-minimum)/(maximum-minimum),0.f,1.f);io.AddKeyAnalogEvent(key,amount>.1f,amount);};
    analog(ImGuiKey_GamepadL2,pad.bLeftTrigger,30,255);analog(ImGuiKey_GamepadR2,pad.bRightTrigger,30,255);
    // ImGui uses left-stick keys for scrolling; Helpy uses the stick for focus navigation instead.
    analog(ImGuiKey_GamepadRStickLeft,pad.sThumbRX,-8689,-32768);analog(ImGuiKey_GamepadRStickRight,pad.sThumbRX,8689,32767);analog(ImGuiKey_GamepadRStickUp,pad.sThumbRY,8689,32767);analog(ImGuiKey_GamepadRStickDown,pad.sThumbRY,-8689,-32768);
}
inline void Poll(int mode,const XINPUT_GAMEPAD* native=nullptr){
    auto& io=ImGui::GetIO();if(mode==1){Feed({});if(!KeepShortcuts)HelpyControllerDevice::Shutdown();io.ConfigFlags&=~ImGuiConfigFlags_NavEnableGamepad;io.BackendFlags&=~ImGuiBackendFlags_HasGamepad;return;}
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad;
    const auto mapped=DeviceRead(Refresh);
    if(mapped.Connected){ReportedGlyphs=mapped.Glyphs;Feed(mapped.Pad);Refresh=false;return;}
    if(native){Refresh=false;ReportedGlyphs=mapped.Connected?mapped.Glyphs:HelpyControllerDevice::Xbox;Feed(*native);return;}Initialize();
    if(Refresh){Connected=0;if(GetCaps)for(unsigned i=0;i<4;++i){XINPUT_CAPABILITIES caps{};if(GetCaps(i,XINPUT_FLAG_GAMEPAD,&caps)==ERROR_SUCCESS)Connected|=1u<<i;}Refresh=false;for(int i=0;i<4;++i)if(Connected&(1u<<i)){Slot=i;break;}}
    XINPUT_STATE chosen{};bool found=false;static std::array<XINPUT_GAMEPAD,4> prior{};
    for(int i=0;i<4;++i)if(Connected&(1u<<i)){XINPUT_STATE state{};
        if(!GetState||GetState(i,&state)!=ERROR_SUCCESS){Connected&=~(1u<<i);continue;}
        if(i==Slot){chosen=state;found=true;}
        if(HelpyControllerDevice::Active(state.Gamepad)&&std::memcmp(&state.Gamepad,&prior[i],sizeof(state.Gamepad))!=0){chosen=state;found=true;Slot=i;}
        prior[i]=state.Gamepad;
    }
    if(found){ReportedGlyphs=HelpyControllerDevice::Xbox;Feed(chosen.Gamepad);}else{Feed({});io.BackendFlags&=~ImGuiBackendFlags_HasGamepad;}
}
inline void Observe(int mode){
    bool pad=false,keys=false;static std::array<float,ImGuiKey_GamepadRStickDown-ImGuiKey_GamepadStart+1> previous{};
    for(int key=ImGuiKey_GamepadStart;key<=ImGuiKey_GamepadRStickDown;++key){const auto value=ImGui::GetKeyData(ImGuiKey(key))->AnalogValue;auto& prior=previous[key-ImGuiKey_GamepadStart];pad|=ImGui::IsKeyPressed(ImGuiKey(key),false)||(value>.1f&&std::abs(value-prior)>.08f);prior=value;}
    for(int key=ImGuiKey_Tab;key<=ImGuiKey_KeypadEqual;++key)keys|=ImGui::IsKeyPressed(ImGuiKey(key),false);
    const auto& io=ImGui::GetIO();keys|=io.MouseDelta.x!=0||io.MouseDelta.y!=0||io.MouseWheel!=0||ImGui::IsMouseClicked(0)||ImGui::IsMouseClicked(1);
    const bool prior=Selection.Controller;const bool shortcut=std::exchange(ShortcutActivated,false);Selection.Update(HelpyInputMode::Mode(mode),pad||shortcut,keys&&!shortcut);JustActivated=Selection.Controller&&!prior;
}
inline bool DefaultFocus(bool arriving=false){
    auto& context=*ImGui::GetCurrentContext();auto* window=ImGui::GetCurrentWindow();
    if(!Selection.Controller||!context.LastItemData.ID||context.ActiveId||context.DragDropActive||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId))return false;
    const bool missing=!context.NavWindow||!context.NavId||(!context.NavWindow->Active&&!context.NavWindow->WasActive);
    if(!missing&&(!arriving||context.NavWindow->RootWindow!=window->RootWindow))return false;
    // NoFocusOnAppearing keeps mouse-mode windows passive; controller entry needs an explicit target.
    ImGui::FocusWindow(window,ImGuiFocusRequestFlags_UnlessBelowModal);if(context.NavWindow!=window)return false;
    ImGui::SetNavWindow(window);context.NavInitResult.ID=0;
    ImGui::SetFocusID(context.LastItemData.ID,window);context.NavInputSource=ImGuiInputSource_Gamepad;ImGui::SetNavCursorVisible(true);return true;
}
inline void DrawGlyph(ImVec2 center,float radius,size_t button,int style){
    auto* draw=ImGui::GetWindowDrawList();const auto color=ImGui::GetColorU32(HelpyImGuiTheme::Current.accent);
    draw->AddCircle(center,radius,color,24,1.5f);
    using B=HelpyControllerDevice::Button;const auto glyph=HelpyControllerDevice::Resolve(style,ReportedGlyphs)[button];const float r=radius*.5f;
    if(glyph==B::Cross){draw->AddLine({center.x-r,center.y-r},{center.x+r,center.y+r},color,1.5f);draw->AddLine({center.x-r,center.y+r},{center.x+r,center.y-r},color,1.5f);}
    else if(glyph==B::Circle)draw->AddCircle(center,r,color,24,1.5f);
    else if(glyph==B::Square)draw->AddRect({center.x-r,center.y-r},{center.x+r,center.y+r},color,0,0,1.5f);
    else if(glyph==B::Triangle)draw->AddTriangle({center.x,center.y-r},{center.x+r,center.y+r},{center.x-r,center.y+r},color,1.5f);
    else {const char* text=glyph==B::A?"A":glyph==B::B?"B":glyph==B::X?"X":glyph==B::Y?"Y":glyph==B::South?"S":glyph==B::East?"E":glyph==B::West?"W":"N";const auto bounds=ImGui::CalcTextSize(text);draw->AddText({center.x-bounds.x*.5f,center.y-bounds.y*.5f},color,text);}
}
inline void Glyph(const char* label,size_t button,int style){
    const float size=ImGui::GetFrameHeight();const auto at=ImGui::GetCursorScreenPos();DrawGlyph({at.x+size*.5f,at.y+size*.5f},size*.35f,button,style);
    ImGui::Dummy({size,size});ImGui::SameLine();ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(label);
}
inline void Hints(int style){Glyph("Select",0,style);ImGui::SameLine();Glyph("Back / hide",1,style);if(ImGui::GetContentRegionAvail().x>150){ImGui::SameLine();Glyph("Actions",2,style);}if(ImGui::GetContentRegionAvail().x>260){ImGui::SameLine();ImGui::AlignTextToFramePadding();const auto labels=HelpyControllerDevice::Resolve(style,ReportedGlyphs);ImGui::TextUnformatted(labels==HelpyControllerDevice::PlayStation?"L1/R1 Tabs · L2/R2 Pages":labels==HelpyControllerDevice::Nintendo?"L/R Tabs · ZL/ZR Pages":"LB/RB Tabs · LT/RT Pages");}}
inline void Suspend(){if(!KeepShortcuts){HelpyControllerDevice::Shutdown();Refresh=true;}Connected=0;}
inline void Shutdown(){KeepShortcuts=false;ShortcutActivated=false;Suspend();if(Library)FreeLibrary(Library);Library=nullptr;GetState=nullptr;GetCaps=nullptr;Initialized=false;}
}
