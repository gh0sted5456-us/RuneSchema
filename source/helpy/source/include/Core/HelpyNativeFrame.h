#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <algorithm>

namespace PS::HelpyNativeFrame {
struct Metrics {
    UINT Dpi=96;
    int Height=0,Border=0,Icon=0;
    RECT Controls{};
    bool Maximized=false;
    void Update(HWND window,UINT dpi){
        Dpi=dpi?dpi:96;
        Border=GetSystemMetricsForDpi(SM_CYSIZEFRAME,Dpi)+GetSystemMetricsForDpi(SM_CXPADDEDBORDER,Dpi);
        Height=GetSystemMetricsForDpi(SM_CYCAPTION,Dpi)+Border;
        Icon=GetSystemMetricsForDpi(SM_CYSMICON,Dpi);
        if(!Extend(window))Height=0; // Keep the native frame if composition is unavailable.
        RefreshControls(window);
    }
    void RefreshControls(HWND window){
        Maximized=IsZoomed(window)!=FALSE;
        RECT client{};GetClientRect(window,&client);
        Controls={std::max(0L,client.right-MulDiv(138,Dpi,96)),Border,client.right,Height};
        RECT bounds{},frame{};POINT origin{};
        if(IsWindowVisible(window)&&!IsIconic(window)&&SUCCEEDED(DwmGetWindowAttribute(window,DWMWA_CAPTION_BUTTON_BOUNDS,&bounds,sizeof(bounds)))&&
            bounds.right>bounds.left&&bounds.bottom>bounds.top&&GetWindowRect(window,&frame)&&ClientToScreen(window,&origin)){
            OffsetRect(&bounds,frame.left-origin.x,frame.top-origin.y);
            bounds.left=std::max(0L,bounds.left);bounds.right=std::min(client.right,bounds.right);
            bounds.top=std::max(0L,bounds.top);bounds.bottom=std::min(LONG(Height),bounds.bottom);
            if(bounds.right-bounds.left>=3&&bounds.bottom>bounds.top)Controls=bounds;
        }
    }
    RECT ControlRect(int column)const{
        const auto width=Controls.right-Controls.left;
        return {Controls.left+width*column/3,Controls.top,Controls.left+width*(column+1)/3,Controls.bottom};
    }
    bool Extend(HWND window)const{
        const MARGINS margins{0,0,Height,0};
        return SUCCEEDED(DwmExtendFrameIntoClientArea(window,&margins));
    }
    int HitTest(int x,int y,int width,bool maximized)const{
        constexpr int hits[]{HTMINBUTTON,HTMAXBUTTON,HTCLOSE};
        for(int column=0;column<3;++column){const auto rect=ControlRect(column);if(x>=rect.left&&x<rect.right&&y>=rect.top&&y<rect.bottom)return hits[column];}
        if(!maximized&&y<Border){
            if(x<Border)return HTTOPLEFT;
            if(x>=width-Border)return HTTOPRIGHT;
            return HTTOP;
        }
        if(y<Height){
            if(x<Icon+MulDiv(16,Dpi,96))return HTSYSMENU;
            return HTCAPTION;
        }
        return HTCLIENT;
    }
};
}
