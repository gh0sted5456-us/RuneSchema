#pragma once
#include "HelpyOverlayFrames.h"
#include <imgui.h>
#include <xinput.h>

extern ImGuiKey ImGui_ImplWin32_KeyEventToImGuiKey(WPARAM,LPARAM);
namespace PS::HelpyOverlayInput {
inline void Feed(HelpyOverlayFrames::Page& page,HWND game){
    auto& io=ImGui::GetIO();io.AddFocusEvent(true);HelpyOverlayFrames::Input input;
    RECT client{};GetClientRect(game,&client);const auto width=HelpyOverlayFrames::Read(page.ViewportWidth),height=HelpyOverlayFrames::Read(page.ViewportHeight);
    const float sx=width>0&&client.right>0?float(width)/client.right:1.f,sy=height>0&&client.bottom>0?float(height)/client.bottom:1.f;
    const auto position=[&](float x,float y){io.AddMousePosEvent(x*sx-page.Left,y*sy-page.Top);};
    while(HelpyOverlayFrames::Pop(page,input)){
        using namespace HelpyOverlayFrames;
        switch(input.Kind){
        case InputKind::Key:{
            io.AddKeyEvent(ImGuiMod_Ctrl,(input.X&1)!=0);io.AddKeyEvent(ImGuiMod_Shift,(input.X&2)!=0);io.AddKeyEvent(ImGuiMod_Alt,(input.X&4)!=0);io.AddKeyEvent(ImGuiMod_Super,(input.X&8)!=0);
            const auto key=ImGui_ImplWin32_KeyEventToImGuiKey(input.Code,LPARAM(input.Flags));if(key!=ImGuiKey_None)io.AddKeyEvent(key,(input.Flags&(1u<<31))==0);break;
        }
        case InputKind::Character:if(input.Code<=0xffff)io.AddInputCharacterUTF16(ImWchar16(input.Code));break;
        case InputKind::Mouse:position(float(input.X),float(input.Y));break;
        case InputKind::Wheel:io.AddMouseWheelEvent(input.Code==WM_MOUSEHWHEEL?-input.Y/120.f:0,input.Code==WM_MOUSEWHEEL?input.Y/120.f:0);break;
        case InputKind::Button:{int button=-1;bool down=false;switch(input.Code){case WM_LBUTTONDOWN:down=true;[[fallthrough]];case WM_LBUTTONUP:button=0;break;case WM_RBUTTONDOWN:down=true;[[fallthrough]];case WM_RBUTTONUP:button=1;break;case WM_MBUTTONDOWN:down=true;[[fallthrough]];case WM_MBUTTONUP:button=2;break;case WM_XBUTTONDOWN:down=true;[[fallthrough]];case WM_XBUTTONUP:button=HIWORD(input.Flags)==XBUTTON1?3:4;break;}if(button>=0){position(float(input.X),float(input.Y));io.AddMouseButtonEvent(button,down);}break;}
        }
    }
    POINT cursor{};if(GetCursorPos(&cursor)&&ScreenToClient(game,&cursor))position(float(cursor.x),float(cursor.y));
}
inline XINPUT_GAMEPAD Pad(HelpyOverlayFrames::Page& page){
    XINPUT_GAMEPAD pad{};const auto before=HelpyOverlayFrames::Read(page.PadSequence);if(before&1)return pad;
    std::memcpy(&pad,page.Pad.data(),sizeof(pad));MemoryBarrier();if(before!=HelpyOverlayFrames::Read(page.PadSequence))pad={};return pad;
}
}
