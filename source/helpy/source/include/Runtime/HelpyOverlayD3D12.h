#pragma once
#include "Runtime/HelpyOverlayGpu.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <vector>

namespace PS::HelpyOverlayGpu {
inline constexpr char Shader[]=R"(
Texture2D picture:register(t0);SamplerState linearClamp:register(s0);cbuffer Options:register(b0){float opacity;};
struct Vertex{float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
Vertex vs(uint id:SV_VertexID){Vertex v;v.uv=float2((id<<1)&2,id&2);v.position=float4(v.uv*float2(2,-2)+float2(-1,1),0,1);return v;}
float4 ps(Vertex v):SV_TARGET{return picture.SampleLevel(linearClamp,v.uv,0)*opacity;}
)";
inline ComPtr<ID3DBlob> Compile(const char* entry,const char* profile){ComPtr<ID3DBlob> code,error;Need(D3DCompile(Shader,sizeof(Shader),"Helpy overlay",nullptr,nullptr,entry,profile,D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error),"Cannot compile overlay shader");return code;}
class Consumer12 {
    ComPtr<ID3D12Device> Device;ComPtr<ID3D12CommandQueue> Queue;ComPtr<ID3D12RootSignature> Root;ComPtr<ID3D12PipelineState> Pipeline;
    ComPtr<ID3D12DescriptorHeap> Srv,Rtv;ComPtr<ID3D12GraphicsCommandList> List;
    ComPtr<ID3D12Fence> Ready,Done;std::array<ComPtr<ID3D12Resource>,3> Pictures;
    struct Frame{ComPtr<ID3D12Resource> Buffer;};std::vector<Frame> Frames;
    // DXGI permits 16 queued presents; allocators follow submissions rather than back-buffer indices.
    struct Submission{ComPtr<ID3D12CommandAllocator> Allocator;uint64_t Fence=0;};std::array<Submission,18> Submissions;
    HANDLE Mapping=nullptr,App=nullptr;HelpyOverlayFrames::Page* Control=nullptr;uint64_t Sequence=0;UINT RtvStride=0,SrvStride=0;
    static void Barrier(ID3D12GraphicsCommandList* list,ID3D12Resource* resource,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to){D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,from,to};list->ResourceBarrier(1,&barrier);}
public:
    ~Consumer12(){Reset();}
    Consumer12()=default;Consumer12(const Consumer12&)=delete;Consumer12& operator=(const Consumer12&)=delete;
    bool Active()const{return Control!=nullptr;}
    bool Idle()const{return !Done||Done->GetCompletedValue()>=Sequence;}
    void Hide(){if(Control)InterlockedExchange(&Control->Visible,0);}
    void Reset(){
        Hide();if(Control)InterlockedExchange(&Control->Bound,0);Frames.clear();List.Reset();for(auto& submission:Submissions){submission.Allocator.Reset();submission.Fence=0;}Pipeline.Reset();Root.Reset();Rtv.Reset();Srv.Reset();for(auto& picture:Pictures)picture.Reset();Ready.Reset();Done.Reset();Queue.Reset();Device.Reset();
        if(Control){UnmapViewOfFile(Control);Control=nullptr;}if(Mapping){CloseHandle(Mapping);Mapping=nullptr;}if(App){CloseHandle(App);App=nullptr;}Sequence=0;
    }
    void Create(ID3D12Device* device,ID3D12CommandQueue* queue,IDXGISwapChain3* swap,uint32_t app,const std::string& nonce,ID3DBlob* vertex,ID3DBlob* pixel){
        if(!Idle())throw std::runtime_error("Previous overlay frames are still in flight");Reset();Device=device;Queue=queue;
        const auto key=Name(GetCurrentProcessId(),app,nonce);Mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,(key+L".control").c_str());if(!Mapping)throw std::runtime_error("Overlay control page cannot be opened");
        Control=static_cast<HelpyOverlayFrames::Page*>(MapViewOfFile(Mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(HelpyOverlayFrames::Page)));
        if(!Control||!HelpyOverlayFrames::Shape(*Control,GetCurrentProcessId(),app))throw std::runtime_error("Overlay control page is invalid");
        App=OpenProcess(SYNCHRONIZE,FALSE,app);if(!App)throw std::runtime_error("Overlay application is unavailable");
        for(unsigned i=0;i<3;++i){HANDLE handle{};Need(Device->OpenSharedHandleByName((key+L".texture"+std::to_wstring(i)).c_str(),GENERIC_ALL,&handle),"Cannot open overlay texture handle");const auto opened=Device->OpenSharedHandle(handle,IID_PPV_ARGS(&Pictures[i]));CloseHandle(handle);Need(opened,"Cannot open overlay texture");
            const auto desc=Pictures[i]->GetDesc();if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||desc.Width!=Control->Width||desc.Height!=Control->Height||desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||desc.DepthOrArraySize!=1||desc.MipLevels!=1||desc.SampleDesc.Count!=1)throw std::runtime_error("Shared overlay texture shape differs");}
        for(unsigned i=0;i<2;++i){HANDLE handle{};Need(Device->OpenSharedHandleByName((key+(i?L".done":L".ready")).c_str(),GENERIC_ALL,&handle),"Cannot open overlay fence handle");const auto opened=i?Device->OpenSharedHandle(handle,IID_PPV_ARGS(&Done)):Device->OpenSharedHandle(handle,IID_PPV_ARGS(&Ready));CloseHandle(handle);Need(opened,"Cannot open overlay fence");}
        D3D12_DESCRIPTOR_HEAP_DESC heap{};heap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;heap.NumDescriptors=3;heap.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;Need(Device->CreateDescriptorHeap(&heap,IID_PPV_ARGS(&Srv)),"Cannot create overlay texture descriptors");SrvStride=Device->GetDescriptorHandleIncrementSize(heap.Type);
        auto handle=Srv->GetCPUDescriptorHandleForHeapStart();for(auto& picture:Pictures){D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=DXGI_FORMAT_R8G8B8A8_UNORM;view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;view.Texture2D.MipLevels=1;Device->CreateShaderResourceView(picture.Get(),&view,handle);handle.ptr+=SrvStride;}
        DXGI_SWAP_CHAIN_DESC desc{};Need(swap->GetDesc(&desc),"Cannot read game swap chain");if(!desc.BufferCount||desc.BufferCount>8)throw std::runtime_error("Unsupported game back-buffer count");
        const auto targetFormat=TargetFormat(desc.BufferDesc.Format);
        heap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;heap.NumDescriptors=desc.BufferCount;heap.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;Need(Device->CreateDescriptorHeap(&heap,IID_PPV_ARGS(&Rtv)),"Cannot create overlay target descriptors");RtvStride=Device->GetDescriptorHandleIncrementSize(heap.Type);handle=Rtv->GetCPUDescriptorHandleForHeapStart();Frames.resize(desc.BufferCount);
        D3D12_RENDER_TARGET_VIEW_DESC view{};view.Format=targetFormat;view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;
        for(UINT i=0;i<desc.BufferCount;++i){Need(swap->GetBuffer(i,IID_PPV_ARGS(&Frames[i].Buffer)),"Cannot read game back buffer");Device->CreateRenderTargetView(Frames[i].Buffer.Get(),&view,handle);handle.ptr+=RtvStride;}
        for(auto& submission:Submissions)Need(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&submission.Allocator)),"Cannot create overlay allocator");
        D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};D3D12_ROOT_PARAMETER parameters[2]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;parameters[0].DescriptorTable={1,&range};parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;parameters[1].Constants={0,0,1};
        D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC signature{2,parameters,1,&sampler,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> serialized,error;Need(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&error),"Cannot serialize overlay root signature");Need(Device->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&Root)),"Cannot create overlay root signature");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};state.pRootSignature=Root.Get();state.VS={vertex->GetBufferPointer(),vertex->GetBufferSize()};state.PS={pixel->GetBufferPointer(),pixel->GetBufferSize()};state.SampleMask=UINT_MAX;state.SampleDesc.Count=1;state.NumRenderTargets=1;state.RTVFormats[0]=targetFormat;state.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        state.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;state.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;state.RasterizerState.DepthClipEnable=TRUE;state.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
        auto& blend=state.BlendState.RenderTarget[0];blend.BlendEnable=TRUE;blend.SrcBlend=D3D12_BLEND_ONE;blend.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;blend.BlendOp=D3D12_BLEND_OP_ADD;blend.SrcBlendAlpha=D3D12_BLEND_ONE;blend.DestBlendAlpha=D3D12_BLEND_INV_SRC_ALPHA;blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
        Need(Device->CreateGraphicsPipelineState(&state,IID_PPV_ARGS(&Pipeline)),"Cannot create overlay pipeline");Need(Device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,Submissions[0].Allocator.Get(),Pipeline.Get(),IID_PPV_ARGS(&List)),"Cannot create overlay command list");Need(List->Close(),"Cannot close overlay command list");
        InterlockedExchange(&Control->Bound,1);
    }
    HelpyOverlayFrames::Page* Page()const{return Control;}
    bool Draw(IDXGISwapChain3* swap){
        using namespace HelpyOverlayFrames;if(!Control||WaitForSingleObject(App,0)!=WAIT_TIMEOUT||!Alive(*Control,GetTickCount64())){Hide();return false;}
        const auto index=swap->GetCurrentBackBufferIndex();if(index>=Frames.size())return false;auto& frame=Frames[index];
        const auto slot=Latest(*Control,Ready->GetCompletedValue());if(slot<0||slot>=3)return false;
        const auto completed=Done->GetCompletedValue();auto submission=std::find_if(Submissions.begin(),Submissions.end(),[completed](const auto& value){return value.Fence<=completed;});
        if(submission==Submissions.end())throw std::runtime_error("Native overlay GPU submissions exceeded the bounded presentation queue");
        const auto buffer=frame.Buffer->GetDesc();InterlockedExchange(&Control->ViewportWidth,LONG(buffer.Width));InterlockedExchange(&Control->ViewportHeight,LONG(buffer.Height));
        const float left=float(std::min<uint64_t>(Control->Left,buffer.Width)),top=float(std::min(Control->Top,buffer.Height));const float width=float(std::min<uint64_t>(Control->Width,buffer.Width-uint64_t(left))),height=float(std::min(Control->Height,buffer.Height-uint32_t(top)));if(width<=0||height<=0)return false;
        Need(submission->Allocator->Reset(),"Cannot reset overlay allocator");Need(List->Reset(submission->Allocator.Get(),Pipeline.Get()),"Cannot reset overlay command list");
        Barrier(List.Get(),frame.Buffer.Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);Barrier(List.Get(),Pictures[slot].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        const D3D12_VIEWPORT viewport{left,top,width,height,0,1};const D3D12_RECT scissor{LONG(left),LONG(top),LONG(left+width),LONG(top+height)};List->RSSetViewports(1,&viewport);List->RSSetScissorRects(1,&scissor);
        auto rtv=Rtv->GetCPUDescriptorHandleForHeapStart();rtv.ptr+=index*RtvStride;List->OMSetRenderTargets(1,&rtv,FALSE,nullptr);List->SetGraphicsRootSignature(Root.Get());ID3D12DescriptorHeap* heaps[]{Srv.Get()};List->SetDescriptorHeaps(1,heaps);auto srv=Srv->GetGPUDescriptorHandleForHeapStart();srv.ptr+=slot*SrvStride;List->SetGraphicsRootDescriptorTable(0,srv);
        const float opacity=std::clamp(Read(Control->Opacity)/10000.f,.6f,1.f);List->SetGraphicsRoot32BitConstants(1,1,&opacity,0);List->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);List->DrawInstanced(3,1,0,0);
        Barrier(List.Get(),Pictures[slot].Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);Barrier(List.Get(),frame.Buffer.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);Need(List->Close(),"Cannot close overlay draw");
        ID3D12CommandList* lists[]{List.Get()};Queue->ExecuteCommandLists(1,lists);const auto sequence=++Sequence;Need(Queue->Signal(Done.Get(),sequence),"Cannot signal overlay completion");submission->Fence=sequence;
        Presented(*Control,slot,sequence);return true;
    }
};
}
