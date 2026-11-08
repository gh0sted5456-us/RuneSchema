#pragma once
#include <nlohmann/json.hpp>
#include "Runtime/HelpyOverlayGpu.h"
#include "Core/HelpyGameOverlay.h"
#include <future>
#include <functional>
#include <chrono>
#include <cmath>

namespace PS {
class HelpyNativeOverlayClient {
public:
    enum class Hud{None,Coordinates,Startup,Combined};
    static RECT HudBounds(int width,int height,float size,Hud hud){
        if(hud==Hud::None)return HelpyGameOverlay::Bounds({0,0,width,height},size,HelpyOverlayFrames::MaxWidth,HelpyOverlayFrames::MaxHeight);
        const int w=std::min(width,hud==Hud::Combined?int(HelpyOverlayFrames::MaxWidth):hud==Hud::Coordinates?1024:768),h=std::min(height,hud==Hud::Startup?480:hud==Hud::Coordinates?128:int(HelpyOverlayFrames::MaxHeight));
        const int left=hud==Hud::Startup?(width-w)/2:width-w,top=hud==Hud::Startup?(height-h)/2:height-h;
        return {left,top,left+w,top+h};
    }
private:
    using Json=nlohmann::json;using Remote=std::function<bool(const Json&,Json&)>;
    std::future<Json> m_request;std::string m_action,m_nonce;Remote m_remote;
    Json m_status=Json::object();uint32_t m_pid=0,m_requestPid=0;bool m_bound=false,m_seenBound=false,m_wanted=false,m_notifications=false,m_showing=false,m_failed=false,m_verified=false,m_openInteractive=false;
    unsigned m_attempts=0;std::chrono::steady_clock::time_point m_next{},m_openDeadline{};
    HelpyOverlayGpu::Producer m_surface;HWND m_game=nullptr;
    bool m_changeAdapter=false,m_recoveredDevice=false;LUID m_adapter{};
    bool RecoverDevice(HRESULT reason){
        if(SUCCEEDED(reason)||m_recoveredDevice)return false;
        m_recoveredDevice=true;
        Hide(true);
        m_surface.Reset();
        m_changeAdapter=true;
        Status="Rebuilding Helpy's native overlay graphics device";
        return true;
    }
    void Request(Json payload){if(m_request.valid()||!m_remote)return;m_requestPid=m_pid;m_action=payload.value("Action",std::string{});if(m_action=="Open")m_openInteractive=payload.at("Interactive").get<bool>();const auto remote=m_remote;
        m_request=std::async(std::launch::async,[remote,payload=std::move(payload)]{Json result;if(!remote({{"Action","HelpyNativeOverlay"},{"Payload",payload}},result))result={{"Ready",false},{"Failed",true},{"Error","The installed RuneSchema bridge does not provide native overlay presentation"}};return result;});}
public:
    std::string Status="Native overlay is waiting for a connected game";
    bool Wanted()const{return m_wanted;}
    bool Visible()const{return m_wanted&&m_showing&&m_bound&&m_surface.Page&&HelpyOverlayFrames::Read(m_surface.Page->Bound)==1;}
    bool NotificationsVisible()const{return m_notifications&&m_showing&&m_bound&&m_surface.Page&&HelpyOverlayFrames::Read(m_surface.Page->Bound)==1;}
    bool Presenting()const{return (m_wanted||m_notifications)&&m_bound&&m_surface.Page&&HelpyOverlayFrames::Read(m_surface.Page->Bound)==1;}
    bool Pending()const{return m_request.valid();}
    bool HardwareCursor()const{return Visible()&&HelpyOverlayFrames::Read(m_surface.Page->CursorActive)==1;}
    void Cursor(int shape){if(m_surface.Page&&InterlockedExchange(&m_surface.Page->CursorShape,LONG(shape))!=shape&&m_game){DWORD owner=0;GetWindowThreadProcessId(m_game,&owner);if(owner==m_pid)PostMessageW(m_game,WM_APP+0x5a1,1,0);}}
    bool Failed()const{return m_failed;}
    bool AdapterChange(LUID& adapter)const{adapter=m_adapter;return m_changeAdapter&&!m_failed;}
    void AdapterChanged(){m_surface.Reset();m_bound=false;m_seenBound=false;m_showing=false;m_changeAdapter=false;}
    void Fail(std::string error){Hide();m_changeAdapter=false;m_failed=true;Status=std::move(error);}
    HWND GameWindow()const{return m_game;}
    HelpyOverlayFrames::Page* Page(){return m_surface.Page;}
    ID3D11RenderTargetView* Target(int slot){return slot>=0&&slot<3?m_surface.Targets[slot].Get():nullptr;}
    int Begin(){if(!Presenting())return -1;InterlockedExchange64(&m_surface.Page->Heartbeat,GetTickCount64());return m_surface.Begin();}
    void Publish(int slot){m_surface.Publish(slot);}
    void Hide(bool suspend=false){
        const bool notify=(!suspend&&(m_wanted||m_notifications))||m_showing||(m_surface.Page&&HelpyOverlayFrames::Read(m_surface.Page->Visible)!=0);if(!suspend)m_wanted=m_notifications=false;m_showing=false;
        if(m_surface.Page&&notify)InterlockedExchange(&m_surface.Page->Visible,0);
        if(notify&&m_game){DWORD owner=0;GetWindowThreadProcessId(m_game,&owner);if(owner==m_pid)PostMessageW(m_game,WM_APP+0x5a1,0,0);}
    }
    void Show(){m_wanted=true;m_notifications=false;m_showing=false;m_failed=false;m_recoveredDevice=false;m_verified=false;m_attempts=0;m_next={};}
    void Notifications(bool enabled){
        enabled=enabled&&!m_wanted&&!m_failed&&m_status.value("PassivePresentation",false);if(enabled==m_notifications)return;
        if(!enabled){Hide();return;}m_notifications=true;m_showing=false;m_verified=false;
    }
    void Tick(Remote remote,uint32_t pid,bool enabled,ID3D11Device* device,float size,float opacity,Hud hud=Hud::None,const std::function<RECT(int,int)>& hudBounds={},HWND foreground=GetForegroundWindow()){
        m_remote=std::move(remote);
        if(pid!=m_pid){const bool opening=m_wanted&&enabled;Hide();m_surface.Reset();m_bound=false;m_game=nullptr;m_failed=false;m_changeAdapter=false;m_recoveredDevice=false;m_attempts=0;m_status=Json::object();m_requestPid=0;m_pid=pid;m_next={};if(opening)Show();}
        if(m_request.valid()&&m_request.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
            try{auto result=m_request.get();if(pid&&m_requestPid==pid){m_status=std::move(result);Status=m_status.value("Error",std::string{});m_failed=m_status.value("Failed",false);
                if(Status.empty())Status=m_status.value("Pending",false)?m_status.value("Status",std::string("Waiting for previous overlay frames to finish")):m_status.value("Ready",false)?"Native "+m_status.value("Api",std::string("game"))+" presentation is ready":"Detecting the game's active renderer";
                if(m_action=="Open"){m_bound=m_status.value("Ready",false)&&!m_status.value("Pending",false);m_seenBound=false;m_next=std::chrono::steady_clock::now()+(m_status.value("Busy",false)?std::chrono::milliseconds(500):m_status.value("Pending",false)?std::chrono::milliseconds(50):std::chrono::milliseconds(5000));}
            }}catch(const std::exception& error){Status=error.what();m_status=Json::object();m_failed=true;}
        }
        if(!enabled){Hide();m_changeAdapter=false;return;}
        if(!pid){Hide(true);m_changeAdapter=false;return;}
        if(m_failed){Hide();return;}
        if(m_surface.Page){
            if(m_bound&&HelpyOverlayFrames::Read(m_surface.Page->Bound))m_seenBound=true;
            if(m_bound&&!HelpyOverlayFrames::Read(m_surface.Page->Bound)&&(m_seenBound||std::chrono::steady_clock::now()>=m_next)){const bool reopen=m_wanted;Hide();m_bound=false;m_status=Json::object();m_next={};if(reopen)Show();}
            if(HelpyOverlayFrames::Read(m_surface.Page->InputOverflow)){Hide();Status="Native input queue overflowed; the game input capture was released";}
            InterlockedExchange(&m_surface.Page->Opacity,LONG(std::clamp(opacity,.6f,1.f)*10000));
            if(Presenting()){InterlockedExchange64(&m_surface.Page->Heartbeat,GetTickCount64());if(foreground!=m_game||(m_showing&&HelpyOverlayFrames::Read(m_surface.Page->Visible)==0))Hide(true);}
        }
        const auto now=std::chrono::steady_clock::now();
        if(m_status.value("Pending",false)){
            if(!m_wanted&&!m_notifications){m_openDeadline=now+std::chrono::seconds(5);return;}
            if(!m_status.value("Busy",false)&&now>=m_openDeadline){Fail("The game GPU did not finish the previous overlay frames within five seconds");return;}
            if(!m_request.valid()&&now>=m_next){if(m_status.value("Busy",false))m_openDeadline=now+std::chrono::seconds(5);Request({{"Action","Open"},{"AppPid",GetCurrentProcessId()},{"Nonce",m_nonce},{"Interactive",m_wanted}});}
            return;
        }
        if(!m_status.value("Ready",false)){
            if(!m_failed&&m_attempts<30&&!m_request.valid()&&now>=m_next){++m_attempts;Request({{"Action","Prepare"}});m_next=now+std::chrono::seconds(2);}
            else if(m_attempts>=30&&!m_request.valid()){Hide();Request({{"Action","Suspend"}});m_failed=true;Status="Native presentation was not detected. Open Helpy from the tray. Vulkan needs its startup adapter enabled before game launch; unsupported drivers can use Desktop presentation.";}return;
        }
        if(m_request.valid()||m_failed||(!m_wanted&&!m_notifications))return;
        // Visibility alone cannot promote a passive surface to an input owner.
        if(m_bound&&m_openInteractive!=m_wanted){if(m_surface.Page)InterlockedExchange(&m_surface.Page->Visible,0);m_bound=false;m_showing=false;}
        if(m_bound&&(!m_wanted||!m_showing)&&m_surface.Page&&m_status.value("Width",0)>0&&m_status.value("Height",0)>0){
            const auto wanted=hud!=Hud::None&&hudBounds?hudBounds(m_status.at("Width"),m_status.at("Height")):HudBounds(m_status.at("Width"),m_status.at("Height"),size,hud);const auto* page=m_surface.Page;
            if(wanted.left!=page->Left||wanted.top!=page->Top||wanted.right-wanted.left!=page->Width||wanted.bottom-wanted.top!=page->Height){InterlockedExchange(&m_surface.Page->Visible,0);m_bound=false;m_showing=false;}
        }
        if(!m_bound&&device){
            try{
                const auto width=m_status.value("Width",0),height=m_status.value("Height",0);if(width<160||height<120){m_status=Json::object();Status="Waiting for the game viewport after resize";m_next=now+std::chrono::milliseconds(250);return;}
                Microsoft::WRL::ComPtr<IDXGIDevice> dxgi;Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC gpu{};HelpyOverlayGpu::Need(device->QueryInterface(IID_PPV_ARGS(&dxgi)),"Helpy GPU unavailable");HelpyOverlayGpu::Need(dxgi->GetAdapter(&adapter),"Helpy adapter unavailable");HelpyOverlayGpu::Need(adapter->GetDesc(&gpu),"Helpy adapter identity unavailable");
                m_adapter={m_status.value("AdapterLow",DWORD{}),m_status.value("AdapterHigh",LONG{})};
                if(gpu.AdapterLuid.LowPart!=m_adapter.LowPart||gpu.AdapterLuid.HighPart!=m_adapter.HighPart){m_changeAdapter=true;Status="Aligning Helpy with the game's graphics adapter";return;}
                m_changeAdapter=false;
                m_game=reinterpret_cast<HWND>(uintptr_t(m_status.value("Window",uint64_t{})));
                DWORD owner=0;GetWindowThreadProcessId(m_game,&owner);if(!m_game||owner!=pid){m_game=nullptr;m_status=Json::object();Status="Waiting for the connected game's renderer window";m_next=now+std::chrono::milliseconds(250);return;}
                const auto bounds=hud!=Hud::None&&hudBounds?hudBounds(width,height):HudBounds(width,height,size,hud);
                GUID id{};if(FAILED(CoCreateGuid(&id)))throw std::runtime_error("Cannot reserve native overlay surface");char nonce[33]{};const auto* bytes=reinterpret_cast<const unsigned char*>(&id);for(unsigned i=0;i<16;++i)std::snprintf(nonce+i*2,3,"%02x",bytes[i]);m_nonce=nonce;
                m_surface.Create(device,pid,m_nonce,bounds.right-bounds.left,bounds.bottom-bounds.top);m_surface.Page->Left=bounds.left;m_surface.Page->Top=bounds.top;m_surface.Page->ViewportWidth=width;m_surface.Page->ViewportHeight=height;
                m_openDeadline=now+std::chrono::seconds(5);Request({{"Action","Open"},{"AppPid",GetCurrentProcessId()},{"Nonce",m_nonce},{"Interactive",m_wanted}});
            }catch(const std::exception& error){
                if(RecoverDevice(device->GetDeviceRemovedReason()))return;
                Fail(error.what());m_surface.Reset();
            }
        }else if(Presenting()&&!m_showing&&foreground==m_game&&!IsIconic(m_game)){
            InterlockedExchange64(&m_surface.Page->Heartbeat,GetTickCount64());InterlockedExchange(&m_surface.Page->Visible,m_notifications?2:1);m_showing=true;Request({{"Action","Show"}});
        }else if(Presenting()&&m_showing&&!m_verified&&HelpyOverlayFrames::Read(m_surface.Page->Reading)>=0){
            m_verified=true;Request({{"Action","Status"}});
        }
    }
    void Shutdown(){Hide();if(m_request.valid()){try{m_request.get();}catch(...){}}m_surface.Reset();m_bound=false;m_remote={};}
#if defined(HELPY_PREVIEW)
    bool PreviewDeviceRemoved(LUID adapter){m_adapter=adapter;return RecoverDevice(DXGI_ERROR_DEVICE_REMOVED);}
    void PreviewSurface(ID3D11Device* device,uint32_t width,uint32_t height,bool notifications=false){
        char nonce[33]{};std::snprintf(nonce,sizeof(nonce),"%032llx",static_cast<unsigned long long>(GetTickCount64())*GetCurrentProcessId());
        m_surface.Create(device,GetCurrentProcessId(),nonce,width,height);m_surface.Page->Bound=1;m_surface.Page->Visible=notifications?2:1;m_surface.Page->Heartbeat=GetTickCount64();m_notifications=notifications;m_wanted=!notifications;m_bound=true;m_showing=true;
    }
#endif
};
}
