#pragma once
#include "RuneSchemaCompanionSurface.h"
#include "RuneSchemaHudSurface.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include "Generator/HelpyControllerInput.h"
#include <bcrypt.h>
#include <shellapi.h>
#include <filesystem>
#include <memory>

namespace PS::HelpyCompanionSurface {
using Microsoft::WRL::ComPtr;
namespace Wire=CompanionSurface;
struct Host {
    HWND Window{};ID3D11Device* Device{};ID3D11DeviceContext* Context{};
    bool ReturnHome=false;
};
inline Host Current;
inline std::string ActiveRoute;
class Surface {
    HANDLE Mapping{},Process{},FrameMapping{};Wire::Session* Session{};HudFrames::Page* Page{};
    ComPtr<ID3D11Device5> Device;ComPtr<ID3D11DeviceContext4> Context;
    std::array<ComPtr<ID3D11Texture2D>,3> Pictures;
    ComPtr<ID3D11Texture2D> Copy;ComPtr<ID3D11ShaderResourceView> View;
    ComPtr<ID3D11Fence> Ready,Done;
    std::string Nonce,FrameNonce,Owner,Status;uint32_t AppPid{};uint64_t Completed{},Started{},LastBind{},FrameAt{};
    bool Launched=false,Bound=false,Shown=false;std::array<bool,256> Keys{};std::array<bool,5> Buttons{};
    void CloseFrame() {
        if(Page){InterlockedExchange(&Page->Visible,0);InterlockedExchange(&Page->Bound,0);UnmapViewOfFile(Page);Page=nullptr;}
        if(FrameMapping){CloseHandle(FrameMapping);FrameMapping=nullptr;}
        if(Process){CloseHandle(Process);Process=nullptr;}
        for(auto& picture:Pictures)picture.Reset();Copy.Reset();View.Reset();Ready.Reset();Done.Reset();FrameNonce.clear();AppPid=0;Completed=0;Keys={};Buttons={};
    }
    void Reset() {
        CloseFrame();if(Session){InterlockedExchange(&Session->Visible,0);UnmapViewOfFile(Session);Session=nullptr;}
        if(Mapping){CloseHandle(Mapping);Mapping=nullptr;}Device.Reset();Context.Reset();Owner.clear();Nonce.clear();Status.clear();Launched=Bound=Shown=false;
    }
    void Create(const std::string& route) {
        Reset();Owner=route;Started=FrameAt=GetTickCount64();
        unsigned char random[16]{};if(BCryptGenRandom(nullptr,random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("Cannot create companion session");
        constexpr char hex[]="0123456789abcdef";for(auto byte:random){Nonce+=hex[byte>>4];Nonce+=hex[byte&15];}
        HudGpu::Need(Current.Device->QueryInterface(IID_PPV_ARGS(&Device)),"Companion sharing requires Direct3D 11.4");HudGpu::Need(Current.Context->QueryInterface(IID_PPV_ARGS(&Context)),"Companion context sharing unavailable");
        PSECURITY_DESCRIPTOR descriptor{};if(!UserSecurity::SameUserPipeSecurity(descriptor))throw std::runtime_error("Companion session security unavailable");
        SECURITY_ATTRIBUTES attributes{sizeof(attributes),descriptor,FALSE};Mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,&attributes,PAGE_READWRITE,0,sizeof(Wire::Session),Wire::Name(GetCurrentProcessId(),Nonce.c_str()).c_str());const auto error=GetLastError();LocalFree(descriptor);
        if(!Mapping||error==ERROR_ALREADY_EXISTS)throw std::runtime_error("Companion session already exists");
        Session=static_cast<Wire::Session*>(MapViewOfFile(Mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Wire::Session)));if(!Session)throw std::runtime_error("Cannot map companion session");
        new(Session)Wire::Session;Session->HostPid=GetCurrentProcessId();Session->Dpi=GetDpiForWindow(Current.Window);
        ComPtr<IDXGIDevice> dxgi;HudGpu::Need(Device.As(&dxgi),"Cannot identify host GPU");ComPtr<IDXGIAdapter> adapter;HudGpu::Need(dxgi->GetAdapter(&adapter),"Cannot identify host adapter");DXGI_ADAPTER_DESC desc{};HudGpu::Need(adapter->GetDesc(&desc),"Cannot read host adapter");Session->AdapterLow=desc.AdapterLuid.LowPart;Session->AdapterHigh=desc.AdapterLuid.HighPart;
    }
    void Open(const Wire::Frame& frame) {
        CloseFrame();const auto key=HudGpu::Name(GetCurrentProcessId(),frame.AppPid,frame.Nonce);
        FrameMapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,(key+L".control").c_str());if(!FrameMapping)throw std::runtime_error("Companion frame is not ready");
        Page=static_cast<HudFrames::Page*>(MapViewOfFile(FrameMapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(HudFrames::Page)));
        if(!Page||!HudFrames::Shape(*Page,GetCurrentProcessId(),frame.AppPid)||Page->Width!=frame.Width||Page->Height!=frame.Height)throw std::runtime_error("Companion frame identity differs");
        Process=OpenProcess(SYNCHRONIZE|PROCESS_DUP_HANDLE,FALSE,frame.AppPid);if(!Process)throw std::runtime_error("Companion process is unavailable");
        for(unsigned i=0;i<3;++i){HudGpu::Need(Device->OpenSharedResourceByName((key+L".texture"+std::to_wstring(i)).c_str(),DXGI_SHARED_RESOURCE_READ,IID_PPV_ARGS(&Pictures[i])),"Cannot open companion picture");
            D3D11_TEXTURE2D_DESC desc{};Pictures[i]->GetDesc(&desc);if(desc.Width!=frame.Width||desc.Height!=frame.Height||desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||desc.MipLevels!=1||desc.ArraySize!=1||desc.SampleDesc.Count!=1)throw std::runtime_error("Companion texture shape differs");}
        for(unsigned i=0;i<2;++i){HANDLE handle{};if(!DuplicateHandle(Process,reinterpret_cast<HANDLE>(uintptr_t(i?Page->DoneHandle:Page->ReadyHandle)),GetCurrentProcess(),&handle,0,FALSE,DUPLICATE_SAME_ACCESS))throw std::runtime_error("Cannot open companion synchronization");
            const auto result=i?Device->OpenSharedFence(handle,IID_PPV_ARGS(&Done)):Device->OpenSharedFence(handle,IID_PPV_ARGS(&Ready));CloseHandle(handle);HudGpu::Need(result,"Companion synchronization unavailable");}
        D3D11_TEXTURE2D_DESC desc{};Pictures[0]->GetDesc(&desc);desc.MiscFlags=0;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;HudGpu::Need(Device->CreateTexture2D(&desc,nullptr,&Copy),"Cannot create companion display");HudGpu::Need(Device->CreateShaderResourceView(Copy.Get(),nullptr,&View),"Cannot create companion view");
        FrameNonce=frame.Nonce;AppPid=frame.AppPid;InterlockedExchange(&Page->Bound,1);
    }
    void Input(ImVec2 at,ImVec2 size) {
        auto& io=ImGui::GetIO();const bool focused=!io.AppFocusLost&&ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        const bool hover=ImGui::IsItemHovered();const auto push=[&](HudFrames::Input event){HudFrames::Push(*Page,event);};
        const auto coordinate=[](float value){return std::isfinite(value)?int32_t(std::clamp(value,-1000000.f,1000000.f)):-1000000;};
        const auto x=coordinate((io.MousePos.x-at.x)*Page->Width/std::max(1.f,size.x)),y=coordinate((io.MousePos.y-at.y)*Page->Height/std::max(1.f,size.y));
        push({HudFrames::InputKind::Mouse,0,hover?x:INT32_MIN,hover?y:INT32_MIN});
        constexpr std::array down{WM_LBUTTONDOWN,WM_RBUTTONDOWN,WM_MBUTTONDOWN,WM_XBUTTONDOWN,WM_XBUTTONDOWN},up{WM_LBUTTONUP,WM_RBUTTONUP,WM_MBUTTONUP,WM_XBUTTONUP,WM_XBUTTONUP};
        for(int i=0;i<5;++i){const bool held=focused&&io.MouseDown[i]&&(hover||Buttons[i]);if(held!=Buttons[i]){push({HudFrames::InputKind::Button,uint32_t(held?down[i]:up[i]),x,y,i>=3?uint32_t((i==3?XBUTTON1:XBUTTON2)<<16):0});Buttons[i]=held;}}
        if(focused&&hover&&(io.MouseWheel||io.MouseWheelH)){if(io.MouseWheel)push({HudFrames::InputKind::Wheel,WM_MOUSEWHEEL,0,int32_t(io.MouseWheel*120)});if(io.MouseWheelH)push({HudFrames::InputKind::Wheel,WM_MOUSEHWHEEL,0,int32_t(io.MouseWheelH*120)});}
        const auto key=[&](ImGuiKey imgui,int vk){const bool held=focused&&ImGui::IsKeyDown(imgui);if(held!=Keys[vk]){push({HudFrames::InputKind::Key,uint32_t(vk),(io.KeyCtrl?1:0)|(io.KeyShift?2:0)|(io.KeyAlt?4:0)|(io.KeySuper?8:0),0,held?0u:0x80000000u});Keys[vk]=held;}};
        for(int i=0;i<26;++i)key(ImGuiKey(ImGuiKey_A+i),'A'+i);for(int i=0;i<10;++i)key(ImGuiKey(ImGuiKey_0+i),'0'+i);
        constexpr std::pair<ImGuiKey,int> pairs[]{{ImGuiKey_Tab,VK_TAB},{ImGuiKey_LeftArrow,VK_LEFT},{ImGuiKey_RightArrow,VK_RIGHT},{ImGuiKey_UpArrow,VK_UP},{ImGuiKey_DownArrow,VK_DOWN},{ImGuiKey_PageUp,VK_PRIOR},{ImGuiKey_PageDown,VK_NEXT},{ImGuiKey_Home,VK_HOME},{ImGuiKey_End,VK_END},{ImGuiKey_Insert,VK_INSERT},{ImGuiKey_Delete,VK_DELETE},{ImGuiKey_Backspace,VK_BACK},{ImGuiKey_Space,VK_SPACE},{ImGuiKey_Enter,VK_RETURN},{ImGuiKey_Escape,VK_ESCAPE},{ImGuiKey_LeftCtrl,VK_LCONTROL},{ImGuiKey_RightCtrl,VK_RCONTROL},{ImGuiKey_LeftShift,VK_LSHIFT},{ImGuiKey_RightShift,VK_RSHIFT},{ImGuiKey_LeftAlt,VK_LMENU},{ImGuiKey_RightAlt,VK_RMENU}};
        for(const auto& [imgui,vk]:pairs)key(imgui,vk);
        if(focused)for(const auto character:io.InputQueueCharacters){if(character<=0xffff)push({HudFrames::InputKind::Character,uint32_t(character)});else{const auto code=uint32_t(character)-0x10000;push({HudFrames::InputKind::Character,0xd800+(code>>10)});push({HudFrames::InputKind::Character,0xdc00+(code&1023)});}}
        const auto pad=focused?HelpyControllerInput::PhysicalPad:XINPUT_GAMEPAD{};InterlockedIncrement(&Page->PadSequence);std::memcpy(Page->Pad.data(),&pad,sizeof(pad));MemoryBarrier();InterlockedIncrement(&Page->PadSequence);
    }
public:
    ~Surface(){Reset();}
    bool Interactive()const{return Page&&View&&Completed&&Shown;}
    const std::string& Route()const{return Owner;}
    int32_t Draw(const std::string& route,const RuneSchemaCompanionV1& app) {
        if(!Current.Window||!Current.Device||!Current.Context)return RS_PLUGIN_FAILED;
        if(Owner!=route||!Session)Create(route);
        if(HudFrames::Read(Session->ReturnRequested)){Reset();Current.ReturnHome=true;return RS_PLUGIN_OK;}
        const auto size=ImGui::GetContentRegionAvail();const LONG wantWidth=std::clamp(LONG(size.x),160L,LONG(HudFrames::MaxWidth)),wantHeight=std::clamp(LONG(size.y),120L,LONG(HudFrames::MaxHeight));
        InterlockedExchange(&Session->RequestedWidth,wantWidth);InterlockedExchange(&Session->RequestedHeight,wantHeight);InterlockedExchange(&Session->Visible,1);InterlockedExchange64(&Session->HostHeartbeat,LONG64(GetTickCount64()));
        Wire::Frame frame;if(Wire::ReadFrame(*Session,frame)) {
            if(FrameNonce!=frame.Nonce||AppPid!=frame.AppPid)Open(frame);
            if(WaitForSingleObject(Process,0)!=WAIT_TIMEOUT)throw std::runtime_error("Grand Exchange closed. Return to Helpy and reopen it.");
            const auto slot=HudFrames::Latest(*Page,Ready->GetCompletedValue());
            if(slot>=0){HudGpu::Need(Context->Wait(Ready.Get(),uint64_t(HudFrames::Read(Page->Ready[slot]))),"Companion frame wait failed");Context->CopyResource(Copy.Get(),Pictures[slot].Get());HudGpu::Need(Context->Signal(Done.Get(),++Completed),"Companion completion failed");Context->Flush();HudFrames::Presented(*Page,slot,Completed);FrameAt=GetTickCount64();}
        }
        const auto now=GetTickCount64();
        if(now-FrameAt>20000)throw std::runtime_error("The companion stopped drawing; return to Helpy and reopen it");
        // The first picture waits for a frame at the size asked for, so a frame from GE's own window size never shows.
        if(View&&Completed&&!Shown&&Page&&LONG(Page->Width)==wantWidth&&LONG(Page->Height)==wantHeight)Shown=true;
        if(View&&Completed&&Shown){const auto at=ImGui::GetCursorScreenPos();ImGui::Image(ImTextureRef(reinterpret_cast<ImTextureID>(View.Get())),size);Input(at,size);return RS_PLUGIN_OK;}
        if(!Bound&&now-LastBind>=250){LastBind=now;const auto window=FindWindowW(app.WindowClass,app.WindowTitle);
            if(window){Wire::BindRequest request;request.HostPid=GetCurrentProcessId();strcpy_s(request.Nonce,Nonce.c_str());COPYDATASTRUCT data{Wire::BindMessage,sizeof(request),&request};DWORD_PTR result{};
                if(!SendMessageTimeoutW(window,WM_COPYDATA,reinterpret_cast<WPARAM>(Current.Window),reinterpret_cast<LPARAM>(&data),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&result)||!result)throw std::runtime_error("The companion needs a matching build with Helpy workspace support");Bound=true;}
            else if(!Launched){if(!std::filesystem::is_regular_file(app.Executable))throw std::runtime_error("Grand Exchange companion is missing; repair this plugin");const auto result=ShellExecuteW(Current.Window,L"open",app.Executable,L"--no-handoff --helpy-workspace",std::filesystem::path(app.Executable).parent_path().c_str(),SW_HIDE);if(reinterpret_cast<INT_PTR>(result)<=32)throw std::runtime_error("Cannot start Grand Exchange");Launched=true;}}
        ImGui::TextWrapped("Opening Grand Exchange...");return RS_PLUGIN_BUFFER_TOO_SMALL;
    }
};
inline std::unique_ptr<Surface> Active;
inline int32_t RS_PLUGIN_CALL Draw(void*,const RuneSchemaCompanionV1* app) {
    if(!app||app->StructSize<sizeof(*app)||!app->Executable||!app->WindowClass||!app->WindowTitle||ActiveRoute.empty())return RS_PLUGIN_INVALID_ARGUMENT;
    try{if(!Active)Active=std::make_unique<Surface>();return Active->Draw(ActiveRoute,*app);}
    catch(const std::exception& error){ImGui::TextWrapped("%s",error.what());return RS_PLUGIN_FAILED;}
}
inline void RS_PLUGIN_CALL Return(void*){Current.ReturnHome=true;Active.reset();}
inline bool CapturesInput(){return !ActiveRoute.empty()&&Active&&Active->Interactive();}
// While a companion's workspace is open, Helpy's window is shown at full opacity: Window opacity and
// overlay opacity fade the whole window, which would let the desktop show through the companion's opaque panels.
inline bool Opaque(){return !ActiveRoute.empty()&&Active&&Active->Route()==ActiveRoute;}
}
