#include "Core/HelpyGameOverlay.h"
#include <stdexcept>
static void Check(bool value){if(!value)throw std::runtime_error("Game panel overlay regression");}
int main(){
    const auto module=GetModuleHandleW(nullptr);
    const auto game=CreateWindowExW(0,L"STATIC",L"Private overlay host",WS_OVERLAPPEDWINDOW,10,10,1200,800,nullptr,nullptr,module,nullptr);
    const auto panel=CreateWindowExW(WS_EX_LAYERED,L"STATIC",L"Private Helpy panel",WS_OVERLAPPEDWINDOW,20,20,800,600,nullptr,nullptr,module,nullptr);
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
    overlay.Update(panel,game,true,false,false,.8f);Check(!overlay.Active());
    GetWindowRect(panel,&after);Check(EqualRect(&before,&after));
    GetLayeredWindowAttributes(panel,&color,&alpha,&flags);Check(alpha==240);
    overlay.Update(panel,game,true,true,true,.8f);DestroyWindow(game);
    overlay.Update(panel,game,true,true,true,.8f);Check(!overlay.Active());DestroyWindow(panel);
}
