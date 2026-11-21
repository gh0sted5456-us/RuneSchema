#include "Generator/HelpyCompanionSurface.h"
#include <cassert>
#include <cstdio>

namespace Host=PS::HelpyCompanionSurface;
namespace Wire=PS::CompanionSurface;
namespace Frames=PS::HudFrames;
using Microsoft::WRL::ComPtr;
static HANDLE Mapping{};
static Wire::Session* Session{};
static LRESULT CALLBACK Companion(HWND window,UINT message,WPARAM sender,LPARAM value) {
    if(message!=WM_COPYDATA)return DefWindowProcW(window,message,sender,value);
    const auto* data=reinterpret_cast<COPYDATASTRUCT*>(value);
    if(data->dwData!=Wire::BindMessage||data->cbData!=sizeof(Wire::BindRequest))return FALSE;
    const auto& request=*static_cast<Wire::BindRequest*>(data->lpData);assert(Wire::Valid(request));
    Mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,Wire::Name(request.HostPid,request.Nonce).c_str());assert(Mapping);
    Session=static_cast<Wire::Session*>(MapViewOfFile(Mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Wire::Session)));assert(Session);
    return TRUE;
}
int main() {
    ComPtr<ID3D11Device> device,producerDevice;ComPtr<ID3D11DeviceContext> context,producerContext;
    const auto make=[](auto& device,auto& context){return D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context);};
    assert(SUCCEEDED(make(device,context))&&SUCCEEDED(make(producerDevice,producerContext)));
    ComPtr<IDXGIDevice> dxgi;assert(SUCCEEDED(device.As(&dxgi)));ComPtr<IDXGIAdapter> adapter;assert(SUCCEEDED(dxgi->GetAdapter(&adapter)));
    DXGI_ADAPTER_DESC hardware{};assert(SUCCEEDED(adapter->GetDesc(&hardware)));std::wprintf(L"GPU fixture: %ls\n",hardware.Description);
    WNDCLASSW hostClass{};hostClass.hInstance=GetModuleHandleW(nullptr);hostClass.lpszClassName=L"GrandExchange.SurfaceFixture.Host";hostClass.lpfnWndProc=DefWindowProcW;assert(RegisterClassW(&hostClass));
    WNDCLASSW childClass=hostClass;childClass.lpszClassName=L"GrandExchange.SurfaceFixture.Child";childClass.lpfnWndProc=Companion;assert(RegisterClassW(&childClass));
    const auto host=CreateWindowW(hostClass.lpszClassName,L"Grand Exchange synthetic host",0,0,0,960,640,nullptr,nullptr,hostClass.hInstance,nullptr);
    const auto child=CreateWindowW(childClass.lpszClassName,L"Grand Exchange synthetic companion",0,0,0,960,640,nullptr,nullptr,childClass.hInstance,nullptr);assert(host&&child);
    Host::Current={host,device.Get(),context.Get(),false};Host::ActiveRoute="Fixture.exchange";
    RuneSchemaCompanionV1 app{sizeof(app),L"unused-synthetic-fixture",childClass.lpszClassName,L"Grand Exchange synthetic companion"};
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DeltaTime=1.f/60;io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures;
    PS::HudGpu::Producer producer;unsigned generation{};uint32_t width{},height{};bool image=false,mouse=false;
    const auto step=[&](ImVec2 dimensions,bool sendInput,bool publish) {
        io.DisplaySize=dimensions;io.AddMousePosEvent(100,100);io.AddMouseButtonEvent(0,sendInput);
        ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(dimensions);
        ImGui::Begin("host",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove);
        const auto status=Host::Draw(nullptr,&app);assert(status==RS_PLUGIN_OK||status==RS_PLUGIN_BUFFER_TOO_SMALL);
        if(Host::CapturesInput()) {
            image=true;
            auto* list=ImGui::GetWindowDrawList();ID3D11ShaderResourceView* view{};
            for(const auto& command:list->CmdBuffer)if(!command.TexRef._TexData&&command.GetTexID())view=reinterpret_cast<ID3D11ShaderResourceView*>(command.GetTexID());
            assert(view);ComPtr<ID3D11Resource> resource;view->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;assert(SUCCEEDED(resource.As(&texture)));
            D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
            ComPtr<ID3D11Texture2D> read;assert(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&read)));context->CopyResource(read.Get(),texture.Get());
            D3D11_MAPPED_SUBRESOURCE pixels{};assert(SUCCEEDED(context->Map(read.Get(),0,D3D11_MAP_READ,0,&pixels)));
            const auto* rgba=static_cast<unsigned char*>(pixels.pData);assert(rgba[0]==51&&rgba[1]==102&&rgba[2]==153&&rgba[3]==255);context->Unmap(read.Get(),0);
        }
        ImGui::End();ImGui::Render();
        if(!Session||Host::Current.ReturnHome)return;
        const auto nextWidth=uint32_t(Frames::Read(Session->RequestedWidth)),nextHeight=uint32_t(Frames::Read(Session->RequestedHeight));
        if(!producer.Page||nextWidth!=width||nextHeight!=height) {
            width=nextWidth;height=nextHeight;char nonce[33]{};sprintf_s(nonce,"%032x",++generation);producer.Create(producerDevice.Get(),GetCurrentProcessId(),nonce,width,height);
            Wire::Frame frame;frame.AppPid=GetCurrentProcessId();frame.Width=width;frame.Height=height;strcpy_s(frame.Nonce,nonce);Wire::Publish(*Session,frame);
        }
        Frames::Input input;while(Frames::Pop(*producer.Page,input))if(input.Kind==Frames::InputKind::Button&&input.Code==WM_LBUTTONDOWN)mouse=true;
        if(publish){const auto slot=producer.Begin();if(slot>=0){const float color[]{.2f,.4f,.6f,1};producerContext->ClearRenderTargetView(producer.Targets[slot].Get(),color);producer.Publish(slot);
            const auto ready=CreateEventW(nullptr,FALSE,FALSE,nullptr);assert(ready);assert(SUCCEEDED(producer.Ready->SetEventOnCompletion(producer.Sequence,ready)));assert(WaitForSingleObject(ready,5000)==WAIT_OBJECT_0);CloseHandle(ready);}}
    };
    for(const ImVec2 size: {ImVec2{960,640},ImVec2{1440,900},ImVec2{800,600}}) {
        image=false;for(int i=0;i<8;++i)step(size,i>=5,true);assert(image);
        image=false;step(size,false,false);step(size,false,false);assert(image); // Retain the last picture between publications.
    }
    assert(mouse&&generation==3);
    InterlockedExchange(&Session->ReturnRequested,1);step({800,600},false,false);assert(Host::Current.ReturnHome);
    Host::Active.reset();Host::ActiveRoute.clear();producer.Reset();UnmapViewOfFile(Session);CloseHandle(Mapping);Session=nullptr;
    ImGui::DestroyContext();DestroyWindow(child);DestroyWindow(host);
    std::puts("companion GPU fixture passed: shared pixels, resize, retained frame, input and return");
}
