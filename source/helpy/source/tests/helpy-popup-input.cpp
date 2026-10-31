#include "Core/HelpyPopupInput.h"
#include "Core/HelpyFramePacing.h"
#include "Core/HelpyUiMotion.h"
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <filesystem>
static void Require(bool value){if(!value)throw std::runtime_error("Helpy popup input regression");}
static std::string Read(const char* path){std::ifstream input(path);Require(input.good());return {std::istreambuf_iterator<char>(input),{}};}
int main(int argc,char** argv){
    PS::HelpyPushPacing push;
    Require(push.Delay(100)==0);push.Polled(100);
    Require(push.Delay(101)==39&&push.Delay(139)==1&&push.Delay(140)==0&&push.Delay(600)==0);
    unsigned polls=0;for(unsigned ms=600;ms<1600;++ms)if(!push.Delay(ms)){++polls;push.Polled(ms);}Require(polls==25);
    using namespace std::chrono_literals;
    const auto now=PS::HelpyFramePacing::Clock::time_point{};PS::HelpyFramePacing pacing;
    Require(!pacing.Due(now,false,false));Require(pacing.Due(now,true,false,30));
    Require(!pacing.Due(now+1ms,true,false,30));Require(pacing.Due(now+2ms,true,true,30));
    Require(!pacing.Due(now+35ms,true,true,30));Require(pacing.Due(now+36ms,true,true,30));
    Require(pacing.Due(now+70ms,true,false,30));Require(!pacing.Due(now+169ms,true,false,30));
    Require(!pacing.Due(now+170ms,false,false));Require(pacing.Due(now+171ms,true,true));
    PS::HelpyFramePacing highRefresh;unsigned frames=0;
    for(int tick=0;tick<240;++tick)frames+=highRefresh.Due(now+std::chrono::microseconds(tick*4167),true,true);
    Require(frames>=58&&frames<=60);
    PS::HelpyFramePacing fast;frames=0;for(int tick=0;tick<1000;++tick)frames+=fast.Due(now+std::chrono::milliseconds(tick),true,true,144);Require(frames>=140&&frames<=144);
    PS::HelpyFramePacing inactive;frames=0;
    for(int tick=0;tick<240;++tick)frames+=inactive.Due(now+std::chrono::microseconds(tick*4167),true,false);
    Require(frames<=10);
    PS::HelpyFramePacing adjustable;Require(adjustable.Due(now,true,true,60));
    Require(!adjustable.Due(now+16ms,true,true,60));Require(adjustable.Due(now+17ms,true,true,60));
    Require(adjustable.Due(now+18ms,true,true,15));Require(!adjustable.Due(now+84ms,true,true,15));
    Require(adjustable.Due(now+85ms,true,true,15));
    Require(adjustable.WaitMs(now+85ms,true)==16);Require(adjustable.WaitMs(now+150ms,true)==2);
    Require(adjustable.WaitMs(now,false)==50);
    Require(adjustable.WaitMs(now+160ms,true)==0);
    PS::HelpyFramePacing late;
    Require(late.Due(now,true,true,60)&&late.Due(now+18ms,true,true,60));
    Require(late.Next<now+34ms); // Preserve the cadence after a late wake.
    Require(late.Due(now+34ms,true,true,60)&&late.Next<now+51ms);
    Require(late.Due(now+5s,true,true,60)&&!late.Due(now+5s,true,true,60));
    PS::HelpyUiMotion::PageFade fade;
    Require(fade.Alpha(0,0,true)==1.f&&fade.Alpha(1,1,true)==.6f);
    const auto half=fade.Alpha(1,1.08,true);Require(half>.9f&&half<1.f);
    Require(fade.Alpha(1,1.17,true)==1.f&&fade.Alpha(2,2,false)==1.f);
    Require(fade.Alpha(2,2.01,true)==1.f);
    const auto hover=PS::HelpyUiMotion::Hover(0,true,.016f,true);Require(hover>0&&hover<1);
    Require(PS::HelpyUiMotion::Hover(hover,false,.016f,true)<hover);
    Require(PS::HelpyUiMotion::Hover(0,true,.016f,false)==1.f);
    using namespace PS::HelpyPopupInput;
    for(unsigned flags=0;flags<4;++flags){
        Require(CanToggle(flags&1,flags&2)==(flags!=0));
        Require(ReturnToGame(flags&1,flags&2)==(flags==3));
        Require(!CanToggle(flags&1,flags&2,false,true));
        Require(CanToggle(flags&1,flags&2,true)==bool((flags&1)&&!(flags&2)));
    }
    HotkeyLatch latch;Require(latch.Poll(true));Require(!latch.Poll(true));Require(!latch.Press());
    Require(!latch.Poll(false));Require(latch.Press());Require(!latch.Poll(true));
    latch.Release();Require(latch.Poll(true));
    Require(argc==2);const auto source=Read(argv[1]);
    Require(source.find("CreateWindowExW(WS_EX_APPWINDOW")!=std::string::npos);
    Require(source.find("WS_OVERLAPPEDWINDOW")!=std::string::npos);
    Require(source.find("WS_EX_NOACTIVATE")==std::string::npos);
    Require(source.find("WS_EX_TOPMOST")==std::string::npos);
    Require(source.find("SetWindowsHookExW(")==std::string::npos);
    Require(source.find("GWLP_HWNDPARENT")==std::string::npos);
    Require(source.find("HelpyGameOverlay::Focus(m_hwnd,m_gameWindow,overlay)")!=std::string::npos);
    Require(source.find("SyncGameOverlay(true);ShowWindow(m_hwnd,SW_SHOWNOACTIVATE)")!=std::string::npos);
    Require(source.find("if(returnToGame)SetForegroundWindow(m_gameWindow)")!=std::string::npos);
    Require(source.find("WH_KEYBOARD_LL")==std::string::npos);
    Require(source.find("ClipCursor(")==std::string::npos&&source.find("SetKeyboardState(")==std::string::npos);
    Require(source.find("PollHotkey(msg.message==WM_KEYDOWN)")!=std::string::npos);
    Require(source.find("CanToggle(GameWindow(foreground),OwnsWindow(foreground)||(m_nativeOverlay.Visible()&&foreground==m_gameWindow),editing,modified)")!=std::string::npos);
    Require(source.find("if(key!=boundKey){Hotkey.Poll(down);boundKey=key;}else PollHotkey(down);")!=std::string::npos);
    Require(source.find("GetAsyncKeyState(VK_F5)")==std::string::npos);
    const auto detached=Read((std::filesystem::path(argv[1]).parent_path()/"DetachedWorkspaces.inl").string().c_str());
    for(const auto* owned:{"m_publishedWindow.load()","m_publishedBayWindow.load()","m_publishedCacheWindow.load()","m_publishedWorkspaces"})Require(detached.find(owned)!=std::string::npos);
    Require(source.find("if(m_visible.load()&&GetForegroundWindow()!=m_gameWindow)ClosePopup()")==std::string::npos);
}
