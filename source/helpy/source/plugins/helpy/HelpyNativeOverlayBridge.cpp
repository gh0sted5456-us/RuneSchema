#include "Runtime/HelpyNativeOverlayBridge.h"
#include "Runtime/HelpyOverlayD3D11.h"
#include "Runtime/HelpyOverlayOpenGL.h"
#include "Runtime/HelpyOverlayVulkan.h"
#include <vulkan/vk_layer.h>
#include <map>
#include <safetyhook.hpp>
#include <xinput.h>
#include <mutex>
#include <thread>
#include <atomic>

namespace PS::HelpyNativeOverlayBridge {
namespace {
using namespace HelpyOverlayGpu;using Json=nlohmann::json;
struct VulkanConnection {VkDevice Device{};VkQueue Queue{};uint32_t Family=0;PFN_vkGetDeviceProcAddr Get{};VkSwapchainKHR Swap{};VkExtent2D Extent{};VkFormat Format{};std::vector<VkImage> Images;VkPhysicalDeviceMemoryProperties Memory{};LUID Adapter{};};
struct State {
    std::mutex Gate;std::jthread Worker;std::atomic<bool> Started=false,Stopping=false,Suspended=false,Ready=false,Prepared=false;
    std::atomic<unsigned> Calls=0;std::atomic<bool> Capturing=false,Drawing=false;std::string Error="Native overlay has not been prepared";
    SafetyHookInline Present,Present1,Resize,Resize1,CreateSwap,CreateHwnd,Execute,Barriers,EnhancedBarriers,Clip,Warp,Cursor,Shape,SwapGl,SwapWgl;std::array<SafetyHookInline,3> Pads;
    ComPtr<IDXGISwapChain3> Swap;ComPtr<ID3D12Device> Device;ComPtr<ID3D12CommandQueue> Queue;
    std::array<ID3D12Resource*,8> Buffers{};std::vector<ID3D12CommandList*> PresentLists;ComPtr<ID3DBlob> Vertex,Pixel;Consumer12 Renderer;Consumer11 Renderer11;ConsumerGL RendererGL;ConsumerVulkan RendererVk;std::shared_ptr<VulkanConnection> Vulkan;ComPtr<ID3D11Device> VulkanVerifier;std::atomic<uint32_t> PendingApp=0;std::string PendingNonce;LUID GlAdapter{};ComPtr<ID3D11Device> Device11;std::string Api;
    HWND Window=nullptr;WNDPROC Previous=nullptr;HCURSOR PreviousCursor=nullptr;int CursorAdds=0;std::atomic<bool> InputActive=false,HardwareCursor=false;std::atomic<int> CursorIndex=0;std::array<HCURSOR,11> Cursors{};
    uint32_t Width=0,Height=0;bool Exclusive=false;
    HelpyOverlayFrames::Page* Page(){return Api=="Vulkan"?RendererVk.Page():Api=="OpenGL"?RendererGL.Page():Device11?Renderer11.Page():Renderer.Page();}
    void Hide(){Renderer.Hide();Renderer11.Hide();RendererGL.Hide();RendererVk.Hide();}
    bool Idle(){return Renderer.Idle()&&Renderer11.Idle()&&RendererVk.Idle();}
    void Reset(){Renderer.Reset();Renderer11.Reset();if(Api=="OpenGL")RendererGL.Reset();if(Api=="Vulkan"){if(RendererVk.Active()&&Vulkan){const auto wait=reinterpret_cast<PFN_vkQueueWaitIdle>(Vulkan->Get(Vulkan->Device,"vkQueueWaitIdle"));wait(Vulkan->Queue);}RendererVk.Reset();}}
};
// Trampolines and callback code stay mapped for the game's lifetime, including an in-flight shutdown callback.
State& Shared(){static auto* state=new State;return *state;}
struct Callback {State& Value;Callback():Value(Shared()){++Value.Calls;}~Callback(){--Value.Calls;}};
bool GameWindow(HWND window){wchar_t name[64]{};DWORD pid=0;GetWindowThreadProcessId(window,&pid);return pid==GetCurrentProcessId()&&GetClassNameW(window,name,64)&&std::wstring_view(name)==L"UnrealWindow";}
bool Captured(State& state){return !state.Stopping&&state.Capturing&&GetForegroundWindow()==state.Window;}
HCURSOR CurrentCursor(State& state){const auto index=state.CursorIndex.load();return index<0?nullptr:state.Cursors[size_t(index)<state.Cursors.size()?size_t(index):0];}
void CursorInput(State& state,bool active){
    if(state.InputActive==active)return;state.InputActive=active;
    if(active){if(state.Clip)state.Clip.call<BOOL>(static_cast<const RECT*>(nullptr));else ClipCursor(nullptr);
        bool visible=false;if(state.Cursor)for(int i=0;i<32;++i){++state.CursorAdds;if(state.Cursor.call<int>(TRUE)>=0){visible=true;break;}}
        state.HardwareCursor=visible&&state.Cursors[0];state.PreviousCursor=state.Shape?state.Shape.call<HCURSOR>(state.HardwareCursor?CurrentCursor(state):nullptr):SetCursor(nullptr);
        CURSORINFO cursor{sizeof(cursor)};if(state.HardwareCursor&&CurrentCursor(state)&&GetCursorInfo(&cursor)&&!(cursor.flags&CURSOR_SHOWING)){state.HardwareCursor=false;if(state.Shape)state.Shape.call<HCURSOR>(nullptr);}
        if(auto* page=state.Page())InterlockedExchange(&page->CursorActive,state.HardwareCursor?1:0);
        if(state.Previous)for(UINT key=1;key<256;++key)if(GetAsyncKeyState(key)&0x8000)CallWindowProcW(state.Previous,state.Window,WM_KEYUP,key,LPARAM(0xc0000001));
    }else{state.HardwareCursor=false;if(auto* page=state.Page())InterlockedExchange(&page->CursorActive,0);if(state.Shape)state.Shape.call<HCURSOR>(state.PreviousCursor);else SetCursor(state.PreviousCursor);state.PreviousCursor=nullptr;if(state.Cursor)while(state.CursorAdds>0){--state.CursorAdds;state.Cursor.call<int>(FALSE);}}
}
LRESULT CALLBACK Input(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
    Callback callback;auto& state=callback.Value;
    if(!state.Drawing&&!state.Capturing&&!state.InputActive&&message!=WM_APP+0x5a1)return state.Previous?CallWindowProcW(state.Previous,window,message,wParam,lParam):DefWindowProcW(window,message,wParam,lParam);
    const bool input=(message>=WM_KEYFIRST&&message<=WM_KEYLAST)||(message>=WM_MOUSEFIRST&&message<=WM_MOUSELAST)||message==WM_INPUT||message==WM_SETCURSOR;
    if(!input&&message!=WM_APP+0x5a1&&message!=WM_ACTIVATEAPP&&message!=WM_NCDESTROY)return state.Previous?CallWindowProcW(state.Previous,window,message,wParam,lParam):DefWindowProcW(window,message,wParam,lParam);
    if(Captured(state)){
        if(message==WM_INPUT)return DefWindowProcW(window,message,wParam,lParam);
        if(message==WM_MOUSEMOVE)return 0; // The app samples the latest pointer once per UI frame.
        if(message==WM_SETCURSOR){if(state.Shape)state.Shape.call<HCURSOR>(state.HardwareCursor?CurrentCursor(state):nullptr);return TRUE;}
    }
    std::unique_lock lock(state.Gate,std::try_to_lock);
    if(!lock){if(Captured(state)){if(message==WM_INPUT)return DefWindowProcW(window,message,wParam,lParam);if((message>=WM_KEYFIRST&&message<=WM_KEYLAST)||(message>=WM_MOUSEFIRST&&message<=WM_MOUSELAST))return 0;}return state.Previous?CallWindowProcW(state.Previous,window,message,wParam,lParam):DefWindowProcW(window,message,wParam,lParam);}
    auto* page=state.Page();const bool active=!state.Stopping&&page&&HelpyOverlayFrames::Alive(*page,GetTickCount64())&&GetForegroundWindow()==window;state.Capturing=active;
    CursorInput(state,active);
    if(active&&page){state.CursorIndex=HelpyOverlayFrames::Read(page->CursorShape);if(message==WM_APP+0x5a1&&state.Shape)state.Shape.call<HCURSOR>(state.HardwareCursor?CurrentCursor(state):nullptr);}
    if(active&&page){
        using namespace HelpyOverlayFrames;PS::HelpyOverlayFrames::Input input;bool send=false;
        switch(message){
        case WM_KEYDOWN:case WM_SYSKEYDOWN:case WM_KEYUP:case WM_SYSKEYUP:input={InputKind::Key,uint32_t(wParam),0,0,uint32_t(lParam)};input.X=((GetKeyState(VK_CONTROL)&0x8000)?1:0)|((GetKeyState(VK_SHIFT)&0x8000)?2:0)|((GetKeyState(VK_MENU)&0x8000)?4:0)|(((GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&0x8000)?8:0);send=true;break;
        case WM_CHAR:input={InputKind::Character,uint32_t(wParam)};send=true;break;
        case WM_MOUSEMOVE:return 0;
        case WM_LBUTTONDOWN:case WM_LBUTTONUP:case WM_RBUTTONDOWN:case WM_RBUTTONUP:case WM_MBUTTONDOWN:case WM_MBUTTONUP:
            input={InputKind::Button,message,int16_t(LOWORD(lParam)),int16_t(HIWORD(lParam)),uint32_t(wParam)};send=true;break;
        case WM_XBUTTONDOWN:case WM_XBUTTONUP:input={InputKind::Button,message,int16_t(LOWORD(lParam)),int16_t(HIWORD(lParam)),uint32_t(wParam)};send=true;break;
        case WM_MOUSEWHEEL:case WM_MOUSEHWHEEL:input={InputKind::Wheel,message,0,int16_t(HIWORD(wParam))};send=true;break;
        case WM_INPUT:return DefWindowProcW(window,message,wParam,lParam);
        case WM_SETCURSOR:if(state.Shape)state.Shape.call<HCURSOR>(state.HardwareCursor?CurrentCursor(state):nullptr);return TRUE;
        }
        if(send){Push(*page,input);return 0;}
    }
    if(message==WM_ACTIVATEAPP&&!wParam){if(page)InterlockedExchange(&page->Visible,0);CursorInput(state,false);}
    if(message==WM_NCDESTROY){if(page)InterlockedExchange(&page->Visible,0);CursorInput(state,false);}
    return state.Previous?CallWindowProcW(state.Previous,window,message,wParam,lParam):DefWindowProcW(window,message,wParam,lParam);
}
BOOL WINAPI Clip(const RECT* area){auto& state=Shared();return Captured(state)?TRUE:state.Clip.call<BOOL>(area);}
BOOL WINAPI Warp(int x,int y){auto& state=Shared();return Captured(state)?TRUE:state.Warp.call<BOOL>(x,y);}
int WINAPI Cursor(BOOL show){auto& state=Shared();return Captured(state)?(show?0:-1):state.Cursor.call<int>(show);}
HCURSOR WINAPI Shape(HCURSOR cursor){auto& state=Shared();return state.Shape.call<HCURSOR>(Captured(state)?(state.HardwareCursor?CurrentCursor(state):nullptr):cursor);}
template<class Function> void Hook(SafetyHookInline& hook,void* target,Function callback){
    if(hook){if(!hook.enable())throw std::runtime_error("Cannot re-enable native overlay hook");return;}
    auto created=safetyhook::InlineHook::create(target,reinterpret_cast<void*>(callback),safetyhook::InlineHook::StartDisabled);
    if(!created)throw std::runtime_error("Native overlay hook was refused");hook=std::move(*created);if(!hook.enable())throw std::runtime_error("Cannot enable native overlay hook");
}
void BeforePresent(IDXGISwapChain* swap){
    auto& state=Shared();if(state.Stopping||state.Suspended||(state.Ready&&!state.Drawing))return;std::unique_lock lock(state.Gate,std::defer_lock);if(state.Drawing)lock.lock();else if(!lock.try_lock())return;
    try{
        if(state.Swap&&state.Swap.Get()!=swap){
            DXGI_SWAP_CHAIN_DESC replacement{};if(FAILED(swap->GetDesc(&replacement))||!GameWindow(replacement.OutputWindow)||(replacement.OutputWindow!=state.Window&&IsWindow(state.Window)))return;
            state.Hide();state.Capturing=false;state.Drawing=false;state.Ready=false;if(state.Window)PostMessageW(state.Window,WM_APP+0x5a1,0,0);if(!state.Idle())return;
            state.Reset();state.Swap.Reset();state.Device.Reset();state.Device11.Reset();state.Queue.Reset();state.Buffers.fill(nullptr);state.PresentLists.clear();state.Api.clear();if(!IsWindow(state.Window))state.Previous=nullptr;
        }
        if(!state.Swap){DXGI_SWAP_CHAIN_DESC desc{};if(FAILED(swap->GetDesc(&desc))||!GameWindow(desc.OutputWindow))return;
            ComPtr<ID3D12Device> device;if(FAILED(swap->GetDevice(IID_PPV_ARGS(&device)))){Need(swap->GetDevice(IID_PPV_ARGS(&state.Device11)),"This swap chain does not expose a DX11 or DX12 renderer");state.Api="D3D11";}else state.Api="D3D12";
            Need(swap->QueryInterface(IID_PPV_ARGS(&state.Swap)),"Native swap chain 3 unavailable");state.Device=device;state.Window=desc.OutputWindow;state.Width=desc.BufferDesc.Width;state.Height=desc.BufferDesc.Height;
            BOOL exclusive=FALSE;swap->GetFullscreenState(&exclusive,nullptr);state.Exclusive=exclusive!=FALSE;
        }
        if(state.Swap.Get()!=swap)return;
        if(state.Device11){state.Ready=true;state.Error.clear();}
        else {
            if(!state.Queue)state.Error="Waiting to verify the queue that presents the game's back buffer";
            else {state.Ready=true;state.Error.clear();}
            if(!state.Buffers[0]){DXGI_SWAP_CHAIN_DESC desc{};Need(swap->GetDesc(&desc),"Cannot read DX12 swap-chain buffers");if(desc.BufferCount>state.Buffers.size())throw std::runtime_error("Too many DX12 game buffers");for(UINT i=0;i<desc.BufferCount;++i){ComPtr<ID3D12Resource> buffer;Need(swap->GetBuffer(i,IID_PPV_ARGS(&buffer)),"Cannot inspect DX12 game buffer identity");state.Buffers[i]=buffer.Get();}}
        }
        if(state.Page()){
            auto* page=state.Page();if(GetForegroundWindow()!=state.Window)InterlockedExchange(&page->Visible,0);
            if(state.Device11)state.Renderer11.Draw(state.Swap.Get());else state.Renderer.Draw(state.Swap.Get());
            const bool active=HelpyOverlayFrames::Alive(*page,GetTickCount64())&&GetForegroundWindow()==state.Window;
            if(state.Capturing.exchange(active)!=active)PostMessageW(state.Window,WM_APP+0x5a1,0,0);
            if(!active)state.Drawing=false;
        }
    }catch(const std::exception& error){state.Error=error.what();state.Stopping=true;state.Hide();}
}
void BeforeGlSwap(HDC dc){
    auto& state=Shared();if(state.Stopping||state.Suspended||!state.Started||(!state.Drawing&&state.Ready&&!state.PendingApp)||!wglGetCurrentContext())return;
    const auto window=WindowFromDC(dc);if(!GameWindow(window))return;std::unique_lock lock(state.Gate,std::defer_lock);if(state.Drawing)lock.lock();else if(!lock.try_lock())return;
    try{
        if(!state.Api.empty()&&state.Api!="OpenGL")return;
        if(!state.Ready){state.RendererGL.Load();state.GlAdapter=state.RendererGL.Adapter();state.Api="OpenGL";state.Window=window;state.Ready=true;state.Error.clear();}
        RECT area{};if(!GetClientRect(window,&area))return;
        if(state.RendererGL.Active()&&(state.Width!=uint32_t(area.right)||state.Height!=uint32_t(area.bottom))){state.RendererGL.Reset();state.Capturing=false;state.Drawing=false;PostMessageW(window,WM_APP+0x5a1,0,0);}
        state.Width=area.right;state.Height=area.bottom;
        if(state.PendingApp){const auto app=state.PendingApp.load();state.PendingApp=0;state.RendererGL.Create(app,state.PendingNonce);}
        if(auto* page=state.Page()){
            if(GetForegroundWindow()!=window)InterlockedExchange(&page->Visible,0);
            if(state.Drawing)state.RendererGL.Draw(state.Width,state.Height);
            const bool active=HelpyOverlayFrames::Alive(*page,GetTickCount64())&&GetForegroundWindow()==window;
            if(state.Capturing.exchange(active)!=active)PostMessageW(window,WM_APP+0x5a1,0,0);if(!active)state.Drawing=false;
        }
    }catch(const std::exception& error){state.Error=error.what();state.Stopping=true;state.Hide();state.Capturing=false;state.Drawing=false;if(state.Window)PostMessageW(state.Window,WM_APP+0x5a1,0,0);}
}
thread_local unsigned GlSwapDepth=0;
BOOL WINAPI SwapGl(HDC dc){Callback callback;const bool outer=GlSwapDepth++==0;if(outer)BeforeGlSwap(dc);const auto result=callback.Value.SwapGl.call<BOOL>(dc);--GlSwapDepth;return result;}
BOOL WINAPI SwapWgl(HDC dc){Callback callback;const bool outer=GlSwapDepth++==0;if(outer)BeforeGlSwap(dc);const auto result=callback.Value.SwapWgl.call<BOOL>(dc);--GlSwapDepth;return result;}
thread_local unsigned PresentDepth=0;
HRESULT WINAPI Present(IDXGISwapChain* swap,UINT interval,UINT flags){Callback callback;const bool outer=PresentDepth++==0;if(outer&&!(flags&DXGI_PRESENT_TEST))BeforePresent(swap);const auto result=callback.Value.Present.call<HRESULT>(swap,interval,flags);--PresentDepth;return result;}
HRESULT WINAPI Present1(IDXGISwapChain1* swap,UINT interval,UINT flags,const DXGI_PRESENT_PARAMETERS* parameters){
    Callback callback;const bool outer=PresentDepth++==0;if(outer&&!(flags&DXGI_PRESENT_TEST))BeforePresent(swap);
    // The game's dirty rectangles do not include changes to Helpy or its cursor.
    DXGI_PRESENT_PARAMETERS full{};if(outer&&Captured(callback.Value)&&callback.Value.Swap.Get()==swap)parameters=&full;
    const auto result=callback.Value.Present1.call<HRESULT>(swap,interval,flags,parameters);--PresentDepth;return result;
}
HRESULT WINAPI Resize(IDXGISwapChain* swap,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags){
    Callback callback;auto& state=callback.Value;{
        std::lock_guard lock(state.Gate);if(state.Swap.Get()==swap){state.Hide();if(!state.Idle()){for(unsigned i=0;i<500&&!state.Idle();++i)Sleep(1);if(!state.Idle())return DXGI_ERROR_WAS_STILL_DRAWING;}state.Reset();state.Buffers.fill(nullptr);state.PresentLists.clear();state.Width=width;state.Height=height;}
    }const auto result=state.Resize.call<HRESULT>(swap,count,width,height,format,flags);if(SUCCEEDED(result)){std::lock_guard lock(state.Gate);DXGI_SWAP_CHAIN_DESC desc{};if(state.Swap.Get()==swap&&SUCCEEDED(swap->GetDesc(&desc))){state.Width=desc.BufferDesc.Width;state.Height=desc.BufferDesc.Height;}}return result;
}
bool BeforeSwapCreation(HWND window){
    auto& state=Shared();std::lock_guard lock(state.Gate);if(!state.Swap||window!=state.Window)return true;
    state.Hide();state.Capturing=false;state.Drawing=false;state.Ready=false;PostMessageW(window,WM_APP+0x5a1,0,0);
    for(unsigned i=0;i<500&&!state.Idle();++i)Sleep(1);if(!state.Idle())return false;
    state.Reset();state.Swap.Reset();state.Device.Reset();state.Device11.Reset();state.Queue.Reset();state.Buffers.fill(nullptr);state.PresentLists.clear();state.Api.clear();return true;
}
void CreatedSwap(HWND window,IUnknown* device){
    auto& state=Shared();if(state.Stopping||!GameWindow(window))return;std::lock_guard lock(state.Gate);if(state.Window&&state.Window!=window)return;ComPtr<ID3D12CommandQueue> queue;if(SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&queue)))){state.Queue=queue;state.Window=window;}
}
HRESULT WINAPI CreateSwap(IDXGIFactory* factory,IUnknown* device,DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** output){
    auto& state=Shared();if(desc&&!BeforeSwapCreation(desc->OutputWindow))return DXGI_ERROR_WAS_STILL_DRAWING;
    const auto result=state.CreateSwap.call<HRESULT>(factory,device,desc,output);if(SUCCEEDED(result)&&desc)CreatedSwap(desc->OutputWindow,device);return result;
}
HRESULT WINAPI CreateHwnd(IDXGIFactory2* factory,IUnknown* device,HWND window,const DXGI_SWAP_CHAIN_DESC1* desc,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,IDXGIOutput* restrictOutput,IDXGISwapChain1** output){
    auto& state=Shared();if(!BeforeSwapCreation(window))return DXGI_ERROR_WAS_STILL_DRAWING;
    const auto result=state.CreateHwnd.call<HRESULT>(factory,device,window,desc,fullscreen,restrictOutput,output);if(SUCCEEDED(result))CreatedSwap(window,device);return result;
}
void RecordPresentList(State& state,ID3D12CommandList* list){if(state.PresentLists.size()<64&&std::find(state.PresentLists.begin(),state.PresentLists.end(),list)==state.PresentLists.end())state.PresentLists.push_back(list);}
void WINAPI Barriers(ID3D12GraphicsCommandList* list,UINT count,const D3D12_RESOURCE_BARRIER* barriers){
    auto& state=Shared();if(!state.Ready&&!state.Stopping&&!state.Suspended){std::unique_lock lock(state.Gate,std::try_to_lock);if(lock&&state.Swap)for(UINT i=0;i<count;++i)if(barriers[i].Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION&&barriers[i].Transition.StateAfter==D3D12_RESOURCE_STATE_PRESENT&&std::find(state.Buffers.begin(),state.Buffers.end(),barriers[i].Transition.pResource)!=state.Buffers.end())RecordPresentList(state,list);}
    state.Barriers.call<void>(list,count,barriers);
}
void WINAPI EnhancedBarriers(ID3D12GraphicsCommandList7* list,UINT count,const D3D12_BARRIER_GROUP* groups){
    auto& state=Shared();if(!state.Ready&&!state.Stopping&&!state.Suspended){std::unique_lock lock(state.Gate,std::try_to_lock);if(lock&&state.Swap)for(UINT i=0;i<count;++i)if(groups[i].Type==D3D12_BARRIER_TYPE_TEXTURE)for(UINT j=0;j<groups[i].NumBarriers;++j){const auto& barrier=groups[i].pTextureBarriers[j];if(barrier.LayoutAfter==D3D12_BARRIER_LAYOUT_PRESENT&&std::find(state.Buffers.begin(),state.Buffers.end(),barrier.pResource)!=state.Buffers.end())RecordPresentList(state,list);}}
    state.EnhancedBarriers.call<void>(list,count,groups);
}
void WINAPI Execute(ID3D12CommandQueue* queue,UINT count,ID3D12CommandList* const* lists){
    Callback callback;auto& state=callback.Value;
    if(!state.Ready&&!state.Stopping&&!state.Suspended&&queue->GetDesc().Type==D3D12_COMMAND_LIST_TYPE_DIRECT){std::unique_lock lock(state.Gate,std::try_to_lock);if(lock)for(UINT i=0;i<count;++i)if(const auto found=std::find(state.PresentLists.begin(),state.PresentLists.end(),lists[i]);found!=state.PresentLists.end()){
        ComPtr<ID3D12Device> device;if(SUCCEEDED(queue->GetDevice(IID_PPV_ARGS(&device)))&&device.Get()==state.Device.Get())state.Queue=queue;state.PresentLists.erase(found);
    }}state.Execute.call<void>(queue,count,lists);
}
HRESULT WINAPI Resize1(IDXGISwapChain3* swap,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags,const UINT* masks,IUnknown* const* queues){
    auto& state=Shared();{std::lock_guard lock(state.Gate);if(state.Swap.Get()==swap){state.Hide();for(unsigned i=0;i<500&&!state.Idle();++i)Sleep(1);if(!state.Idle())return DXGI_ERROR_WAS_STILL_DRAWING;state.Reset();state.Buffers.fill(nullptr);state.PresentLists.clear();state.Queue.Reset();state.Ready=false;}}
    const auto result=state.Resize1.call<HRESULT>(swap,count,width,height,format,flags,masks,queues);if(SUCCEEDED(result)){std::lock_guard lock(state.Gate);DXGI_SWAP_CHAIN_DESC desc{};if(state.Swap.Get()==swap&&SUCCEEDED(swap->GetDesc(&desc))){state.Width=desc.BufferDesc.Width;state.Height=desc.BufferDesc.Height;}}return result;
}
template<unsigned Slot> DWORD WINAPI Pad(DWORD index,XINPUT_STATE* output){
    auto& state=Shared();const auto result=state.Pads[Slot].call<DWORD>(index,output);if(result!=ERROR_SUCCESS||!output||!Captured(state))return result;
    std::unique_lock lock(state.Gate,std::try_to_lock);if(lock)if(auto* page=state.Page()){
        InterlockedIncrement(&page->PadSequence);std::memcpy(page->Pad.data(),&output->Gamepad,sizeof(output->Gamepad));InterlockedIncrement(&page->PadSequence);
    }
    // A held pad must deliver a changed packet when capture starts and ends.
    output->dwPacketNumber^=0x80000000u;output->Gamepad={};return result;
}
void Prepare(State& state){
    try{
        HMODULE pinned{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&Prepare),&pinned);
        if(auto library=GetModuleHandleW(L"opengl32.dll")){auto swap=GetProcAddress(GetModuleHandleW(L"gdi32.dll"),"SwapBuffers");auto wgl=GetProcAddress(library,"wglSwapBuffers");if(swap)Hook(state.SwapGl,reinterpret_cast<void*>(swap),SwapGl);if(wgl&&wgl!=swap)Hook(state.SwapWgl,reinterpret_cast<void*>(wgl),SwapWgl);}
        state.Vertex=Compile("vs","vs_5_0");state.Pixel=Compile("ps","ps_5_0");
        const auto window=CreateWindowExW(0,L"STATIC",L"RuneSchema private render probe",WS_OVERLAPPEDWINDOW,0,0,32,32,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window)throw std::runtime_error("Cannot create native renderer probe");
        const auto destroy=[](HWND value){DestroyWindow(value);};std::unique_ptr<std::remove_pointer_t<HWND>,decltype(destroy)> probe(window,destroy);
        ComPtr<ID3D11Device> device11;ComPtr<ID3D11DeviceContext> context;ComPtr<IDXGISwapChain> swap;
        DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=window;desc.SampleDesc.Count=1;desc.Windowed=TRUE;
        Need(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&device11,nullptr,&context),"Cannot prepare presentation hook");
        ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory2> factory;Need(device11.As(&dxgi),"Cannot inspect DXGI swap-chain factory");Need(dxgi->GetAdapter(&adapter),"Cannot inspect DXGI adapter");Need(adapter->GetParent(IID_PPV_ARGS(&factory)),"Cannot inspect DXGI factory");auto** factoryMethods=*reinterpret_cast<void***>(factory.Get());Hook(state.CreateSwap,factoryMethods[10],CreateSwap);Hook(state.CreateHwnd,factoryMethods[15],CreateHwnd);
        ComPtr<ID3D12Device> device12;ComPtr<ID3D12CommandQueue> queue;if(SUCCEEDED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device12)))){D3D12_COMMAND_QUEUE_DESC queueDesc{};queueDesc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;if(SUCCEEDED(device12->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue)))){auto** methods=*reinterpret_cast<void***>(queue.Get());Hook(state.Execute,methods[10],Execute);ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;Need(device12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"Cannot prepare DX12 barrier verification");Need(device12->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"Cannot prepare DX12 command verification");auto** listMethods=*reinterpret_cast<void***>(list.Get());Hook(state.Barriers,listMethods[26],Barriers);ComPtr<ID3D12GraphicsCommandList7> enhanced;if(SUCCEEDED(list.As(&enhanced))){auto** enhancedMethods=*reinterpret_cast<void***>(enhanced.Get());Hook(state.EnhancedBarriers,enhancedMethods[80],EnhancedBarriers);}Need(list->Close(),"Cannot close DX12 barrier probe");}}
        ComPtr<IDXGISwapChain1> swap1;Need(swap.As(&swap1),"Cannot prepare presentation 1 hook");auto** methods=*reinterpret_cast<void***>(swap.Get());auto** methods1=*reinterpret_cast<void***>(swap1.Get());
        Hook(state.Present,methods[8],Present);Hook(state.Present1,methods1[22],Present1);Hook(state.Resize,methods[13],Resize);ComPtr<IDXGISwapChain3> swap3;if(SUCCEEDED(swap.As(&swap3))){auto** methods3=*reinterpret_cast<void***>(swap3.Get());Hook(state.Resize1,methods3[39],Resize1);}
        std::array<const wchar_t*,3> libraries{L"xinput1_4.dll",L"xinput1_3.dll",L"xinput9_1_0.dll"};std::array<void*,3> callbacks{reinterpret_cast<void*>(Pad<0>),reinterpret_cast<void*>(Pad<1>),reinterpret_cast<void*>(Pad<2>)};
        for(unsigned i=0;i<3;++i)if(auto library=GetModuleHandleW(libraries[i]))if(auto address=GetProcAddress(library,"XInputGetState")){bool duplicate=false;for(unsigned j=0;j<i;++j)duplicate|=state.Pads[j]&&state.Pads[j].target_address()==reinterpret_cast<uintptr_t>(address);if(!duplicate)Hook(state.Pads[i],reinterpret_cast<void*>(address),callbacks[i]);}
        const std::array<LPCWSTR,11> cursors{IDC_ARROW,IDC_IBEAM,IDC_SIZEALL,IDC_SIZENS,IDC_SIZEWE,IDC_SIZENESW,IDC_SIZENWSE,IDC_HAND,IDC_WAIT,IDC_APPSTARTING,IDC_NO};for(size_t i=0;i<cursors.size();++i)state.Cursors[i]=LoadCursorW(nullptr,cursors[i]);
        Hook(state.Clip,reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"ClipCursor")),Clip);Hook(state.Warp,reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetCursorPos")),Warp);Hook(state.Cursor,reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"ShowCursor")),Cursor);Hook(state.Shape,reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetCursor")),Shape);
        state.Prepared=true;
    }catch(const std::exception& error){std::lock_guard lock(state.Gate);state.Error=error.what();state.Stopping=true;}
}
#include "HelpyVulkanLayer.inl"
}
Json Call(const Json& request){
    auto& state=Shared();const auto action=request.value("Action",std::string{});
    if(action=="Prepare")state.Suspended=false;
    if(action=="Suspend")state.Suspended=true;
    if(action=="Prepare"&&!state.Started.exchange(true)){state.Stopping=false;state.Worker=std::jthread([&state]{Prepare(state);});}
    std::lock_guard lock(state.Gate);
    if(action=="Open"){
        if(!state.Ready||!state.Prepared||state.Stopping)return {{"Ready",false},{"Error",state.Error}};
        state.Capturing=false;
        if(state.Api=="Vulkan"){
            if(!state.Vulkan)throw std::runtime_error("Vulkan swap chain is no longer active");
            const auto& vk=*state.Vulkan;
            if(!state.VulkanVerifier){ComPtr<IDXGIFactory1> factory;Need(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"Vulkan/DXGI adapter matching unavailable");ComPtr<IDXGIAdapter1> gpu;for(UINT i=0;SUCCEEDED(factory->EnumAdapters1(i,&gpu));++i){DXGI_ADAPTER_DESC1 desc{};gpu->GetDesc1(&desc);if(desc.AdapterLuid.LowPart==vk.Adapter.LowPart&&desc.AdapterLuid.HighPart==vk.Adapter.HighPart)break;gpu.Reset();}if(!gpu)throw std::runtime_error("Vulkan GPU cannot be matched to the shared-texture producer");Need(D3D11CreateDevice(gpu.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&state.VulkanVerifier,nullptr,nullptr),"Vulkan shared-texture verifier unavailable");}
            if(state.RendererVk.Active()){const auto wait=reinterpret_cast<PFN_vkQueueWaitIdle>(vk.Get(vk.Device,"vkQueueWaitIdle"));NeedVk(wait(vk.Queue),"Cannot drain previous Vulkan overlay attachment");}
            state.RendererVk.Create(vk.Device,vk.Get,vk.Queue,vk.Family,vk.Memory,vk.Extent,vk.Format,vk.Images,state.VulkanVerifier.Get(),request.at("AppPid").get<uint32_t>(),request.at("Nonce").get<std::string>());
        }
        else if(state.Api=="OpenGL"){state.PendingApp=request.at("AppPid").get<uint32_t>();state.PendingNonce=request.at("Nonce").get<std::string>();Name(GetCurrentProcessId(),state.PendingApp,state.PendingNonce);}
        else if(state.Device11)state.Renderer11.Create(state.Device11.Get(),request.at("AppPid").get<uint32_t>(),request.at("Nonce").get<std::string>(),state.Vertex.Get(),state.Pixel.Get());
        else state.Renderer.Create(state.Device.Get(),state.Queue.Get(),state.Swap.Get(),request.at("AppPid").get<uint32_t>(),request.at("Nonce").get<std::string>(),state.Vertex.Get(),state.Pixel.Get());
        if(!state.Previous){SetLastError(0);state.Previous=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(state.Window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(Input)));if(!state.Previous)throw std::runtime_error("Cannot acquire the game window's overlay input route");}
    }
    if(action=="Show")state.Drawing=true;
    if(action=="Hide"){state.Hide();state.Capturing=false;state.Drawing=false;if(state.Window)PostMessageW(state.Window,WM_APP+0x5a1,0,0);}
    auto adapter=state.Api=="Vulkan"&&state.Vulkan?state.Vulkan->Adapter:state.Api=="OpenGL"?state.GlAdapter:state.Device?state.Device->GetAdapterLuid():LUID{};if(state.Device11){ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> gpu;DXGI_ADAPTER_DESC desc{};if(SUCCEEDED(state.Device11.As(&dxgi))&&SUCCEEDED(dxgi->GetAdapter(&gpu))&&SUCCEEDED(gpu->GetDesc(&desc)))adapter=desc.AdapterLuid;}
    DXGI_SWAP_CHAIN_DESC desc{};
    if(state.Swap){state.Swap->GetDesc(&desc);BOOL exclusive=FALSE;if(SUCCEEDED(state.Swap->GetFullscreenState(&exclusive,nullptr)))state.Exclusive=exclusive!=FALSE;}
    auto* page=state.Page();
    return {{"Ready",state.Ready.load()&&state.Prepared.load()&&!state.Stopping.load()},{"Failed",state.Stopping.load()},{"Error",state.Error},{"Api",state.Api},{"Attached",page&&HelpyOverlayFrames::Read(page->Bound)==1},{"Visible",page&&HelpyOverlayFrames::Read(page->Visible)==1},{"Presented",page&&HelpyOverlayFrames::Read(page->Reading)>=0},{"BackBufferFormat",unsigned(desc.BufferDesc.Format)},{"Width",state.Width},{"Height",state.Height},{"Window",uint64_t(reinterpret_cast<uintptr_t>(state.Window))},{"Exclusive",state.Exclusive},{"AdapterLow",adapter.LowPart},{"AdapterHigh",adapter.HighPart}};
}
VkResult Negotiate(VkNegotiateLayerInterface* version){
    if(!version||version->loaderLayerInterfaceVersion<2)return VK_ERROR_INITIALIZATION_FAILED;version->loaderLayerInterfaceVersion=2;version->pfnGetInstanceProcAddr=VulkanLayer::GetInstance;version->pfnGetDeviceProcAddr=VulkanLayer::GetDevice;version->pfnGetPhysicalDeviceProcAddr=nullptr;return VK_SUCCESS;
}
void Shutdown() noexcept{
    auto& state=Shared();state.Stopping=true;if(state.Worker.joinable())state.Worker.join();state.Capturing=false;if(state.Window){DWORD_PTR ignored{};SendMessageTimeoutW(state.Window,WM_APP+0x5a1,0,0,SMTO_ABORTIFHUNG,500,&ignored);}
    std::lock_guard lock(state.Gate);state.Hide();state.Capturing=false;CursorInput(state,false);
    for(auto* hook:{&state.Present,&state.Present1,&state.Resize,&state.Resize1,&state.CreateSwap,&state.CreateHwnd,&state.Execute,&state.Barriers,&state.EnhancedBarriers,&state.Clip,&state.Warp,&state.Cursor,&state.Shape,&state.SwapGl,&state.SwapWgl})if(*hook)(void)hook->disable();
    if(state.Window&&state.Previous&&reinterpret_cast<WNDPROC>(GetWindowLongPtrW(state.Window,GWLP_WNDPROC))==Input)SetWindowLongPtrW(state.Window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(state.Previous));
    for(auto& hook:state.Pads)if(hook)(void)hook.disable();
    if(state.Idle())state.Reset();state.Ready=false;
}
}

VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* version){return PS::HelpyNativeOverlayBridge::Negotiate(version);}


#pragma comment(linker, "/EXPORT:vkNegotiateLoaderLayerInterfaceVersion")
