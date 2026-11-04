#include "Runtime/HelpyNativeOverlayClient.h"
#include <cstdio>
#include <atomic>
using namespace PS::HelpyOverlayGpu;
static unsigned Released=0;
static unsigned CursorChanges=0;
static LRESULT CALLBACK Procedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam){if(message==WM_APP+0x5a1){if(wParam==1)++CursorChanges;else ++Released;}return DefWindowProcW(window,message,wParam,lParam);}
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(){try{
    using Client=PS::HelpyNativeOverlayClient;
    const auto loading=Client::HudBounds(5120,1440,.9f,Client::Hud::Startup),coordinates=Client::HudBounds(5120,1440,.9f,Client::Hud::Coordinates);
    Check((loading.left+loading.right)/2==2560&&(loading.top+loading.bottom)/2==720&&loading.right-loading.left==768,"Startup HUD must be centered without a full-screen texture");
    Check(coordinates.right==5120&&coordinates.bottom==1440&&coordinates.bottom-coordinates.top==128&&coordinates.right-coordinates.left==1024,"Horizontal coordinates must use the game's bottom-right edge");
    const auto wide=PS::HelpyGameOverlay::Bounds({0,0,7680,4320},.98f,PS::HelpyOverlayFrames::MaxWidth,PS::HelpyOverlayFrames::MaxHeight);
    Check(wide.right-wide.left==3840&&wide.bottom-wide.top==2160&&(wide.left+wide.right)/2==3840,"Ultrawide native panel must stay centered inside its bounded surface");
    const auto smallPanel=PS::HelpyGameOverlay::Bounds({0,0,640,480},.9f,3840,2160);Check(smallPanel.right<=640&&smallPanel.bottom<=480,"Small game panel escaped its viewport");
    WNDCLASSW type{};type.lpfnWndProc=Procedure;type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=L"HelpyPrivateClientTest";Check(RegisterClassW(&type)!=0,"Client fixture class unavailable");const auto window=CreateWindowW(type.lpszClassName,L"Private native client",WS_OVERLAPPEDWINDOW,0,0,800,600,nullptr,nullptr,type.hInstance,nullptr);Check(window!=nullptr,"Client fixture window unavailable");
    ComPtr<ID3D11Device> device;Need(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr),"Client fixture GPU unavailable");ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC gpu{};Need(device.As(&dxgi),"Client fixture DXGI unavailable");Need(dxgi->GetAdapter(&adapter),"Client fixture adapter unavailable");adapter->GetDesc(&gpu);
    HANDLE mapping=nullptr;PS::HelpyOverlayFrames::Page* page=nullptr;PS::HelpyNativeOverlayClient client;std::atomic<unsigned> verifiedFrames=0,opened=0;HWND focused=window;
    {
        PS::HelpyNativeOverlayClient hybrid;hybrid.Show();LUID wanted{};
        const auto different=[&](const nlohmann::json&,nlohmann::json& result){result={{"Ready",true},{"Width",800},{"Height",600},{"AdapterLow",gpu.AdapterLuid.LowPart^1u},{"AdapterHigh",gpu.AdapterLuid.HighPart}};return true;};
        for(unsigned i=0;i<100&&!hybrid.AdapterChange(wanted);++i){hybrid.Tick(different,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);Sleep(2);}
        Check(hybrid.AdapterChange(wanted)&&wanted.LowPart==(gpu.AdapterLuid.LowPart^1u)&&hybrid.Wanted()&&!hybrid.Failed(),"Hybrid GPU mismatch did not request an adapter change");
        hybrid.Fail("Fixture adapter unavailable");Check(!hybrid.Wanted()&&hybrid.Failed()&&!hybrid.AdapterChange(wanted),"Failed adapter selection kept retrying");hybrid.Shutdown();
    }
    bool passive=true;std::atomic<bool> interactive=false,otherOwner=false;std::atomic<unsigned> blockedOpens=0;unsigned busyOpens=0;std::string pendingNonce;unsigned deferredOpens=0;
    const auto remote=[&](const nlohmann::json& request,nlohmann::json& result){const auto& payload=request.at("Payload");result={{"PassivePresentation",passive},{"Ready",true},{"Width",800},{"Height",600},{"Window",uint64_t(reinterpret_cast<uintptr_t>(window))},{"AdapterLow",gpu.AdapterLuid.LowPart},{"AdapterHigh",gpu.AdapterLuid.HighPart}};
        if(payload.at("Action")=="Status")++verifiedFrames;
        if(payload.at("Action")=="Open"){
            if(otherOwner){++blockedOpens;result["Pending"]=true;result["Busy"]=true;return true;}
            const auto nonce=payload.at("Nonce").get<std::string>();
            if(busyOpens){if(pendingNonce.empty())pendingNonce=nonce;Check(pendingNonce==nonce,"Busy attachment recreated its shared surface instead of retrying it");--busyOpens;++deferredOpens;result["Pending"]=true;result["Failed"]=false;result["Status"]="Waiting for previous overlay frames to finish";return true;}
            if(!pendingNonce.empty())Check(pendingNonce==nonce,"Completed attachment lost the pending shared surface");
            if(page)UnmapViewOfFile(page);if(mapping)CloseHandle(mapping);const auto key=Name(GetCurrentProcessId(),GetCurrentProcessId(),nonce);mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,(key+L".control").c_str());Check(mapping!=nullptr,"Client fixture control page unavailable");page=static_cast<PS::HelpyOverlayFrames::Page*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*page)));Check(page!=nullptr,"Client fixture mapping unavailable");InterlockedExchange(&page->Bound,1);interactive=payload.at("Interactive").get<bool>();++opened;
        }return true;};
    for(unsigned i=0;i<40;++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f,Client::Hud::None,{},focused);Sleep(2);}
    Check(opened==0,"Hidden startup acquired a passive panel before any HUD was requested");
    client.Show();
    for(unsigned i=0;i<100&&(!client.Page()||!PS::HelpyOverlayFrames::Read(client.Page()->Bound));++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f,Client::Hud::None,{},focused);Sleep(2);}Check(client.Page()&&client.Page()->Bound==1,"Client fixture did not attach");Check(client.Wanted(),"First attachment discarded the pending open request");
    for(unsigned i=0;i<100&&client.Page()->Visible!=1;++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f,Client::Hud::None,{},focused);Sleep(2);}Check(client.Visible()&&page->Visible==1,"Client fixture did not open");
    Check(verifiedFrames==0,"An attached surface was reported as a displayed frame");InterlockedExchange(&page->Reading,0);
    for(unsigned i=0;i<100;++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f,Client::Hud::None,{},focused);Sleep(2);}Check(verifiedFrames==1,"First displayed frame was not verified exactly once");
    const auto pump=[](){MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}};
    focused=nullptr;client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f,Client::Hud::None,{},focused);pump();const auto pausedReleases=Released;
    Check(client.Wanted()&&!client.Visible()&&page->Visible==0,"Focus loss closed the requested overlay or left it visible over another app");
    for(unsigned i=0;i<20;++i)client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f,Client::Hud::None,{},focused);pump();Check(Released==pausedReleases,"Suspended overlay repeatedly released game input");
    focused=window;for(unsigned i=0;i<100&&!client.Visible();++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f,Client::Hud::None,{},focused);Sleep(2);}Check(client.Wanted()&&client.Visible()&&page->Visible==1,"Returning to the bound game did not resume its overlay");
    Check(!client.HardwareCursor(),"Unconfirmed hardware cursor disabled the rendered fallback");page->CursorActive=1;Check(client.HardwareCursor(),"Confirmed hardware cursor was not selected");
    client.Cursor(1);for(unsigned i=0;i<20;++i)client.Cursor(1);pump();Check(page->CursorShape==1&&CursorChanges==1,"Cursor shape changes posted recurring game messages");
    InterlockedExchange(&page->Bound,0);client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f,Client::Hud::None,{},focused);Check(client.Wanted()&&!client.Failed(),"Viewport resize lost the pending native open request");
    for(unsigned i=0;i<1000&&(opened<2||!client.Visible()||page->Visible!=1);++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f,Client::Hud::None,{},focused);Sleep(2);}Check(opened==2&&client.Visible()&&page->Visible==1,"Native overlay did not recover after viewport resize");pump();const auto releases=Released;
    client.Hide();pump();Check(Released==releases+1,"First hide did not release game input");
    client.Notifications(true);const auto hudBounds=[](int,int){return RECT{20,30,420,230};};for(unsigned i=0;i<100&&(page->Visible!=2||page->Left!=20);++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::Coordinates,hudBounds,focused);Sleep(2);}
    Check(client.NotificationsVisible()&&client.Presenting()&&!client.Wanted()&&!client.Visible()&&!client.HardwareCursor(),"Passive notices opened the interactive panel or cursor");
    Check(page->Visible==2&&PS::HelpyOverlayFrames::Alive(*page,GetTickCount64())&&!PS::HelpyOverlayFrames::CapturesInput(*page,GetTickCount64()),"Notification presentation captured game input");
    Check(page->Left==20&&page->Top==30&&page->Width==400&&page->Height==200,"Saved HUD layout did not update the passive surface bounds");
    busyOpens=3;client.Show();
    for(unsigned i=0;i<1000&&(!client.Visible()||page->Visible!=1);++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);Check(!client.Failed()&&client.Wanted(),"In-flight notification frames failed the interactive overlay");Sleep(2);}
    Check(deferredOpens==3&&client.Visible()&&!client.NotificationsVisible()&&page->Visible==1,"Opening Helpy did not recover after notification GPU contention");client.Hide();pump();const auto finalReleases=Released;
    for(unsigned i=0;i<20;++i)client.Tick(remote,GetCurrentProcessId(),false,device.Get(),.9f,.88f,Client::Hud::None,{},focused);pump();Check(Released==finalReleases,"Hidden/disabled client kept posting to the game window");client.Shutdown();pump();Check(Released==finalReleases,"Shutdown repeated a completed input release");
    pendingNonce.clear();
    {
        Client modes;const auto sameBounds=[](int width,int height){return Client::HudBounds(width,height,.9f,Client::Hud::None);};
        for(unsigned i=0;i<200&&!modes.NotificationsVisible();++i){modes.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::Coordinates,sameBounds,focused);modes.Notifications(true);Sleep(2);}
        Check(modes.NotificationsVisible()&&!interactive,"Passive HUD did not register a noninteractive owner");
        modes.Show();
        for(unsigned i=0;i<200&&(!modes.Visible()||!interactive);++i){modes.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);Sleep(2);}
        Check(modes.Visible()&&interactive,"Opening an equal-sized HUD did not acquire interactive input ownership");
        modes.Hide();
        for(unsigned i=0;i<200&&(!modes.NotificationsVisible()||interactive);++i){modes.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::Coordinates,sameBounds,focused);modes.Notifications(true);Sleep(2);}
        Check(modes.NotificationsVisible()&&!interactive,"Returning to an equal-sized passive HUD retained interactive ownership");modes.Shutdown();
    }
    {
        Client reconnect;reconnect.Show();
        for(unsigned i=0;i<100&&!reconnect.Visible();++i){reconnect.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);Sleep(2);}Check(reconnect.Visible(),"Reconnect fixture did not open");
        reconnect.Tick(remote,0,true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);Check(reconnect.Wanted()&&!reconnect.Visible(),"A temporary bridge disconnect discarded the overlay request");
        for(unsigned i=0;i<100&&!reconnect.Visible();++i){reconnect.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);Sleep(2);}Check(reconnect.Visible(),"Reconnecting the assigned runtime did not resume its overlay");
        reconnect.Hide();focused=nullptr;reconnect.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);focused=window;reconnect.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);Check(!reconnect.Wanted()&&!reconnect.Visible(),"An explicit close reopened the overlay on focus return");reconnect.Shutdown();
    }
    {
        Client delayed;std::promise<void> release;const auto gate=release.get_future().share();std::atomic<bool> waiting=false,first=true;
        const auto stale=[&](const nlohmann::json& request,nlohmann::json& result){if(first.exchange(false)){waiting=true;gate.wait();result={{"Ready",false},{"Failed",true},{"Error","Stale disconnected request"}};return true;}return remote(request,result);};
        delayed.Show();for(unsigned i=0;i<200&&!waiting;++i){delayed.Tick(stale,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);Sleep(2);}
        const bool started=waiting;delayed.Tick(stale,0,true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);delayed.Tick(stale,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);release.set_value();Check(started,"Delayed reconnect fixture never started");
        for(unsigned i=0;i<200&&!delayed.Visible();++i){delayed.Tick(stale,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);Check(!delayed.Failed(),"A stale request failed the reconnected overlay");Sleep(2);}Check(delayed.Visible(),"The reconnected runtime did not recover from a stale response");delayed.Shutdown();
    }
    {
        Client held;otherOwner=true;held.Show();
        for(unsigned i=0;i<200&&!blockedOpens;++i){held.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);Sleep(2);}
        Check(blockedOpens&&!held.Failed(),"A live foreign owner was not treated as a retryable busy surface");
        held.Hide();const auto before=blockedOpens.load();
        for(unsigned i=0;i<320;++i){held.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);Sleep(2);}
        Check(blockedOpens==before,"A closed overlay kept retrying another app's HUD surface");
        otherOwner=false;held.Show();for(unsigned i=0;i<400&&!held.Visible();++i){held.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);Sleep(2);}Check(held.Visible(),"The waiting panel could not reopen after the other owner released it");held.Shutdown();
        Client notices;otherOwner=true;const auto busy=blockedOpens.load();
        for(unsigned i=0;i<200&&blockedOpens==busy;++i){notices.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::Coordinates,{},focused);notices.Notifications(true);Sleep(2);}
        Check(blockedOpens>busy&&!notices.Failed(),"Passive HUD did not wait behind the interactive owner");
        otherOwner=false;for(unsigned i=0;i<400&&!notices.NotificationsVisible();++i){notices.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::Coordinates,{},focused);notices.Notifications(true);Sleep(2);}Check(notices.NotificationsVisible(),"Passive HUD did not resume after an interactive extension closed");notices.Shutdown();
    }
    passive=false;PS::HelpyNativeOverlayClient legacy;
    for(unsigned i=0;i<100;++i){legacy.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f,Client::Hud::None,{},focused);legacy.Notifications(true);Sleep(2);}
    Check(!legacy.NotificationsVisible()&&!legacy.Presenting()&&!legacy.Wanted(),"An older bridge was asked to capture notification input");legacy.Shutdown();
    UnmapViewOfFile(page);CloseHandle(mapping);DestroyWindow(window);UnregisterClassW(type.lpszClassName,type.hInstance);std::puts("Native client attachment, passive notices, input isolation, resize recovery and idempotent hide passed.");return 0;
}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
