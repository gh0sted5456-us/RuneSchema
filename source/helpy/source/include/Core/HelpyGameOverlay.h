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
    bool Focused()const{return m_topmost;}
    // Release only a game-local clamp after Windows has handed Helpy the foreground.
    static bool GameCursorClip(const RECT& clip,const RECT& game,const RECT& desktop){
        return !EqualRect(&clip,&desktop)&&clip.right>clip.left&&clip.bottom>clip.top
            &&clip.left>=game.left&&clip.top>=game.top&&clip.right<=game.right&&clip.bottom<=game.bottom;
    }
    static bool Focus(HWND window,HWND game,bool overlay){
        if(!IsWindowVisible(window)||IsIconic(window))return false;
        if(GetForegroundWindow()!=window)SetForegroundWindow(window);
        if(GetForegroundWindow()!=window)return false;
        SetFocus(window);
        if(!overlay||!game||!IsWindow(game))return true;
        RECT area{},clip{};POINT origin{};
        if(GetClientRect(game,&area)&&ClientToScreen(game,&origin)&&GetClipCursor(&clip)){
            OffsetRect(&area,origin.x,origin.y);
            const RECT desktop{GetSystemMetrics(SM_XVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN),
                GetSystemMetrics(SM_XVIRTUALSCREEN)+GetSystemMetrics(SM_CXVIRTUALSCREEN),
                GetSystemMetrics(SM_YVIRTUALSCREEN)+GetSystemMetrics(SM_CYVIRTUALSCREEN)};
            if(GameCursorClip(clip,area,desktop))ClipCursor(nullptr);
        }
        POINT pointer{};RECT panel{};
        if(GetCursorPos(&pointer)&&GetWindowRect(window,&panel)&&!PtInRect(&panel,pointer))
            SetCursorPos(panel.left+(panel.right-panel.left)/2,panel.top+(panel.bottom-panel.top)/2);
        SetCursor(LoadCursorW(nullptr,IDC_ARROW));return true;
    }
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
        if(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_LAYERED)SetLayeredWindowAttributes(window,0,m_restoreAlpha,LWA_ALPHA);
        m_active=false;m_topmost=false;m_next={};
    }
    void Suspend(HWND window){
        if(m_topmost){Companion(window,false);m_topmost=false;}
    }
    static RECT Bounds(const RECT& game,float scale){
        scale=std::clamp(scale,.45f,.98f);const LONG width=game.right-game.left,height=game.bottom-game.top;
        const LONG panelWidth=std::min(width,std::max(std::min(760L,width),LONG(width*scale)));
        const LONG panelHeight=std::min(height,std::max(std::min(560L,height),LONG(height*scale)));
        const LONG x=game.left+(width-panelWidth)/2,y=game.top+(height-panelHeight)/2;return {x,y,x+panelWidth,y+panelHeight};
    }
    void Update(HWND window,HWND game,bool enabled,bool visible,bool focused,float opacity,float scale=.9f){
        if(!enabled){Restore(window);return;}
        if(!visible){Suspend(window);return;}
        if(!game||!IsWindow(game)||IsIconic(game)){Restore(window);return;}
        if(!m_active){
            GetWindowRect(window,&m_restore);COLORREF color=0;DWORD flags=0;m_restoreAlpha=255;
            GetLayeredWindowAttributes(window,&color,&m_restoreAlpha,&flags);
            if(!(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_NOREDIRECTIONBITMAP))SetWindowLongPtrW(window,GWL_EXSTYLE,GetWindowLongPtrW(window,GWL_EXSTYLE)|WS_EX_LAYERED);
            m_active=true;m_bounds={};m_alpha=m_restoreAlpha;m_next={};
        }
        const auto alpha=static_cast<BYTE>(std::clamp(opacity,.60f,1.f)*255);
        if(m_alpha!=alpha){if(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_LAYERED)SetLayeredWindowAttributes(window,0,alpha,LWA_ALPHA);m_alpha=alpha;}
        if(m_topmost!=focused){Companion(window,focused);m_topmost=focused;}
        const auto now=std::chrono::steady_clock::now();if(now<m_next)return;
        m_next=now+std::chrono::milliseconds(250);
        RECT area{};POINT origin{};if(!GetClientRect(game,&area)||!ClientToScreen(game,&origin))return;
        const int clientWidth=static_cast<int>(area.right),clientHeight=static_cast<int>(area.bottom);
        if(clientWidth<160||clientHeight<120)return;
        const auto bounds=Bounds({origin.x,origin.y,origin.x+clientWidth,origin.y+clientHeight},scale);
        if(!EqualRect(&bounds,&m_bounds)){
            SetWindowPos(window,nullptr,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOZORDER|SWP_NOACTIVATE);m_bounds=bounds;
        }
    }
};
}
