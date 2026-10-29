#include "Generator/HelpyDiagnosticOverlay.h"
#include <stdexcept>
static void Check(bool value){if(!value)throw std::runtime_error("In-game FPS overlay regression");}
static HWND FindOverlay(){
    HWND result=nullptr;
    EnumThreadWindows(GetCurrentThreadId(),[](HWND window,LPARAM data)->BOOL{
        wchar_t name[128]{};GetClassNameW(window,name,128);
        if(std::wstring_view(name)==L"RuneSchema.Helpy.DiagnosticToasts.v1"){*reinterpret_cast<HWND*>(data)=window;return FALSE;}return TRUE;
    },reinterpret_cast<LPARAM>(&result));return result;
}
int main(){
    const auto module=GetModuleHandleW(nullptr);
    const HWND game=CreateWindowExW(0,L"STATIC",L"FPS test host",WS_OVERLAPPEDWINDOW,10,10,800,600,nullptr,nullptr,module,nullptr);Check(game!=nullptr);
    PS::HelpyDiagnosticOverlay overlay;PS::DiagnosticToastFeed::Feed feed;PS::HelpyImGuiTheme::Theme theme;
    overlay.Update(module,game,feed,theme,true,"Game 60 FPS");const auto window=FindOverlay();Check(window&&IsWindowVisible(window));
    RECT bounds{};GetWindowRect(window,&bounds);Check(bounds.bottom-bounds.top>0&&bounds.bottom-bounds.top<100&&bounds.right-bounds.left<300);
    const auto style=GetWindowLongPtrW(window,GWL_EXSTYLE);Check((style&WS_EX_NOACTIVATE)&&(style&WS_EX_TRANSPARENT));
    Check(SendMessageW(window,WM_NCHITTEST,0,0)==HTTRANSPARENT);
    feed.Accept(PS::LiveLog::Parse("[RuneSchema] [warning] Overlay fixture warning"),{});
    Check(!feed.Empty());overlay.Update(module,game,feed,theme,true,"Game 60 FPS");
    GetWindowRect(window,&bounds);const auto bothHeight=bounds.bottom-bounds.top;
    overlay.Update(module,game,feed,theme,true);GetWindowRect(window,&bounds);
    Check(IsWindowVisible(window)&&bounds.bottom-bounds.top<bothHeight);
    feed={};
    overlay.Update(module,game,feed,theme,true);Check(!IsWindowVisible(window));
    overlay.Update(module,game,feed,theme,true,"Game 60 FPS");Check(IsWindowVisible(window));
    overlay.Update(module,game,feed,theme,false,"Game 60 FPS");Check(!IsWindowVisible(window));
    overlay.Close();DestroyWindow(game);
}
