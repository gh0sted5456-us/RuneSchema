#pragma once
#include "HelpyOverlayFrames.h"
#include "HelpyDesktopTransport.h"
#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <string>
#include <stdexcept>
#include <memory>
#include <new>

namespace PS::HelpyOverlayGpu {
using Microsoft::WRL::ComPtr;
inline void Need(HRESULT result,const char* message){if(FAILED(result))throw std::runtime_error(std::string(message)+" ("+std::to_string(uint32_t(result))+")");}
inline DXGI_FORMAT TargetFormat(DXGI_FORMAT format){
    // Helpy's shared pixels are already display encoded; an sRGB target would brighten them a second time.
    switch(format){
    case DXGI_FORMAT_R8G8B8A8_UNORM:case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM:case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_R10G10B10A2_UNORM:return format;
    default:throw std::runtime_error("Native overlay cannot render back-buffer format "+std::to_string(unsigned(format))+"; use an SDR display format");
    }
}
inline std::wstring Name(uint32_t game,uint32_t app,const std::string& nonce){
    if(!game||!app||nonce.size()!=32||nonce.find_first_not_of("0123456789abcdef")!=nonce.npos)throw std::runtime_error("Invalid overlay surface identity");
    const auto text="Local\\RuneSchema.HelpyOverlay."+std::to_string(game)+"."+std::to_string(app)+"."+nonce;return {text.begin(),text.end()};
}
struct Producer {
    ComPtr<ID3D11Device5> Device;ComPtr<ID3D11DeviceContext4> Context;
    std::array<ComPtr<ID3D11Texture2D>,3> Textures;std::array<ComPtr<ID3D11RenderTargetView>,3> Targets;
    ComPtr<ID3D11Fence> Ready,Done;std::array<HANDLE,5> Shared{};HANDLE Mapping{};
    HelpyOverlayFrames::Page* Page{};uint64_t Sequence=0;std::wstring Key;
    ~Producer(){Reset();}
    Producer()=default;Producer(const Producer&)=delete;Producer& operator=(const Producer&)=delete;
    void Reset(){
        if(Page){InterlockedExchange(&Page->Visible,0);UnmapViewOfFile(Page);Page=nullptr;}
        if(Mapping){CloseHandle(Mapping);Mapping=nullptr;}for(auto& handle:Shared)if(handle){CloseHandle(handle);handle=nullptr;}
        for(auto& target:Targets)target.Reset();for(auto& texture:Textures)texture.Reset();Ready.Reset();Done.Reset();Context.Reset();Device.Reset();Sequence=0;Key.clear();
    }
    void Create(ID3D11Device* device,uint32_t game,const std::string& nonce,uint32_t width,uint32_t height){
        Reset();Key=Name(game,GetCurrentProcessId(),nonce);if(width<160||height<120||width>HelpyOverlayFrames::MaxWidth||height>HelpyOverlayFrames::MaxHeight)throw std::runtime_error("Overlay surface exceeds its dimensions");
        Need(device->QueryInterface(IID_PPV_ARGS(&Device)),"Shared overlay fences require Direct3D 11.4");ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);Need(context.As(&Context),"Overlay context fences unavailable");
        PSECURITY_DESCRIPTOR descriptor{};if(!HelpyDesktop::SameUserPipeSecurity(descriptor))throw std::runtime_error("Overlay user security unavailable");
        const auto freeSecurity=[](void* value){if(value)LocalFree(value);};std::unique_ptr<void,decltype(freeSecurity)> security(descriptor,freeSecurity);SECURITY_ATTRIBUTES attributes{sizeof(attributes),descriptor,FALSE};
        Mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,&attributes,PAGE_READWRITE,0,sizeof(HelpyOverlayFrames::Page),(Key+L".control").c_str());
        if(!Mapping||GetLastError()==ERROR_ALREADY_EXISTS)throw std::runtime_error("Overlay surface already exists");
        Page=static_cast<HelpyOverlayFrames::Page*>(MapViewOfFile(Mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(HelpyOverlayFrames::Page)));if(!Page)throw std::runtime_error("Overlay control page unavailable");
        new(Page) HelpyOverlayFrames::Page;Page->GamePid=game;Page->AppPid=GetCurrentProcessId();Page->Width=width;Page->Height=height;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
        desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;desc.MiscFlags=D3D11_RESOURCE_MISC_SHARED|D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
        for(unsigned i=0;i<3;++i){Need(Device->CreateTexture2D(&desc,nullptr,&Textures[i]),"Cannot create shared overlay texture");Need(Device->CreateRenderTargetView(Textures[i].Get(),nullptr,&Targets[i]),"Cannot create overlay target");ComPtr<IDXGIResource1> resource;Need(Textures[i].As(&resource),"Overlay resource sharing unavailable");Need(resource->CreateSharedHandle(&attributes,DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE,(Key+L".texture"+std::to_wstring(i)).c_str(),&Shared[i]),"Cannot share overlay texture");}
        Need(Device->CreateFence(0,D3D11_FENCE_FLAG_SHARED,IID_PPV_ARGS(&Ready)),"Cannot create overlay ready fence");Need(Device->CreateFence(0,D3D11_FENCE_FLAG_SHARED,IID_PPV_ARGS(&Done)),"Cannot create overlay completion fence");
        Need(Ready->CreateSharedHandle(&attributes,GENERIC_ALL,(Key+L".ready").c_str(),&Shared[3]),"Cannot share overlay ready fence");Need(Done->CreateSharedHandle(&attributes,GENERIC_ALL,(Key+L".done").c_str(),&Shared[4]),"Cannot share overlay completion fence");
        Page->ReadyHandle=uint64_t(reinterpret_cast<uintptr_t>(Shared[3]));Page->DoneHandle=uint64_t(reinterpret_cast<uintptr_t>(Shared[4]));
        for(unsigned i=0;i<3;++i)Page->TextureHandles[i]=uint64_t(reinterpret_cast<uintptr_t>(Shared[i]));
    }
    int Begin(){return Page?HelpyOverlayFrames::Writable(*Page,Done->GetCompletedValue()):-1;}
    void Publish(int slot){if(!Page||slot<0||slot>=3)return;const auto sequence=++Sequence;Need(Context->Signal(Ready.Get(),sequence),"Cannot signal overlay frame");Context->Flush();InterlockedExchange64(&Page->Ready[slot],sequence);InterlockedExchange(&Page->Published,slot);InterlockedExchange(&Page->Owner[slot],0);}
};
}
