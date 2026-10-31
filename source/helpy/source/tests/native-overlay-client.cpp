#include "Runtime/HelpyNativeOverlayClient.h"
#include <cstdio>
using namespace PS::HelpyOverlayGpu;
static unsigned Released=0;
static unsigned CursorChanges=0;
static LRESULT CALLBACK Procedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam){if(message==WM_APP+0x5a1){if(wParam==1)++CursorChanges;else ++Released;}return DefWindowProcW(window,message,wParam,lParam);}
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(){try{
    WNDCLASSW type{};type.lpfnWndProc=Procedure;type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=L"HelpyPrivateClientTest";Check(RegisterClassW(&type)!=0,"Client fixture class unavailable");const auto window=CreateWindowW(type.lpszClassName,L"Private native client",WS_OVERLAPPEDWINDOW,0,0,800,600,nullptr,nullptr,type.hInstance,nullptr);Check(window!=nullptr,"Client fixture window unavailable");
    ComPtr<ID3D11Device> device;Need(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr),"Client fixture GPU unavailable");ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC gpu{};Need(device.As(&dxgi),"Client fixture DXGI unavailable");Need(dxgi->GetAdapter(&adapter),"Client fixture adapter unavailable");adapter->GetDesc(&gpu);
    HANDLE mapping=nullptr;PS::HelpyOverlayFrames::Page* page=nullptr;PS::HelpyNativeOverlayClient client;std::atomic<unsigned> verifiedFrames=0;
    {
        PS::HelpyNativeOverlayClient hybrid;hybrid.Show();LUID wanted{};
        const auto different=[&](const nlohmann::json&,nlohmann::json& result){result={{"Ready",true},{"Width",800},{"Height",600},{"AdapterLow",gpu.AdapterLuid.LowPart^1u},{"AdapterHigh",gpu.AdapterLuid.HighPart}};return true;};
        for(unsigned i=0;i<100&&!hybrid.AdapterChange(wanted);++i){hybrid.Tick(different,GetCurrentProcessId(),true,device.Get(),.9f,1.f);Sleep(2);}
        Check(hybrid.AdapterChange(wanted)&&wanted.LowPart==(gpu.AdapterLuid.LowPart^1u)&&hybrid.Wanted()&&!hybrid.Failed(),"Hybrid GPU mismatch did not request an adapter change");
        hybrid.Fail("Fixture adapter unavailable");Check(!hybrid.Wanted()&&hybrid.Failed()&&!hybrid.AdapterChange(wanted),"Failed adapter selection kept retrying");hybrid.Shutdown();
    }
    const auto remote=[&](const nlohmann::json& request,nlohmann::json& result){const auto& payload=request.at("Payload");result={{"Ready",true},{"Width",800},{"Height",600},{"Window",uint64_t(reinterpret_cast<uintptr_t>(window))},{"AdapterLow",gpu.AdapterLuid.LowPart},{"AdapterHigh",gpu.AdapterLuid.HighPart}};
        if(payload.at("Action")=="Status")++verifiedFrames;
        if(payload.at("Action")=="Open"){const auto key=Name(GetCurrentProcessId(),GetCurrentProcessId(),payload.at("Nonce").get<std::string>());mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,(key+L".control").c_str());Check(mapping!=nullptr,"Client fixture control page unavailable");page=static_cast<PS::HelpyOverlayFrames::Page*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*page)));Check(page!=nullptr,"Client fixture mapping unavailable");InterlockedExchange(&page->Bound,1);}return true;};
    client.Show();
    for(unsigned i=0;i<100&&(!client.Page()||!PS::HelpyOverlayFrames::Read(client.Page()->Bound));++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f);Sleep(2);}Check(client.Page()&&client.Page()->Bound==1,"Client fixture did not attach");Check(client.Wanted(),"First attachment discarded the pending open request");
    for(unsigned i=0;i<100&&client.Page()->Visible!=1;++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f);Sleep(2);}Check(client.Visible()&&page->Visible==1,"Client fixture did not open");
    Check(verifiedFrames==0,"An attached surface was reported as a displayed frame");InterlockedExchange(&page->Reading,0);
    for(unsigned i=0;i<100;++i){client.Tick(remote,GetCurrentProcessId(),true,device.Get(),.9f,.88f);Sleep(2);}Check(verifiedFrames==1,"First displayed frame was not verified exactly once");
    const auto pump=[](){MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}};
    Check(!client.HardwareCursor(),"Unconfirmed hardware cursor disabled the rendered fallback");page->CursorActive=1;Check(client.HardwareCursor(),"Confirmed hardware cursor was not selected");
    client.Cursor(1);for(unsigned i=0;i<20;++i)client.Cursor(1);pump();Check(page->CursorShape==1&&CursorChanges==1,"Cursor shape changes posted recurring game messages");
    client.Hide();pump();Check(Released==1,"First hide did not release game input");for(unsigned i=0;i<20;++i)client.Tick(remote,GetCurrentProcessId(),false,device.Get(),.9f,.88f);pump();Check(Released==1,"Hidden/disabled client kept posting to the game window");client.Shutdown();pump();Check(Released==1,"Shutdown repeated a completed input release");UnmapViewOfFile(page);CloseHandle(mapping);DestroyWindow(window);UnregisterClassW(type.lpszClassName,type.hInstance);std::puts("Native client first-attachment open, idempotent hide and disabled message budget passed.");return 0;
}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
