#pragma once
#include <windows.h>
#include <algorithm>
#include <chrono>

namespace PS {
// Reuses Helpy's own window and renderer; never hooks the game's presentation.
class HelpyGameOverlay {
    bool m_active=false,m_topmost=false;
    RECT m_restore{},m_bounds{};
    BYTE m_restoreAlpha=255,m_alpha=255;
    std::chrono::steady_clock::time_point m_next{};
public:
    bool Active()const{return m_active;}
    static void Companion(HWND window,bool overlayFocused){
        if(!window)return;
        const bool wanted=overlayFocused&&IsWindowVisible(window);
        if(bool(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOPMOST)==wanted)return;
        SetWindowPos(window,wanted?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        const auto foreground=GetForegroundWindow();
        if(!wanted&&foreground&&foreground!=window&&!(GetWindowLongPtrW(foreground,GWL_EXSTYLE)&WS_EX_TOPMOST))
            SetWindowPos(window,foreground,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    }
    void Restore(HWND window){
        if(!m_active)return;
        SetWindowPos(window,HWND_NOTOPMOST,m_restore.left,m_restore.top,m_restore.right-m_restore.left,
            m_restore.bottom-m_restore.top,SWP_NOACTIVATE);
        SetLayeredWindowAttributes(window,0,m_restoreAlpha,LWA_ALPHA);
        m_active=false;m_topmost=false;m_next={};
    }
    void Update(HWND window,HWND game,bool enabled,bool visible,bool focused,float opacity){
        if(!enabled||!visible||!game||!IsWindow(game)||IsIconic(game)){Restore(window);return;}
        if(!m_active){
            GetWindowRect(window,&m_restore);COLORREF color=0;DWORD flags=0;m_restoreAlpha=255;
            GetLayeredWindowAttributes(window,&color,&m_restoreAlpha,&flags);
            SetWindowLongPtrW(window,GWL_EXSTYLE,GetWindowLongPtrW(window,GWL_EXSTYLE)|WS_EX_LAYERED);
            m_active=true;m_bounds={};m_alpha=m_restoreAlpha;m_next={};
        }
        const auto alpha=static_cast<BYTE>(std::clamp(opacity,.60f,1.f)*255);
        if(m_alpha!=alpha){SetLayeredWindowAttributes(window,0,alpha,LWA_ALPHA);m_alpha=alpha;}
        if(m_topmost!=focused){Companion(window,focused);m_topmost=focused;}
        const auto now=std::chrono::steady_clock::now();if(now<m_next)return;
        m_next=now+std::chrono::milliseconds(250);
        RECT area{};POINT origin{};if(!GetClientRect(game,&area)||!ClientToScreen(game,&origin))return;
        const int clientWidth=static_cast<int>(area.right),clientHeight=static_cast<int>(area.bottom);
        if(clientWidth<160||clientHeight<120)return;
        const int width=std::min(1440,std::max(1,clientWidth-std::max(16,clientWidth/20)));
        const int height=std::min(1000,std::max(1,clientHeight-std::max(16,clientHeight/16)));
        const RECT bounds{origin.x+(clientWidth-width)/2,origin.y+(clientHeight-height)/2,
            origin.x+(clientWidth+width)/2,origin.y+(clientHeight+height)/2};
        if(!EqualRect(&bounds,&m_bounds)){
            SetWindowPos(window,nullptr,bounds.left,bounds.top,width,height,SWP_NOZORDER|SWP_NOACTIVATE);m_bounds=bounds;
        }
    }
};
}
