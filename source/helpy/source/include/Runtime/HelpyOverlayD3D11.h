#pragma once
#include "Runtime/HelpyOverlayD3D12.h"

namespace PS::HelpyOverlayGpu {
class Consumer11 {
    ComPtr<ID3D11Device5> Device;ComPtr<ID3D11DeviceContext4> Immediate;
    ComPtr<ID3D11DeviceContext> Deferred;ComPtr<ID3D11VertexShader> Vertex;ComPtr<ID3D11PixelShader> Pixel;
    ComPtr<ID3D11BlendState> Blend;ComPtr<ID3D11DepthStencilState> Depth;ComPtr<ID3D11RasterizerState> Raster;
    ComPtr<ID3D11SamplerState> Sampler;ComPtr<ID3D11Buffer> Options;
    std::array<ComPtr<ID3D11ShaderResourceView>,3> Pictures;
    ComPtr<ID3D11Fence> Ready,Done;HANDLE Mapping=nullptr,App=nullptr;
    HelpyOverlayFrames::Page* Control=nullptr;uint64_t Sequence=0;
public:
    ~Consumer11(){Reset();}
    bool Active()const{return Control!=nullptr;}
    bool Idle()const{return !Done||Done->GetCompletedValue()>=Sequence;}
    void Hide(){if(Control)InterlockedExchange(&Control->Visible,0);}
    HelpyOverlayFrames::Page* Page()const{return Control;}
    void Reset(){
        Hide();if(Control)InterlockedExchange(&Control->Bound,0);for(auto& picture:Pictures)picture.Reset();Options.Reset();Sampler.Reset();Blend.Reset();Depth.Reset();Raster.Reset();Pixel.Reset();Vertex.Reset();Deferred.Reset();Immediate.Reset();Ready.Reset();Done.Reset();Device.Reset();
        if(Control){UnmapViewOfFile(Control);Control=nullptr;}if(Mapping){CloseHandle(Mapping);Mapping=nullptr;}if(App){CloseHandle(App);App=nullptr;}Sequence=0;
    }
    void Create(ID3D11Device* device,uint32_t app,const std::string& nonce,ID3DBlob* vertex,ID3DBlob* pixel){
        if(!Idle())throw std::runtime_error("Previous overlay frames are still in flight");Reset();
        Need(device->QueryInterface(IID_PPV_ARGS(&Device)),"DX11 native overlay requires shared-fence support");ComPtr<ID3D11DeviceContext> immediate;device->GetImmediateContext(&immediate);Need(immediate.As(&Immediate),"DX11 native context fences unavailable");
        const auto key=Name(GetCurrentProcessId(),app,nonce);Mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,(key+L".control").c_str());if(!Mapping)throw std::runtime_error("Overlay control page cannot be opened");
        Control=static_cast<HelpyOverlayFrames::Page*>(MapViewOfFile(Mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(HelpyOverlayFrames::Page)));
        if(!Control||!HelpyOverlayFrames::Shape(*Control,GetCurrentProcessId(),app))throw std::runtime_error("Overlay control page is invalid");
        App=OpenProcess(SYNCHRONIZE|PROCESS_DUP_HANDLE,FALSE,app);if(!App)throw std::runtime_error("Overlay application is unavailable");
        for(unsigned i=0;i<3;++i){ComPtr<ID3D11Texture2D> picture;Need(Device->OpenSharedResourceByName((key+L".texture"+std::to_wstring(i)).c_str(),DXGI_SHARED_RESOURCE_READ,IID_PPV_ARGS(&picture)),"Cannot open DX11 overlay texture");
            D3D11_TEXTURE2D_DESC desc{};picture->GetDesc(&desc);if(desc.Width!=Control->Width||desc.Height!=Control->Height||desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||desc.ArraySize!=1||desc.MipLevels!=1||desc.SampleDesc.Count!=1)throw std::runtime_error("DX11 shared overlay texture shape differs");
            Need(Device->CreateShaderResourceView(picture.Get(),nullptr,&Pictures[i]),"Cannot create DX11 overlay picture view");}
        for(unsigned i=0;i<2;++i){HANDLE handle{};const auto source=reinterpret_cast<HANDLE>(uintptr_t(i?Control->DoneHandle:Control->ReadyHandle));
            if(!DuplicateHandle(App,source,GetCurrentProcess(),&handle,0,FALSE,DUPLICATE_SAME_ACCESS))throw std::runtime_error("Cannot duplicate overlay fence handle");
            const auto opened=i?Device->OpenSharedFence(handle,IID_PPV_ARGS(&Done)):Device->OpenSharedFence(handle,IID_PPV_ARGS(&Ready));CloseHandle(handle);Need(opened,"Cannot open DX11 overlay fence");}
        Need(Device->CreateDeferredContext(0,&Deferred),"Cannot create DX11 overlay command context");
        Need(Device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&Vertex),"Cannot create DX11 overlay vertex shader");Need(Device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&Pixel),"Cannot create DX11 overlay pixel shader");
        D3D11_BLEND_DESC blend{};auto& target=blend.RenderTarget[0];target.BlendEnable=TRUE;target.SrcBlend=target.SrcBlendAlpha=D3D11_BLEND_ONE;target.DestBlend=target.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;target.BlendOp=target.BlendOpAlpha=D3D11_BLEND_OP_ADD;target.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;Need(Device->CreateBlendState(&blend,&Blend),"Cannot create DX11 overlay blend");
        D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthFunc=D3D11_COMPARISON_ALWAYS;Need(Device->CreateDepthStencilState(&depth,&Depth),"Cannot create DX11 overlay depth state");D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;Need(Device->CreateRasterizerState(&raster,&Raster),"Cannot create DX11 overlay raster state");
        D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampler.MaxLOD=D3D11_FLOAT32_MAX;sampler.ComparisonFunc=D3D11_COMPARISON_ALWAYS;Need(Device->CreateSamplerState(&sampler,&Sampler),"Cannot create DX11 overlay sampler");D3D11_BUFFER_DESC options{16,D3D11_USAGE_DEFAULT,D3D11_BIND_CONSTANT_BUFFER};Need(Device->CreateBuffer(&options,nullptr,&Options),"Cannot create DX11 overlay options");
        InterlockedExchange(&Control->Bound,1);
    }
    bool Draw(IDXGISwapChain* swap){
        using namespace HelpyOverlayFrames;if(!Control||WaitForSingleObject(App,0)!=WAIT_TIMEOUT||!Alive(*Control,GetTickCount64())){Hide();return false;}
        const auto slot=Latest(*Control,Ready->GetCompletedValue());if(slot<0||slot>=3)return false;
        ComPtr<ID3D11Texture2D> buffer;Need(swap->GetBuffer(0,IID_PPV_ARGS(&buffer)),"Cannot read DX11 game back buffer");D3D11_TEXTURE2D_DESC desc{};buffer->GetDesc(&desc);
        const auto targetFormat=TargetFormat(desc.Format);
        InterlockedExchange(&Control->ViewportWidth,desc.Width);InterlockedExchange(&Control->ViewportHeight,desc.Height);
        const float left=float(std::min(Control->Left,desc.Width)),top=float(std::min(Control->Top,desc.Height));const float width=float(std::min(Control->Width,desc.Width-UINT(left))),height=float(std::min(Control->Height,desc.Height-UINT(top)));if(width<=0||height<=0)return false;
        D3D11_RENDER_TARGET_VIEW_DESC view{};view.Format=targetFormat;view.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
        ComPtr<ID3D11RenderTargetView> target;Need(Device->CreateRenderTargetView(buffer.Get(),&view,&target),"Cannot create DX11 overlay target");
        const float opacity[4]{std::clamp(Read(Control->Opacity)/10000.f,.6f,1.f)};Deferred->ClearState();Deferred->UpdateSubresource(Options.Get(),0,nullptr,opacity,0,0);
        ID3D11RenderTargetView* targets[]{target.Get()};Deferred->OMSetRenderTargets(1,targets,nullptr);Deferred->OMSetBlendState(Blend.Get(),nullptr,UINT_MAX);Deferred->OMSetDepthStencilState(Depth.Get(),0);Deferred->RSSetState(Raster.Get());
        const D3D11_VIEWPORT viewport{left,top,width,height,0,1};Deferred->RSSetViewports(1,&viewport);Deferred->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);Deferred->VSSetShader(Vertex.Get(),nullptr,0);Deferred->PSSetShader(Pixel.Get(),nullptr,0);ID3D11ShaderResourceView* pictures[]{Pictures[slot].Get()};Deferred->PSSetShaderResources(0,1,pictures);ID3D11SamplerState* samplers[]{Sampler.Get()};Deferred->PSSetSamplers(0,1,samplers);ID3D11Buffer* options[]{Options.Get()};Deferred->PSSetConstantBuffers(0,1,options);Deferred->Draw(3,0);
        ComPtr<ID3D11CommandList> commands;Need(Deferred->FinishCommandList(FALSE,&commands),"Cannot finish DX11 overlay commands");Immediate->ExecuteCommandList(commands.Get(),TRUE);const auto sequence=++Sequence;Need(Immediate->Signal(Done.Get(),sequence),"Cannot signal DX11 overlay completion");Immediate->Flush();
        Presented(*Control,slot,sequence);return true;
    }
};
}
