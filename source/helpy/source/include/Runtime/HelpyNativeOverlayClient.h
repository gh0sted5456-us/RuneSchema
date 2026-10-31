#pragma once
#include "Runtime/HelpyOverlayGpu.h"
#include "Core/HelpyGameOverlay.h"
#include <future>
#include <functional>
#include <chrono>
#include <cmath>

namespace PS {
class HelpyNativeOverlayClient {
    using Json=nlohmann::json;using Remote=std::function<bool(const Json&,Json&)>;
    std::future<Json> m_request;std::string m_action,m_nonce;Remote m_remote;
    Json m_status=Json::object();uint32_t m_pid=0,m_requestPid=0;bool m_bound=false,m_seenBound=false,m_wanted=false,m_showing=false,m_failed=false,m_verified=false;
    unsigned m_attempts=0;float m_size=0;std::chrono::steady_clock::time_point m_next{};
    HelpyOverlayGpu::Producer m_surface;HWND m_game=nullptr;
    bool m_changeAdapter=false;LUID m_adapter{};
    void Request(Json payload){if(m_request.valid()||!m_remote)return;m_requestPid=m_pid;m_action=payload.value("Action",std::string{});const auto remote=m_remote;
        m_request=std::async(std::launch::async,[remote,payload=std::move(payload)]{Json result;if(!remote({{"Action","HelpyNativeOverlay"},{"Payload",payload}},result))result={{"Ready",false},{"Failed",true},{"Error","The installed RuneSchema bridge does not provide native overlay presentation"}};return result;});}
public:
    std::string Status="Native overlay is waiting for a connected game";
    bool Wanted()const{return m_wanted;}
    bool Visible()const{return m_wanted&&m_bound&&m_surface.Page&&HelpyOverlayFrames::Read(m_surface.Page->Bound)==1;}
    bool Pending()const{return m_request.valid();}
    bool HardwareCursor()const{return Visible()&&HelpyOverlayFrames::Read(m_surface.Page->CursorActive)==1;}
    void Cursor(int shape){if(m_surface.Page&&InterlockedExchange(&m_surface.Page->CursorShape,LONG(shape))!=shape&&m_game){DWORD owner=0;GetWindowThreadProcessId(m_game,&owner);if(owner==m_pid)PostMessageW(m_game,WM_APP+0x5a1,1,0);}}
    bool Failed()const{return m_failed;}
    bool AdapterChange(LUID& adapter)const{adapter=m_adapter;return m_changeAdapter&&!m_failed;}
    void AdapterChanged(){m_surface.Reset();m_bound=false;m_seenBound=false;m_changeAdapter=false;}
    void Fail(std::string error){Hide();m_changeAdapter=false;m_failed=true;Status=std::move(error);}
    HWND GameWindow()const{return m_game;}
    HelpyOverlayFrames::Page* Page(){return m_surface.Page;}
    ID3D11RenderTargetView* Target(int slot){return slot>=0&&slot<3?m_surface.Targets[slot].Get():nullptr;}
    int Begin(){if(!Visible())return -1;InterlockedExchange64(&m_surface.Page->Heartbeat,GetTickCount64());return m_surface.Begin();}
    void Publish(int slot){m_surface.Publish(slot);}
    void Hide(){
        const bool notify=m_wanted||m_showing||(m_surface.Page&&HelpyOverlayFrames::Read(m_surface.Page->Visible)!=0);m_wanted=false;m_showing=false;
        if(m_surface.Page&&notify)InterlockedExchange(&m_surface.Page->Visible,0);
        if(notify&&m_game){DWORD owner=0;GetWindowThreadProcessId(m_game,&owner);if(owner==m_pid)PostMessageW(m_game,WM_APP+0x5a1,0,0);}
    }
    void Show(){m_wanted=true;m_showing=false;m_failed=false;m_verified=false;m_attempts=0;m_next={};}
    void Tick(Remote remote,uint32_t pid,bool enabled,ID3D11Device* device,float size,float opacity){
        m_remote=std::move(remote);
        if(pid!=m_pid){const bool opening=m_wanted&&pid;Hide();m_surface.Reset();m_bound=false;m_failed=false;m_changeAdapter=false;m_attempts=0;m_status=Json::object();m_pid=pid;m_next={};if(opening)Show();}
        if(m_request.valid()&&m_request.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
            try{auto result=m_request.get();if(m_requestPid==pid){m_status=std::move(result);Status=m_status.value("Error",std::string{});m_failed=m_status.value("Failed",false);
                if(Status.empty())Status=m_status.value("Ready",false)?"Native "+m_status.value("Api",std::string("game"))+" presentation is ready":"Detecting the game's active renderer";
                if(m_action=="Open"){m_bound=m_status.value("Ready",false);m_seenBound=false;m_next=std::chrono::steady_clock::now()+std::chrono::seconds(5);}
            }}catch(const std::exception& error){Status=error.what();m_status=Json::object();m_failed=true;}
        }
        if(!enabled||!pid){Hide();m_changeAdapter=false;return;}
        if(m_failed){Hide();return;}
        if(m_surface.Page){
            if(m_bound&&HelpyOverlayFrames::Read(m_surface.Page->Bound))m_seenBound=true;
            if(m_bound&&!HelpyOverlayFrames::Read(m_surface.Page->Bound)&&(m_seenBound||std::chrono::steady_clock::now()>=m_next)){Hide();m_bound=false;m_status=Json::object();m_next={};}
            if(HelpyOverlayFrames::Read(m_surface.Page->InputOverflow)){Hide();Status="Native input queue overflowed; the game input capture was released";}
            InterlockedExchange(&m_surface.Page->Opacity,LONG(std::clamp(opacity,.6f,1.f)*10000));
            if(Visible()){InterlockedExchange64(&m_surface.Page->Heartbeat,GetTickCount64());if(m_showing&&HelpyOverlayFrames::Read(m_surface.Page->Visible)==0){Hide();}}
        }
        const auto now=std::chrono::steady_clock::now();
        if(!m_status.value("Ready",false)){
            if(!m_failed&&m_attempts<30&&!m_request.valid()&&now>=m_next){++m_attempts;Request({{"Action","Prepare"}});m_next=now+std::chrono::seconds(2);}
            else if(m_attempts>=30&&!m_request.valid()){Hide();Request({{"Action","Suspend"}});m_failed=true;Status="Native presentation was not detected. Open Helpy from the tray. Vulkan needs its startup adapter enabled before game launch; unsupported drivers can use Desktop presentation.";}return;
        }
        if(m_request.valid()||m_failed)return;
        if(m_bound&&!m_wanted&&std::abs(m_size-size)>.001f)m_bound=false;
        if(!m_bound&&device){
            try{
                const auto width=m_status.value("Width",0),height=m_status.value("Height",0);if(width<160||height<120)throw std::runtime_error("Native game viewport is not ready");
                Microsoft::WRL::ComPtr<IDXGIDevice> dxgi;Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC gpu{};HelpyOverlayGpu::Need(device->QueryInterface(IID_PPV_ARGS(&dxgi)),"Helpy GPU unavailable");HelpyOverlayGpu::Need(dxgi->GetAdapter(&adapter),"Helpy adapter unavailable");HelpyOverlayGpu::Need(adapter->GetDesc(&gpu),"Helpy adapter identity unavailable");
                m_adapter={m_status.value("AdapterLow",DWORD{}),m_status.value("AdapterHigh",LONG{})};
                if(gpu.AdapterLuid.LowPart!=m_adapter.LowPart||gpu.AdapterLuid.HighPart!=m_adapter.HighPart){m_changeAdapter=true;Status="Aligning Helpy with the game's graphics adapter";return;}
                m_changeAdapter=false;
                m_game=reinterpret_cast<HWND>(uintptr_t(m_status.value("Window",uint64_t{})));const auto bounds=HelpyGameOverlay::Bounds({0,0,width,height},size);
                GUID id{};if(FAILED(CoCreateGuid(&id)))throw std::runtime_error("Cannot reserve native overlay surface");char nonce[33]{};const auto* bytes=reinterpret_cast<const unsigned char*>(&id);for(unsigned i=0;i<16;++i)std::snprintf(nonce+i*2,3,"%02x",bytes[i]);m_nonce=nonce;
                m_surface.Create(device,pid,m_nonce,bounds.right-bounds.left,bounds.bottom-bounds.top);m_surface.Page->Left=bounds.left;m_surface.Page->Top=bounds.top;m_size=size;
                Request({{"Action","Open"},{"AppPid",GetCurrentProcessId()},{"Nonce",m_nonce}});
            }catch(const std::exception& error){Status=error.what();m_wanted=false;m_failed=true;}
        }else if(Visible()&&!m_showing){
            InterlockedExchange64(&m_surface.Page->Heartbeat,GetTickCount64());InterlockedExchange(&m_surface.Page->Visible,1);m_showing=true;Request({{"Action","Show"}});
        }else if(Visible()&&m_showing&&!m_verified&&HelpyOverlayFrames::Read(m_surface.Page->Reading)>=0){
            m_verified=true;Request({{"Action","Status"}});
        }
    }
    void Shutdown(){Hide();if(m_request.valid()){try{m_request.get();}catch(...){}}m_surface.Reset();m_bound=false;m_remote={};}
#if defined(HELPY_PREVIEW)
    void PreviewSurface(ID3D11Device* device,uint32_t width,uint32_t height){
        char nonce[33]{};std::snprintf(nonce,sizeof(nonce),"%032llx",static_cast<unsigned long long>(GetTickCount64())*GetCurrentProcessId());
        m_surface.Create(device,GetCurrentProcessId(),nonce,width,height);m_surface.Page->Bound=1;m_surface.Page->Visible=1;m_surface.Page->Heartbeat=GetTickCount64();m_wanted=true;m_bound=true;
    }
#endif
};
}
