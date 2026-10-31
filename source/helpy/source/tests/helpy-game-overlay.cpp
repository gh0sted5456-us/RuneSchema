#include "Core/HelpyGameOverlay.h"
#include <stdexcept>
static void Check(bool value){if(!value)throw std::runtime_error("Game panel overlay regression");}
static unsigned Resizes=0;
static LRESULT CALLBACK PanelProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
    if(message==WM_SIZE)++Resizes;
    return DefWindowProcW(window,message,wParam,lParam);
}
int main(){
    const auto module=GetModuleHandleW(nullptr);
    WNDCLASSW type{};type.lpfnWndProc=PanelProc;type.hInstance=module;type.lpszClassName=L"HelpyPrivateOverlayTest";Check(RegisterClassW(&type)!=0);
    const auto game=CreateWindowExW(0,L"STATIC",L"Private overlay host",WS_OVERLAPPEDWINDOW,10,10,1200,800,nullptr,nullptr,module,nullptr);
    const auto panel=CreateWindowExW(WS_EX_LAYERED,type.lpszClassName,L"Private Helpy panel",WS_OVERLAPPEDWINDOW,20,20,800,600,nullptr,nullptr,module,nullptr);
    Check(game&&panel);SetLayeredWindowAttributes(panel,0,240,LWA_ALPHA);ShowWindow(panel,SW_SHOWNOACTIVATE);
    RECT before{},after{};GetWindowRect(panel,&before);
    PS::HelpyGameOverlay overlay;
    overlay.Update(panel,game,false,true,true,.8f);Check(!overlay.Active());
    overlay.Update(panel,game,true,true,true,.8f);Check(overlay.Active());
    Check(GetWindowLongPtrW(panel,GWL_EXSTYLE)&WS_EX_TOPMOST);
    Check(GetWindowLongPtrW(panel,GWLP_HWNDPARENT)==0);
    const auto detached=CreateWindowExW(0,L"STATIC",L"Private detached panel",WS_OVERLAPPEDWINDOW,40,40,200,200,nullptr,nullptr,module,nullptr);
    Check(detached!=nullptr);ShowWindow(detached,SW_SHOWNOACTIVATE);PS::HelpyGameOverlay::Companion(detached,true);Check(GetWindowLongPtrW(detached,GWL_EXSTYLE)&WS_EX_TOPMOST);
    PS::HelpyGameOverlay::Companion(detached,false);Check(!(GetWindowLongPtrW(detached,GWL_EXSTYLE)&WS_EX_TOPMOST));DestroyWindow(detached);
    BYTE alpha=0;DWORD flags=0;COLORREF color=0;GetLayeredWindowAttributes(panel,&color,&alpha,&flags);Check(alpha==204);
    overlay.Update(panel,game,true,true,false,.8f);Check(!(GetWindowLongPtrW(panel,GWL_EXSTYLE)&WS_EX_TOPMOST));
    RECT fitted{};GetWindowRect(panel,&fitted);const auto sizes=Resizes;
    for(int i=0;i<5;++i){
        ShowWindow(panel,SW_HIDE);overlay.Update(panel,game,true,false,false,.8f);Check(overlay.Active()&&!overlay.Focused());
        overlay.Update(panel,game,true,true,true,.8f);ShowWindow(panel,SW_SHOWNOACTIVATE);
        GetWindowRect(panel,&after);Check(EqualRect(&fitted,&after)&&Resizes==sizes);
    }
    // Hidden panels do not touch geometry, opacity, or enumerate a missing game.
    overlay.Update(panel,nullptr,true,false,false,.6f);Check(overlay.Active());
    GetLayeredWindowAttributes(panel,&color,&alpha,&flags);Check(alpha==204&&Resizes==sizes);
    overlay.Update(panel,game,false,false,false,.8f);Check(!overlay.Active());
    GetWindowRect(panel,&after);Check(EqualRect(&before,&after));
    GetLayeredWindowAttributes(panel,&color,&alpha,&flags);Check(alpha==240);
    overlay.Update(panel,game,true,true,true,.8f);DestroyWindow(game);
    overlay.Update(panel,game,true,true,true,.8f);Check(!overlay.Active());DestroyWindow(panel);
    Check(!PS::HelpyGameOverlay::Focus(nullptr,nullptr,true));
    const RECT desktop{-1920,0,1920,1080},area{0,0,1280,720};
    Check(PS::HelpyGameOverlay::GameCursorClip(area,area,desktop));
    Check(PS::HelpyGameOverlay::GameCursorClip({640,360,641,361},area,desktop));
    Check(!PS::HelpyGameOverlay::GameCursorClip(desktop,area,desktop));
    Check(!PS::HelpyGameOverlay::GameCursorClip({-200,0,500,400},area,desktop));
    Check(!PS::HelpyGameOverlay::GameCursorClip({640,360,640,360},area,desktop));
    const RECT large{0,0,3840,2160},small{-1600,50,-800,650};
    const auto half=PS::HelpyGameOverlay::Bounds(large,.5f);Check(half.left==960&&half.right==2880&&half.bottom-half.top==1080);
    const auto minimum=PS::HelpyGameOverlay::Bounds(small,.45f);Check(minimum.left>=small.left&&minimum.right<=small.right&&minimum.bottom<=small.bottom&&minimum.bottom-minimum.top==560);
    UnregisterClassW(type.lpszClassName,module);
}
