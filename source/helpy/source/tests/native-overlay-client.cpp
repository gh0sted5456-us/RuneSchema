#include "Runtime/HelpyNativeOverlayClient.h"
#include <cstdio>
using namespace PS::HelpyOverlayGpu;
static unsigned Released=0;
static unsigned CursorChanges=0;
static LRESULT CALLBACK Procedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam){if(message==WM_APP+0x5a1){if(wParam==1)++CursorChanges;else ++Released;}return DefWindowProcW(window,message,wParam,lParam);}
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(){try{
    const auto wide=PS::HelpyGameOverlay::Bounds({0,0,7680,4320},.98f,PS::HelpyOverlayFrames::MaxWidth,PS::HelpyOverlayFrames::MaxHeight);
    Check(wide.right-wide.left==3840&&wide.bottom-wide.top==2160&&(wide.left+wide.right)/2==3840,"Ultrawide native panel must stay centered inside its bounded surface");
    const auto smallPanel=PS::HelpyGameOverlay::Bounds({0,0,640,480},.9f,3840,2160);Check(smallPanel.right<=640&&smallPanel.bottom<=480,"Small game panel escaped its viewport");
    WNDCLASSW type{};type.lpfnWndProc=Procedure;type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=L"HelpyPrivateClientTest";Check(RegisterClassW(&type)!=0,"Client fixture class unavailable");const auto window=CreateWindowW(type.lpszClassName,L"Private native client",WS_OVERLAPPEDWINDOW,0,0,800,600,nullptr,nullptr,type.hInstance,nullptr);Check(window!=nullptr,"Client fixture window unavailable");
    ComPtr<ID3D11Device> device;Need(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr),"Client fixture GPU unavailable");ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC gpu{};Need(device.As(&dxgi),"Client fixture DXGI unavailable");Need(dxgi->GetAdapter(&adapter),"Client fixture adapter unavailable");adapter->GetDesc(&gpu);
    HANDLE mapping=nullptr;PS::HelpyOverlayFrames::Page* page=nullptr;PS::HelpyNativeOverlayClient client;std::atomic<unsigned> verifiedFrames=0,opened=0;
    {
        PS::HelpyNativeOverlayClient hybrid;hybrid.Show();LUID wanted{};
        const auto different=[&](const nlohmann::json&,nlohmann::json& result){result={{"Ready",true},{"Width",800},{"Height",600},{"AdapterLow",gpu.AdapterLuid.LowPart^1u},{"AdapterHigh",gpu.AdapterLuid.HighPart}};return true;};
        for(unsigned i=0;i<100&&!hybrid.AdapterChange(wanted);++i){hybrid.Tick(different,GetCurrentProcessId(),true,device.Get(),.9f,1.f);Sleep(2);}
        Check(hybrid.AdapterChange(wanted)&&wanted.LowPart==(gpu.AdapterLuid.LowPart^1u)&&hybrid.Wanted()&&!hybrid.Failed(),"Hybrid GPU mismatch did not request an adapter change");
        hybrid.Fail("Fixture adapter unavailable");Check(!hybrid.Wanted()&&hybrid.Failed()&&!hybrid.AdapterChange(wanted),"Failed adapter selection kept retrying");hybrid.Shutdown();
    }
    bool passive=true;
    const auto remote=[&](const nlohmann::json& request,nlohmann::json& result){const auto& payload=request.at("Payload");result={{"PassivePresentation",passive},{"Ready",true},{"Width",800},{"Height",600},{"Window",uint64_t(reinterpret_cast<uintptr_t>(window))},{"AdapterLow",gpu.AdapterLuid.LowPart},{"AdapterHigh",gpu.AdapterLuid.HighPart}};
        if(payload.at("Action")=="Status")++verifiedFrames;
        if(payload.at("Action")=="Open"){if(page)UnmapViewOfFile(page);if(mapping)CloseHandle(mapping);const auto key=Name(GetCurrentProcessId(),GetCurrentProcessId(),payload.at("Nonce").get<std::string>());mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,(key+L".control").c_str());Check(mapping!=nullptr,"Client fixture control page unavailable");page=static_cast<PS::HelpyOverlayFrames::Page*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*page)));Check(page!=nullptr,"Client fixture mapping unavailable");InterlockedExchange(&page->Bound,1);++opened;}return true;};
    client.Show();
    for(unsigned i=0;i<100&&(!client.Page()||!PS::HelpyOverlayFrames::Read(client.Page()->Bound));++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f);Sleep(2);}Check(client.Page()&&client.Page()->Bound==1,"Client fixture did not attach");Check(client.Wanted(),"First attachment discarded the pending open request");
    for(unsigned i=0;i<100&&client.Page()->Visible!=1;++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f);Sleep(2);}Check(client.Visible()&&page->Visible==1,"Client fixture did not open");
    Check(verifiedFrames==0,"An attached surface was reported as a displayed frame");InterlockedExchange(&page->Reading,0);
    for(unsigned i=0;i<100;++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f);Sleep(2);}Check(verifiedFrames==1,"First displayed frame was not verified exactly once");
    const auto pump=[](){MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}};
    Check(!client.HardwareCursor(),"Unconfirmed hardware cursor disabled the rendered fallback");page->CursorActive=1;Check(client.HardwareCursor(),"Confirmed hardware cursor was not selected");
    client.Cursor(1);for(unsigned i=0;i<20;++i)client.Cursor(1);pump();Check(page->CursorShape==1&&CursorChanges==1,"Cursor shape changes posted recurring game messages");
    InterlockedExchange(&page->Bound,0);client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f);Check(client.Wanted()&&!client.Failed(),"Viewport resize lost the pending native open request");
    for(unsigned i=0;i<1000&&(opened<2||!client.Visible()||page->Visible!=1);++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f);Sleep(2);}Check(opened==2&&client.Visible()&&page->Visible==1,"Native overlay did not recover after viewport resize");pump();const auto releases=Released;
    client.Hide();pump();Check(Released==releases+1,"First hide did not release game input");
    client.Notifications(true);for(unsigned i=0;i<100&&page->Visible!=2;++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f);Sleep(2);}
    Check(client.NotificationsVisible()&&client.Presenting()&&!client.Wanted()&&!client.Visible()&&!client.HardwareCursor(),"Passive notices opened the interactive panel or cursor");
    Check(page->Visible==2&&PS::HelpyOverlayFrames::Alive(*page,GetTickCount64())&&!PS::HelpyOverlayFrames::CapturesInput(*page,GetTickCount64()),"Notification presentation captured game input");
    client.Show();for(unsigned i=0;i<100&&page->Visible!=1;++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f);Sleep(2);}
    Check(client.Visible()&&!client.NotificationsVisible()&&page->Visible==1,"Opening Helpy did not replace passive notices");client.Hide();pump();const auto finalReleases=Released;
    for(unsigned i=0;i<20;++i)client.Tick(remote,GetCurrentProcessId(),false,device.Get(),.9f,.88f);pump();Check(Released==finalReleases,"Hidden/disabled client kept posting to the game window");client.Shutdown();pump();Check(Released==finalReleases,"Shutdown repeated a completed input release");
    passive=false;PS::HelpyNativeOverlayClient legacy;
    for(unsigned i=0;i<100;++i){legacy.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,1.f);legacy.Notifications(true);Sleep(2);}
    Check(!legacy.NotificationsVisible()&&!legacy.Presenting()&&!legacy.Wanted(),"An older bridge was asked to capture notification input");legacy.Shutdown();
    UnmapViewOfFile(page);CloseHandle(mapping);DestroyWindow(window);UnregisterClassW(type.lpszClassName,type.hInstance);std::puts("Native client attachment, passive notices, input isolation, resize recovery and idempotent hide passed.");return 0;
}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
