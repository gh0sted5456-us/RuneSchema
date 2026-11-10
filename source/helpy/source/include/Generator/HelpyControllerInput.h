#pragma once
#include "Core/HelpyInputMode.h"
#include "Core/HelpyControllerDevice.h"
#include "Core/HelpyTouchpadGesture.h"
#include "Core/HelpyControllerPointer.h"
#include "Generator/HelpyImGuiTheme.h"
#include <xinput.h>
#include <imgui_internal.h>
#include <cstring>
#include <utility>
#include <optional>

namespace PS::HelpyControllerInput {
using StateFn=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);using CapsFn=DWORD(WINAPI*)(DWORD,DWORD,XINPUT_CAPABILITIES*);
inline HMODULE Library=nullptr;inline StateFn GetState=nullptr;inline CapsFn GetCaps=nullptr;
inline bool Initialized=false,Refresh=true;inline uint64_t NextXInputProbe=0;inline unsigned Connected=0;inline int Slot=0;
inline HelpyInputMode::Selection Selection;
inline HelpyControllerDevice::Labels ReportedGlyphs=HelpyControllerDevice::Xbox;
inline bool JustActivated=false;
inline bool StickUsing=false,PointerUsing=false,StickClickArmed=false;
inline bool PointerMode=false,PointerEnabled=true,DragLatched=false,StickWasDown=false;
inline void CancelGrab(){
    if(DragLatched&&ImGui::GetCurrentContext()){if(ImGui::GetCurrentContext()->DragDropActive)ImGui::ClearDragDrop();if(ImGui::GetCurrentContext()->ActiveIdSource==ImGuiInputSource_Mouse)ImGui::ClearActiveID();}
    DragLatched=false;
}
inline WORD BlockedFaces=0;inline uint32_t ControllerId=0;
inline void PointerModeTo(bool enabled,WORD held=0){
    if(PointerMode!=enabled)BlockedFaces|=held&(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_X);
    if(!enabled)CancelGrab();
    PointerMode=enabled;
}
inline void ControllerChanged(uint32_t id){
    if(ControllerId&&ControllerId!=id){if(PointerMode){BlockedFaces|=XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_X;StickClickArmed=false;StickWasDown=true;}PointerModeTo(false);}
    ControllerId=id;
}
inline ImGuiID NavigationCanvas=0;
inline int ItemBayFrame=-1;inline bool ItemBayMultiSelect=false;
inline ImVec2 LastPointer{-FLT_MAX,-FLT_MAX};inline double PointerUntil=0;
inline bool KeepShortcuts=false;
inline bool ShortcutActivated=false;
inline HelpyControllerDevice::Reading PointerReading;
inline bool TouchUsing=false,TouchTracking=false,TouchClickArmed=false,TouchMouseDown=false,TouchRightArmed=false,TouchRightMouseDown=false;
inline HelpyTouchpadTap TouchTap;
inline uint32_t TouchDevice=0;
inline float TouchX=0,TouchY=0;
inline auto DeviceRead=&HelpyControllerDevice::Read;
#if defined(HELPY_PREVIEW)
inline HelpyControllerDevice::Reading PreviewReading;
#endif
inline void Changed(){Refresh=true;NextXInputProbe=0;}
inline void Initialize(){if(Initialized)return;Initialized=true;Refresh=true;Library=LoadLibraryExW(L"xinput1_4.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(Library){GetState=reinterpret_cast<StateFn>(GetProcAddress(Library,"XInputGetState"));GetCaps=reinterpret_cast<CapsFn>(GetProcAddress(Library,"XInputGetCapabilities"));}}
inline XINPUT_GAMEPAD PhysicalPad{};
inline HelpyControllerBindings::Profile ActiveBindings=HelpyControllerBindings::Defaults();
inline void Feed(const XINPUT_GAMEPAD& physical){
    PhysicalPad=physical;
    static std::string_view family;const auto detected=std::string_view(HelpyControllerBindings::Family(ReportedGlyphs));
    if(family!=detected||!HelpyControllerDevice::Active(physical)){ActiveBindings=HelpyControllerBindings::Read(HelpyImGuiTheme::Current.controllerBindings,detected.data());family=detected;}
    const auto pad=HelpyControllerBindings::Apply(physical,ActiveBindings);
    if(PointerEnabled)BlockedFaces&=pad.wButtons;
    const auto motion=HelpyControllerPointer::Motion(physical.sThumbRX,physical.sThumbRY,ImGui::GetIO().DeltaTime,HelpyImGuiTheme::Current.controllerCursorSpeed);
    if(!PointerEnabled||(!HelpyImGuiTheme::Current.controllerCursor&&!HelpyImGuiTheme::Current.controllerTouchpad))PointerModeTo(false,pad.wButtons);
    else if((HelpyImGuiTheme::Current.controllerCursor&&(motion.X!=0||motion.Y!=0||(physical.wButtons&XINPUT_GAMEPAD_RIGHT_THUMB)))
        ||(HelpyImGuiTheme::Current.controllerTouchpad&&PointerReading.Connected&&PointerReading.Touch.Down))PointerModeTo(true,pad.wButtons);
    else if((pad.wButtons&~(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_X|XINPUT_GAMEPAD_RIGHT_THUMB|XINPUT_GAMEPAD_LEFT_THUMB))||pad.bLeftTrigger>30||pad.bRightTrigger>30)PointerModeTo(false,pad.wButtons);
    auto& io=ImGui::GetIO();io.BackendFlags|=ImGuiBackendFlags_HasGamepad;
    for(const auto& [key,button]:std::array<std::pair<ImGuiKey,WORD>,14>{{{ImGuiKey_GamepadStart,XINPUT_GAMEPAD_START},{ImGuiKey_GamepadBack,XINPUT_GAMEPAD_BACK},{ImGuiKey_GamepadFaceDown,XINPUT_GAMEPAD_A},{ImGuiKey_GamepadFaceRight,XINPUT_GAMEPAD_B},{ImGuiKey_GamepadFaceLeft,XINPUT_GAMEPAD_X},{ImGuiKey_GamepadFaceUp,XINPUT_GAMEPAD_Y},{ImGuiKey_GamepadDpadLeft,XINPUT_GAMEPAD_DPAD_LEFT},{ImGuiKey_GamepadDpadRight,XINPUT_GAMEPAD_DPAD_RIGHT},{ImGuiKey_GamepadDpadUp,XINPUT_GAMEPAD_DPAD_UP},{ImGuiKey_GamepadDpadDown,XINPUT_GAMEPAD_DPAD_DOWN},{ImGuiKey_GamepadL1,XINPUT_GAMEPAD_LEFT_SHOULDER},{ImGuiKey_GamepadR1,XINPUT_GAMEPAD_RIGHT_SHOULDER},{ImGuiKey_GamepadL3,XINPUT_GAMEPAD_LEFT_THUMB},{ImGuiKey_GamepadR3,XINPUT_GAMEPAD_RIGHT_THUMB}}}){
        const bool pointerButton=key==ImGuiKey_GamepadFaceDown||key==ImGuiKey_GamepadFaceLeft;
        io.AddKeyEvent(key,((pad.wButtons&button)!=0)&&!(pointerButton&&(PointerMode||(BlockedFaces&button)))&&!(PointerMode&&key==ImGuiKey_GamepadR3));
    }
    const auto analog=[&](ImGuiKey key,float value,float minimum,float maximum){const float amount=std::clamp((value-minimum)/(maximum-minimum),0.f,1.f);io.AddKeyAnalogEvent(key,amount>.1f,amount);};
    analog(ImGuiKey_GamepadL2,pad.bLeftTrigger,30,255);analog(ImGuiKey_GamepadR2,pad.bRightTrigger,30,255);
    // Helpy routes the left stick to the active scrollable panel or the map.
    for(const auto key:{ImGuiKey_GamepadRStickLeft,ImGuiKey_GamepadRStickRight,ImGuiKey_GamepadRStickUp,ImGuiKey_GamepadRStickDown})io.AddKeyEvent(key,false);
}
inline void Poll(int mode,const XINPUT_GAMEPAD* native=nullptr,bool pointerInteractive=true){
    PointerEnabled=mode!=1&&pointerInteractive;
    if(!PointerEnabled)PointerModeTo(false,HelpyControllerBindings::Apply(PhysicalPad,ActiveBindings).wButtons);
    PointerReading={};auto& io=ImGui::GetIO();if(mode==1){Feed({});io.ConfigFlags&=~ImGuiConfigFlags_NavEnableGamepad;io.BackendFlags&=~ImGuiBackendFlags_HasGamepad;return;}
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad;
    const auto mapped=DeviceRead(Refresh);
    if(mapped.Connected){ControllerChanged(mapped.Id);PointerReading=mapped;ReportedGlyphs=mapped.Glyphs;Feed(mapped.Pad);Refresh=false;return;}
    ControllerChanged(0);
    if(native){Refresh=false;ReportedGlyphs=mapped.Connected?mapped.Glyphs:HelpyControllerDevice::Xbox;Feed(*native);return;}Initialize();
    const auto now=GetTickCount64();
    if(Refresh||(!Connected&&now>=NextXInputProbe)){NextXInputProbe=now+250;Connected=0;if(GetCaps)for(unsigned i=0;i<4;++i){XINPUT_CAPABILITIES caps{};if(GetCaps(i,XINPUT_FLAG_GAMEPAD,&caps)==ERROR_SUCCESS)Connected|=1u<<i;}Refresh=false;for(int i=0;i<4;++i)if(Connected&(1u<<i)){Slot=i;break;}}
    XINPUT_STATE chosen{};bool found=false;static std::array<XINPUT_GAMEPAD,4> prior{};
    for(int i=0;i<4;++i)if(Connected&(1u<<i)){XINPUT_STATE state{};
        if(!GetState||GetState(i,&state)!=ERROR_SUCCESS){Connected&=~(1u<<i);continue;}
        if(i==Slot){chosen=state;found=true;}
        if(HelpyControllerDevice::Active(state.Gamepad)&&std::memcmp(&state.Gamepad,&prior[i],sizeof(state.Gamepad))!=0){chosen=state;found=true;Slot=i;}
        prior[i]=state.Gamepad;
    }
    if(found){ReportedGlyphs=HelpyControllerDevice::Xbox;Feed(chosen.Gamepad);}else{BlockedFaces|=HelpyControllerBindings::Apply(PhysicalPad,ActiveBindings).wButtons&(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_X);PointerModeTo(false);PointerEnabled=false;Feed({});io.BackendFlags&=~ImGuiBackendFlags_HasGamepad;}
}
inline std::optional<ImVec2> UpdatePointer(bool enabled,bool stickEnabled=false){
    auto& io=ImGui::GetIO();const auto& touch=PointerReading.Touch;
    enabled=enabled&&PointerReading.Connected;const bool tap=TouchTap.Poll(enabled,PointerReading.Id,touch,ImGui::GetTime());TouchUsing=enabled&&(touch.Down||touch.Clicked||tap);
    if(!enabled||TouchDevice!=PointerReading.Id){TouchTracking=false;TouchClickArmed=false;TouchRightArmed=false;}
    TouchDevice=PointerReading.Id;
    if(enabled&&!touch.Clicked)TouchClickArmed=true;
    if(enabled&&touch.Fingers<2)TouchRightArmed=true;
    if(!enabled&&!stickEnabled)PointerModeTo(false);
    const auto pad=HelpyControllerBindings::Apply(PhysicalPad,ActiveBindings);
    const bool right=(enabled&&touch.Fingers>=2&&TouchRightArmed)||(PointerMode&&(pad.wButtons&XINPUT_GAMEPAD_X)&&!(BlockedFaces&XINPUT_GAMEPAD_X));
    const auto delta=HelpyControllerPointer::Motion(PhysicalPad.sThumbRX,PhysicalPad.sThumbRY,io.DeltaTime,HelpyImGuiTheme::Current.controllerCursorSpeed);
    const bool pressed=(PhysicalPad.wButtons&XINPUT_GAMEPAD_RIGHT_THUMB)!=0;
    if(!stickEnabled){StickClickArmed=false;CancelGrab();}else if(!pressed)StickClickArmed=true;
    if(PointerMode&&stickEnabled&&pressed&&!StickWasDown&&StickClickArmed)DragLatched=!DragLatched;
    StickWasDown=pressed;
    StickUsing=stickEnabled&&(delta.X!=0||delta.Y!=0||pressed||DragLatched||(PointerMode&&(pad.wButtons&(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_X))));
    const bool down=tap||(enabled&&touch.Clicked&&TouchClickArmed&&touch.Fingers<2)||DragLatched||(PointerMode&&(pad.wButtons&XINPUT_GAMEPAD_A)&&!(BlockedFaces&XINPUT_GAMEPAD_A));
    if(TouchUsing)ReportedGlyphs=PointerReading.Glyphs;
    std::optional<ImVec2> result;
    if(enabled&&touch.Down&&touch.Fingers<2&&std::isfinite(touch.X)&&std::isfinite(touch.Y)&&io.DisplaySize.x>=1&&io.DisplaySize.y>=1){
        if(TouchTracking){
            const float dx=(touch.X-TouchX)*io.DisplaySize.x,dy=(touch.Y-TouchY)*io.DisplaySize.y;
            if(dx!=0||dy!=0){
                const auto origin=ImGui::IsMousePosValid(&io.MousePos)?io.MousePos:ImVec2(io.DisplaySize.x*.5f,io.DisplaySize.y*.5f);
                result=ImVec2(std::clamp(origin.x+dx,0.f,io.DisplaySize.x-1),std::clamp(origin.y+dy,0.f,io.DisplaySize.y-1));
                io.AddMousePosEvent(result->x,result->y);
            }
        }
        TouchX=touch.X;TouchY=touch.Y;TouchTracking=true;
    }else TouchTracking=false;
    if(stickEnabled&&(delta.X!=0||delta.Y!=0)&&io.DisplaySize.x>=1&&io.DisplaySize.y>=1){
        const auto origin=result.value_or(ImGui::IsMousePosValid(&io.MousePos)?io.MousePos:ImVec2(io.DisplaySize.x*.5f,io.DisplaySize.y*.5f));
        result=ImVec2(std::clamp(origin.x+delta.X,0.f,io.DisplaySize.x-1),std::clamp(origin.y+delta.Y,0.f,io.DisplaySize.y-1));
        io.AddMousePosEvent(result->x,result->y);
    }
    if(PointerMode&&!result&&!ImGui::IsMousePosValid(&io.MousePos)&&io.DisplaySize.x>=1&&io.DisplaySize.y>=1){result=ImVec2(io.DisplaySize.x*.5f,io.DisplaySize.y*.5f);io.AddMousePosEvent(result->x,result->y);}
    if(result){LastPointer=*result;PointerUntil=ImGui::GetTime()+.15;}
    else if(PointerMode&&(down||right)&&ImGui::IsMousePosValid(&io.MousePos)){LastPointer=io.MousePos;PointerUntil=ImGui::GetTime()+.15;}
    PointerUsing=TouchUsing||StickUsing||(ImGui::GetTime()<PointerUntil&&std::abs(io.MousePos.x-LastPointer.x)<1&&std::abs(io.MousePos.y-LastPointer.y)<1);
    if(right!=TouchRightMouseDown){io.AddMouseButtonEvent(ImGuiMouseButton_Right,right);TouchRightMouseDown=right;}
    if(down!=TouchMouseDown){io.AddMouseButtonEvent(ImGuiMouseButton_Left,down);TouchMouseDown=down;}
    return result;
}
inline void ScrollPanel() {
    auto& context=*ImGui::GetCurrentContext();
    if(NavigationCanvas&&context.NavId==NavigationCanvas&&!PointerMode)return;
    const auto delta=HelpyControllerPointer::Motion(PhysicalPad.sThumbLX,PhysicalPad.sThumbLY,ImGui::GetIO().DeltaTime,850.f);
    if(delta.X==0&&delta.Y==0)return;
    auto* window=PointerMode?context.HoveredWindow:context.NavWindow;
    const bool horizontal=std::abs(delta.X)>std::abs(delta.Y);
    for(;window;window=(window->Flags&ImGuiWindowFlags_ChildWindow)?window->ParentWindow:nullptr) {
        if(window->Flags&(ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoMouseInputs))continue;
        if(horizontal&&window->ScrollMax.x>0) {
            ImGui::SetScrollX(window,std::clamp(window->Scroll.x+delta.X,0.f,window->ScrollMax.x));
            return;
        }
        if(!horizontal&&window->ScrollMax.y>0) {
            ImGui::SetScrollY(window,std::clamp(window->Scroll.y+delta.Y,0.f,window->ScrollMax.y));
            return;
        }
    }
}
inline void Observe(int mode){
    bool pad=false,keys=false;static std::array<float,ImGuiKey_GamepadRStickDown-ImGuiKey_GamepadStart+1> previous{};
    for(int key=ImGuiKey_GamepadStart;key<=ImGuiKey_GamepadRStickDown;++key){const auto value=ImGui::GetKeyData(ImGuiKey(key))->AnalogValue;auto& prior=previous[key-ImGuiKey_GamepadStart];pad|=ImGui::IsKeyPressed(ImGuiKey(key),false)||(value>.1f&&std::abs(value-prior)>.08f);prior=value;}
    static SHORT priorX=0,priorY=0;
    const bool leftActive=std::abs(int(PhysicalPad.sThumbLX))>8847||std::abs(int(PhysicalPad.sThumbLY))>8847;
    const bool wasActive=std::abs(int(priorX))>8847||std::abs(int(priorY))>8847;
    pad|=leftActive&&(!wasActive||std::abs(int(PhysicalPad.sThumbLX)-priorX)>2500||std::abs(int(PhysicalPad.sThumbLY)-priorY)>2500);
    priorX=PhysicalPad.sThumbLX;priorY=PhysicalPad.sThumbLY;
    for(int key=ImGuiKey_Tab;key<=ImGuiKey_KeypadEqual;++key)keys|=ImGui::IsKeyPressed(ImGuiKey(key),false);
    const auto& io=ImGui::GetIO();
    const bool generatedPointer=TouchUsing||StickUsing||((PointerMode||ImGui::GetTime()<PointerUntil)&&std::abs(io.MousePos.x-LastPointer.x)<1&&std::abs(io.MousePos.y-LastPointer.y)<1);
    keys|=!generatedPointer&&(io.MouseDelta.x!=0||io.MouseDelta.y!=0||io.MouseWheel!=0||ImGui::IsMouseClicked(0)||ImGui::IsMouseClicked(1));
    if(keys&&!StickUsing&&!TouchUsing)PointerModeTo(false,HelpyControllerBindings::Apply(PhysicalPad,ActiveBindings).wButtons);
    PointerUsing=TouchUsing||StickUsing||PointerMode;
    const bool prior=Selection.Controller;const bool shortcut=std::exchange(ShortcutActivated,false);Selection.Update(HelpyInputMode::Mode(mode),pad||shortcut||TouchUsing||StickUsing||PointerMode,keys&&!shortcut&&!TouchUsing);JustActivated=Selection.Controller&&!prior;if(PointerMode){ImGui::SetNavCursorVisible(false);ImGui::GetCurrentContext()->NavHighlightItemUnderNav=false;}HelpyImGuiTheme::ControllerNavigation=Selection.Controller;if(Selection.Controller)ScrollPanel();if(!Selection.Controller){HelpyImGuiTheme::EnterNextSection=false;HelpyControllerPanes::Pending=0;}
}
// Enter a pane without opening the keyboard or activating its first field.
inline void FocusNext(){ImGui::NavClearPreferredPosForAxis(ImGuiAxis_X);ImGui::NavClearPreferredPosForAxis(ImGuiAxis_Y);ImGui::SetKeyboardFocusHere();ImGui::GetCurrentContext()->NavMoveFlags&=~ImGuiNavMoveFlags_Activate;}
inline bool NavigationSourceItem(){
    const auto& context=*ImGui::GetCurrentContext();const auto* window=ImGui::GetCurrentWindow();
    if(context.LastItemData.ItemFlags&ImGuiItemFlags_Disabled)return false;
    return ImGui::IsItemFocused()||(context.NavMoveSubmitted&&!context.NavId&&context.NavWindow==window&&context.LastItemData.ID&&window->NavLastIds[context.NavLayer]==context.LastItemData.ID);
}
inline void EnterSection(){HelpyImGuiTheme::EnterNextSection=true;}
inline bool InSection(){const auto* window=ImGui::GetCurrentContext()->NavWindow;return window&&window->RootWindowForNav!=window->RootWindow&&!(window->RootWindowForNav->Flags&ImGuiWindowFlags_Popup);}
inline void ResolvePaneNavigation(){
    auto& context=*ImGui::GetCurrentContext();
    if(Selection.Controller&&HelpyControllerPanes::Move())return;
    if(!Selection.Controller||!context.NavWindow||!context.NavMoveScoringItems||(context.NavMoveFlags&(ImGuiNavMoveFlags_IsTabbing|ImGuiNavMoveFlags_AlsoScoreVisibleSet)))return;
    auto& local=context.NavMoveResultLocal;const auto& other=HelpyImGuiTheme::PaneNavigationSource==context.NavWindow?HelpyImGuiTheme::PaneNavigationBest:context.NavMoveResultOther;
    // ImGui compares a direct flattened child; Helpy's content can be several panes deep.
    if(other.ID&&other.Window->RootWindowForNav==context.NavWindow->RootWindowForNav&&ImGui::IsWindowChildOf(other.Window,context.NavWindow,false)&&
        HelpyImGuiTheme::Nearer(other,local))local=other;
    if(context.NavInputSource==ImGuiInputSource_Gamepad&&context.NavMoveDir==ImGuiDir_Down&&local.ID){
        for(auto* pane:HelpyControllerPanes::Panes){
            if(pane->ParentWindow!=local.Window||pane->ChildId!=local.ID||!pane->DC.NavLayersActiveMaskNext)continue;
            HelpyControllerPanes::Pending=pane->ID;
            HelpyControllerPanes::PendingFrame=context.FrameCount+1;
            break;
        }
    }
}
inline bool DefaultFocus(bool arriving=false){
    auto& context=*ImGui::GetCurrentContext();auto* window=ImGui::GetCurrentWindow();
    if(PointerMode||!Selection.Controller||!context.LastItemData.ID||context.ActiveId||context.DragDropActive||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId))return false;
    // Scrolling navigation can temporarily clear NavId; do not pull it back to an outer header.
    if(context.NavWindow&&(context.NavMoveSubmitted||context.NavInitRequestFromMove))return false;
    const bool missing=!context.NavWindow||!context.NavId||(!context.NavWindow->Active&&!context.NavWindow->WasActive);
    if(!missing&&(!arriving||context.NavWindow->RootWindow!=window->RootWindow))return false;
    // NoFocusOnAppearing keeps mouse-mode windows passive; controller entry needs an explicit target.
    ImGui::FocusWindow(window,ImGuiFocusRequestFlags_UnlessBelowModal);if(context.NavWindow!=window)return false;
    ImGui::SetNavWindow(window);context.NavInitResult.ID=0;
    ImGui::SetFocusID(context.LastItemData.ID,window);context.NavInputSource=ImGuiInputSource_Gamepad;ImGui::SetNavCursorVisible(true);return true;
}
inline ImU32 HintColor(bool active){return ImGui::GetColorU32(active?HelpyImGuiTheme::Current.accent:HelpyImGuiTheme::Current.textDisabled);}
inline void DrawCenteredGlyphText(ImDrawList* draw,ImVec2 center,ImU32 color,const char* text){
    // The font line box includes space below the visible glyphs.
    auto* font=ImGui::GetFontBaked();
    ImVec2 min{FLT_MAX,FLT_MAX},max{-FLT_MAX,-FLT_MAX};
    float advance=0;
    for(const char* at=text;*at;){
        unsigned int codepoint=0;
        at+=ImTextCharFromUtf8(&codepoint,at,nullptr);
        const auto* glyph=font->FindGlyph(ImWchar(codepoint));
        if(glyph->Visible){
            min.x=std::min(min.x,advance+glyph->X0);min.y=std::min(min.y,glyph->Y0);
            max.x=std::max(max.x,advance+glyph->X1);max.y=std::max(max.y,glyph->Y1);
        }
        advance+=glyph->AdvanceX;
    }
    if(min.x<=max.x)draw->AddText({std::round(center.x-(min.x+max.x)*.5f),std::round(center.y-(min.y+max.y)*.5f)},color,text);
}
inline void DrawPhysicalGlyph(ImVec2 center,float radius,uint32_t button,int style,bool active){
    auto* draw=ImGui::GetWindowDrawList();const auto color=HintColor(active);const auto labels=HelpyControllerDevice::Resolve(style,ReportedGlyphs);
    int face=-1;for(int i=0;i<4;++i)if(button==HelpyControllerBindings::Actions[i].Button)face=i;
    if(face>=0){
        draw->AddCircle(center,radius,color,24,1.5f);using B=HelpyControllerDevice::Button;const auto glyph=labels[size_t(face)];const float r=radius*.5f;
        if(glyph==B::Cross){draw->AddLine({center.x-r,center.y-r},{center.x+r,center.y+r},color,1.5f);draw->AddLine({center.x-r,center.y+r},{center.x+r,center.y-r},color,1.5f);}
        else if(glyph==B::Circle)draw->AddCircle(center,r,color,24,1.5f);
        else if(glyph==B::Square)draw->AddRect({center.x-r,center.y-r},{center.x+r,center.y+r},color,0,0,1.5f);
        else if(glyph==B::Triangle)draw->AddTriangle({center.x,center.y-r},{center.x+r,center.y+r},{center.x-r,center.y+r},color,1.5f);
        else{const char* text=glyph==B::A?"A":glyph==B::B?"B":glyph==B::X?"X":glyph==B::Y?"Y":glyph==B::South?"S":glyph==B::East?"E":glyph==B::West?"W":"N";DrawCenteredGlyphText(draw,center,color,text);}return;
    }
    if(button==InputBindings::Touchpad){draw->AddRect({center.x-radius,center.y-radius*.65f},{center.x+radius,center.y+radius*.65f},color,3,0,1.5f);draw->AddCircleFilled({center.x-radius*.35f,center.y},1.5f,color);draw->AddCircleFilled({center.x+radius*.35f,center.y},1.5f,color);return;}
    const bool ps=labels==HelpyControllerDevice::PlayStation,nintendo=labels==HelpyControllerDevice::Nintendo;
    const char* text=button==XINPUT_GAMEPAD_LEFT_SHOULDER?(ps?"L1":nintendo?"L":"LB"):button==XINPUT_GAMEPAD_RIGHT_SHOULDER?(ps?"R1":nintendo?"R":"RB"):button==InputBindings::LeftTrigger?(ps?"L2":nintendo?"ZL":"LT"):button==InputBindings::RightTrigger?(ps?"R2":nintendo?"ZR":"RT"):button==InputBindings::Guide?(ps?"PS":nintendo?"H":"X"):button==XINPUT_GAMEPAD_BACK?(ps?"SH":"V"):button==XINPUT_GAMEPAD_START?(ps?"OP":"M"):button==XINPUT_GAMEPAD_DPAD_LEFT?"<":button==XINPUT_GAMEPAD_DPAD_RIGHT?">":button==XINPUT_GAMEPAD_DPAD_UP?"^":button==XINPUT_GAMEPAD_DPAD_DOWN?"v":button==XINPUT_GAMEPAD_LEFT_THUMB?(ps?"L3":"LS"):button==XINPUT_GAMEPAD_RIGHT_THUMB?(ps?"R3":"RS"):button==0?"+":"R";
    const float halfWidth=std::max(radius+2,ImGui::CalcTextSize(text).x*.5f+2);
    draw->AddRect({center.x-halfWidth,center.y-radius},{center.x+halfWidth,center.y+radius},color,4,0,1.5f);
    DrawCenteredGlyphText(draw,center,color,text);
}
inline void DrawGlyph(ImVec2 center,float radius,size_t button,int style,bool physical=false){
    constexpr ImGuiKey keys[]{ImGuiKey_GamepadFaceDown,ImGuiKey_GamepadFaceRight,ImGuiKey_GamepadFaceLeft,ImGuiKey_GamepadFaceUp};
    DrawPhysicalGlyph(center,radius,physical?HelpyControllerBindings::Actions[button].Button:ActiveBindings[button],style,!physical&&ImGui::IsKeyDown(keys[button]));
}
inline bool PhysicalDown(uint32_t button){
    if(button==InputBindings::Guide)return PointerReading.Guide;
    if(button==InputBindings::Touchpad)return TouchUsing;
    if(button==InputBindings::LeftTrigger)return PhysicalPad.bLeftTrigger>30;
    if(button==InputBindings::RightTrigger)return PhysicalPad.bRightTrigger>30;
    if(button==XINPUT_GAMEPAD_LEFT_THUMB)return (PhysicalPad.wButtons&button)||std::abs(int(PhysicalPad.sThumbLX))>8847||std::abs(int(PhysicalPad.sThumbLY))>8847;
    if(button==XINPUT_GAMEPAD_RIGHT_THUMB)return (PhysicalPad.wButtons&button)||std::abs(int(PhysicalPad.sThumbRX))>8689||std::abs(int(PhysicalPad.sThumbRY))>8689;
    return button&&(PhysicalPad.wButtons&button)!=0;
}
enum class MouseHint { None, Left, Right, Wheel };
struct Hint {
    std::string Label;
    std::vector<uint32_t> Buttons;
    bool Active=false;
    std::string Key;
    MouseHint Mouse=MouseHint::None;
};
inline std::vector<Hint> HintItems(std::string_view shortcut,bool welcome,bool itemBay){
    const bool map=!PointerMode&&NavigationCanvas&&ImGui::GetCurrentContext()->NavId==NavigationCanvas;
    std::vector<Hint> hints;
    const auto face=[&](const char* label,size_t index){hints.push_back({label,{ActiveBindings[index]},PhysicalDown(ActiveBindings[index])});};
    face(welcome?"Enter":PointerMode?"Click / drag":"Select",0);
    if(!welcome){
        face("Back",1);
        face(PointerMode?"Right-click":"Actions",2);
        if(map||itemBay)face(map?"Zoom out":"Search",3);
        hints.push_back({map?"Pan / zoom in":itemBay?"Scroll / multi-select":"Scroll",
            {XINPUT_GAMEPAD_LEFT_THUMB},PhysicalDown(XINPUT_GAMEPAD_LEFT_THUMB)||(itemBay&&ItemBayMultiSelect)});
        hints.push_back({"Navigate",{0},ImGui::IsKeyDown(ImGuiKey_GamepadDpadLeft)||ImGui::IsKeyDown(ImGuiKey_GamepadDpadRight)||ImGui::IsKeyDown(ImGuiKey_GamepadDpadUp)||ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown)});
        hints.push_back({"Panels",{ActiveBindings[4],ActiveBindings[5]},ImGui::IsKeyDown(ImGuiKey_GamepadL1)||ImGui::IsKeyDown(ImGuiKey_GamepadR1)});
        hints.push_back({"Sections",{ActiveBindings[6],ActiveBindings[7]},ImGui::IsKeyDown(ImGuiKey_GamepadL2)||ImGui::IsKeyDown(ImGuiKey_GamepadR2)});
        if(HelpyImGuiTheme::Current.controllerCursor)hints.push_back({DragLatched?"Drop held item":"Pointer / grab",{XINPUT_GAMEPAD_RIGHT_THUMB},StickUsing||DragLatched});
        if(itemBay)hints.push_back({"Send to cart",{XINPUT_GAMEPAD_START},PhysicalDown(XINPUT_GAMEPAD_START)});
    }
    const auto mask=InputBindings::ParsePad(shortcut);if(mask){Hint hint{"Helpy",{},false};for(const auto& token:InputBindings::PadButtons)if(mask&token.Bit)hint.Buttons.push_back(token.Bit);if(hint.Buttons.size()>3)hint.Buttons.resize(3);hint.Active=std::any_of(hint.Buttons.begin(),hint.Buttons.end(),[](uint32_t button){return PhysicalDown(button);});hints.push_back(std::move(hint));}
    if(!welcome&&HelpyImGuiTheme::Current.controllerTouchpad&&ReportedGlyphs==HelpyControllerDevice::PlayStation)
        hints.push_back({"Touch pointer",{InputBindings::Touchpad},TouchUsing});
    return hints;
}
inline std::vector<Hint> DesktopHints(std::string_view shortcut,bool welcome,bool itemBay,bool map,bool mapActions){
    using Mouse=MouseHint;
    std::vector<Hint> hints;
    const auto mouse=[&](const char* label,Mouse glyph,bool active,std::string key={}){
        hints.push_back({label,{},active,std::move(key),glyph});
    };
    const bool left=ImGui::IsMouseDown(ImGuiMouseButton_Left),right=ImGui::IsMouseDown(ImGuiMouseButton_Right);
    mouse(welcome?"Enter":map?"Drag to pan":"Select",Mouse::Left,left);
    if(map){
        mouse("Zoom",Mouse::Wheel,ImGui::GetIO().MouseWheel!=0);
        if(mapActions)mouse("Map actions",Mouse::Right,right);
    }else if(itemBay){
        mouse("Drag to cart",Mouse::Left,ImGui::GetDragDropPayload()!=nullptr);
        mouse("Multi-select",Mouse::Left,left&&ImGui::GetIO().KeyCtrl,"Ctrl");
        mouse("Item actions",Mouse::Right,right);
    }
    hints.push_back({"Move focus",{},ImGui::IsKeyDown(ImGuiKey_Tab),"Tab"});
    if(!shortcut.empty()&&shortcut!="None")hints.push_back({"Helpy",{},false,std::string(shortcut)});
    return hints;
}
inline float KeyHintWidth(const Hint& hint){
    const float frame=ImGui::GetFrameHeight();
    return hint.Key.empty()?0.f:std::ceil(std::max(frame,ImGui::CalcTextSize(hint.Key.c_str()).x+frame*.5f));
}
inline float HintGlyphWidth(const Hint& hint){
    return KeyHintWidth(hint)+ImGui::GetFrameHeight()*float(hint.Buttons.size()+(hint.Mouse!=MouseHint::None));
}
inline float HintWidth(const Hint& hint){return HintGlyphWidth(hint)+ImGui::CalcTextSize(hint.Label.c_str()).x+ImGui::GetStyle().ItemSpacing.x*2;}
inline void DrawDesktopGlyph(const Hint& hint,ImVec2 at,float size){
    auto* draw=ImGui::GetWindowDrawList();
    const auto color=HintColor(hint.Active);
    const float keyWidth=KeyHintWidth(hint);
    if(keyWidth){
        const float left=std::round(at.x),top=std::round(at.y+size*.1f),height=std::round(size*.8f);
        draw->AddRect({left,top},{left+keyWidth,top+height},color,3.f);
        DrawCenteredGlyphText(draw,{left+keyWidth*.5f,top+height*.5f},color,hint.Key.c_str());
        at.x+=keyWidth;
    }
    if(hint.Mouse==MouseHint::None)return;
    const ImVec2 min{at.x+size*.19f,at.y+size*.08f},max{at.x+size*.81f,at.y+size*.92f};
    const float middle=at.x+size*.5f,split=at.y+size*.45f;
    draw->AddRect(min,max,color,size*.23f);
    draw->AddLine({min.x,split},{max.x,split},color);
    draw->AddLine({middle,min.y},{middle,split},color);
    if(hint.Mouse==MouseHint::Wheel)draw->AddRectFilled({middle-size*.06f,min.y+size*.08f},{middle+size*.06f,split-size*.05f},color,2.f);
    else{
        const float x=hint.Mouse==MouseHint::Left?min.x:middle;
        draw->AddRectFilled({x+size*.07f,min.y+size*.12f},{x+size*.24f,split-size*.06f},color,2.f);
    }
}
inline float HintsHeight(float width,const std::vector<Hint>& hints){
    float used=0;int rows=1;for(const auto& hint:hints){const auto size=HintWidth(hint);if(used&&used+size>width){++rows;used=0;}used+=size;}return rows*ImGui::GetFrameHeightWithSpacing();
}
inline void Hints(int style,const std::vector<Hint>& hints){
    const float width=ImGui::GetContentRegionAvail().x;float used=0;
    for(const auto& hint:hints){
        const float cell=HintWidth(hint);if(used&&used+cell>width){used=0;}else if(used)ImGui::SameLine(0,ImGui::GetStyle().ItemSpacing.x);
        const auto at=ImGui::GetCursorScreenPos();const float size=ImGui::GetFrameHeight();
        for(size_t i=0;i<hint.Buttons.size();++i)DrawPhysicalGlyph({at.x+size*(float(i)+.5f),at.y+size*.5f},size*.31f,hint.Buttons[i],style,hint.Buttons.size()==1?hint.Active:PhysicalDown(hint.Buttons[i]));
        if(hint.Buttons.empty())DrawDesktopGlyph(hint,at,size);
        ImGui::Dummy({HintGlyphWidth(hint),size});ImGui::SameLine(0,ImGui::GetStyle().ItemSpacing.x);ImGui::AlignTextToFramePadding();ImGui::TextColored(hint.Active?HelpyImGuiTheme::Current.accent:HelpyImGuiTheme::Current.textDisabled,"%s",hint.Label.c_str());used+=cell;
    }
}
inline bool DrawPointer(){
    if(!PointerMode||!Selection.Controller||!ImGui::IsMousePosValid())return false;
    auto& io=ImGui::GetIO();io.MouseDrawCursor=false;ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    auto* draw=ImGui::GetForegroundDrawList();const auto center=io.MousePos;
    const float radius=std::clamp(ImGui::GetFontSize()*.45f,7.f,14.f);const auto gold=ImGui::GetColorU32(HelpyImGuiTheme::Current.accent);
    draw->AddCircle(center,radius,IM_COL32(0,0,0,230),32,4.f);
    draw->AddCircleFilled(center,radius-1,ImGui::GetColorU32(gold,.12f),32);
    draw->AddCircle(center,radius,gold,32,1.75f);draw->AddCircleFilled(center,1.5f,gold,12);
    if(io.MouseDown[0]||io.MouseDown[1])draw->AddCircle(center,radius-3,gold,24,1.5f);
    return true;
}
inline void Suspend(){BlockedFaces|=HelpyControllerBindings::Apply(PhysicalPad,ActiveBindings).wButtons&(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_X);PointerModeTo(false);PointerEnabled=false;ControllerId=0;
    if(ImGui::GetCurrentContext()){auto& io=ImGui::GetIO();if(TouchMouseDown)io.AddMouseButtonEvent(ImGuiMouseButton_Left,false);if(TouchRightMouseDown)io.AddMouseButtonEvent(ImGuiMouseButton_Right,false);}TouchMouseDown=TouchRightMouseDown=false;
    PointerReading={};TouchTap={};TouchTracking=false;TouchClickArmed=false;TouchRightArmed=false;TouchUsing=false;StickUsing=PointerUsing=StickClickArmed=StickWasDown=false;PointerUntil=0;PhysicalPad={};HelpyControllerPanes::Pending=0;NextXInputProbe=0;Connected=0;}
inline void Shutdown(){KeepShortcuts=false;ShortcutActivated=false;Suspend();HelpyControllerDevice::Shutdown();TouchMouseDown=TouchRightMouseDown=false;if(Library)FreeLibrary(Library);Library=nullptr;GetState=nullptr;GetCaps=nullptr;Initialized=false;}
}
