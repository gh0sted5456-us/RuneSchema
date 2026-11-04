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
    overlay.Update(module,game,feed,theme,true,"X 123456   Y -234567   Z 890",{},PS::HelpyDiagnosticOverlay::Mode::Coordinates);
    RECT client{};GetClientRect(game,&client);POINT corner{client.right,client.bottom};ClientToScreen(game,&corner);GetWindowRect(window,&bounds);
    Check(IsWindowVisible(window)&&bounds.right<=corner.x&&corner.x-bounds.right<30&&bounds.bottom<=corner.y&&corner.y-bounds.bottom<30);
    Check(SendMessageW(window,WM_NCHITTEST,0,0)==HTTRANSPARENT);
    const auto oneRowHeight=bounds.bottom-bounds.top;
    overlay.Update(module,game,feed,theme,true,"X 123456   Y -234567   Z 890   |   Pitch -12.5   Yaw 90.0",{},PS::HelpyDiagnosticOverlay::Mode::Coordinates);
    GetWindowRect(window,&bounds);Check(bounds.bottom-bounds.top==oneRowHeight&&bounds.right-bounds.left>=500&&corner.x-bounds.right<30&&corner.y-bounds.bottom<30);
    Check(SendMessageW(window,WM_NCHITTEST,0,0)==HTTRANSPARENT);
    MoveWindow(game,50,60,650,450,TRUE);overlay.Update(module,game,feed,theme,true,"X 123457   Y -234567   Z 890   |   Pitch -12.5   Yaw 90.0",{},PS::HelpyDiagnosticOverlay::Mode::Coordinates);
    GetClientRect(game,&client);corner={client.right,client.bottom};ClientToScreen(game,&corner);GetWindowRect(window,&bounds);Check(corner.x-bounds.right<30&&corner.y-bounds.bottom<30);
    PS::HelpyHudLayout::Settings hud;hud.Custom=true;hud.Minimap=true;overlay.Update(module,game,feed,theme,true,"X 123457   Y -234567   Z 890   |   Pitch -12.5   Yaw 90.0",{},PS::HelpyDiagnosticOverlay::Mode::Coordinates,hud);GetWindowRect(window,&bounds);const auto map=PS::HelpyHudLayout::Map(float(client.right),float(client.bottom),hud);POINT origin{};ClientToScreen(game,&origin);Check(bounds.right-bounds.left==LONG(map.W)&&bounds.left==origin.x+LONG(map.X)&&bounds.top>=origin.y+LONG(map.Y+map.H));
    overlay.Update(module,game,feed,theme,true,"Registering game content",{},PS::HelpyDiagnosticOverlay::Mode::Startup);GetWindowRect(window,&bounds);
    POINT center{client.right/2,client.bottom/2};ClientToScreen(game,&center);Check(std::abs((bounds.left+bounds.right)/2-center.x)<3&&std::abs((bounds.top+bounds.bottom)/2-center.y)<3);
    Check(SendMessageW(window,WM_NCHITTEST,0,0)==HTTRANSPARENT);
    const auto startup=PS::LoadingIndicator::FitStartup(float(client.right),float(client.bottom),1);
    Check(bounds.right-bounds.left>360&&bounds.bottom-bounds.top>270);
    Check(bounds.top+startup.LogoY<center.y&&startup.WheelY-startup.WheelRadius>startup.LogoY+startup.RuneSize*.5f);
    overlay.StartupLogos(CopyIcon(LoadIconW(nullptr,IDI_APPLICATION)),CopyIcon(LoadIconW(nullptr,IDI_APPLICATION)),3.2f,CopyIcon(LoadIconW(nullptr,IDI_APPLICATION)));
    RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW);
    const auto handles=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    for(int frame=0;frame<120;++frame)RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW);
    Check(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<=handles+1);
    overlay.Close();DestroyWindow(game);
}
