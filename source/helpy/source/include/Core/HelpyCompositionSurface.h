#pragma once
#include <d3d11.h>
#include <dxgi1_3.h>
#include <dcomp.h>
#include <wrl/client.h>
#include <algorithm>

namespace PS {
class HelpyCompositionSurface {
    Microsoft::WRL::ComPtr<IDCompositionDevice> m_device;
    Microsoft::WRL::ComPtr<IDCompositionTarget> m_target;
    Microsoft::WRL::ComPtr<IDCompositionVisual> m_visual;
    Microsoft::WRL::ComPtr<IDCompositionEffectGroup> m_effect;
    float m_opacity=-1.f;
public:
    ~HelpyCompositionSurface(){Reset();}
    bool Active()const{return m_visual!=nullptr;}
    float AppliedOpacity()const{return m_opacity;}
    bool Create(ID3D11Device* device,HWND window,IDXGISwapChain** output){
        using Microsoft::WRL::ComPtr;Reset();ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory2> factory;ComPtr<IDXGISwapChain1> swap;
        if(FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgi)))||FAILED(dxgi->GetAdapter(&adapter))||FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))))return false;
        RECT area{};if(!GetClientRect(window,&area))return false;
        DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=std::max(1L,area.right);desc.Height=std::max(1L,area.bottom);desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.Scaling=DXGI_SCALING_STRETCH;
        desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
        if(FAILED(factory->CreateSwapChainForComposition(device,&desc,nullptr,&swap))||FAILED(DCompositionCreateDevice(dxgi.Get(),IID_PPV_ARGS(&m_device)))
            ||FAILED(m_device->CreateTargetForHwnd(window,TRUE,&m_target))||FAILED(m_device->CreateVisual(&m_visual))||FAILED(m_device->CreateEffectGroup(&m_effect))||FAILED(m_visual->SetEffect(m_effect.Get()))
            ||FAILED(m_visual->SetContent(swap.Get()))||FAILED(m_target->SetRoot(m_visual.Get()))||FAILED(m_device->Commit())){Reset();return false;}
        ComPtr<IDXGIDevice1> latency;if(SUCCEEDED(dxgi.As(&latency)))latency->SetMaximumFrameLatency(1);
        factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER);
        SetWindowLongPtrW(window,GWL_EXSTYLE,(GetWindowLongPtrW(window,GWL_EXSTYLE)&~WS_EX_LAYERED)|WS_EX_NOREDIRECTIONBITMAP);
        *output=swap.Detach();return true;
    }
    void Opacity(float value){value=std::clamp(value,0.f,1.f);if(m_effect&&value!=m_opacity&&SUCCEEDED(m_effect->SetOpacity(value))&&SUCCEEDED(m_device->Commit()))m_opacity=value;}
    void Reset(){if(m_target)m_target->SetRoot(nullptr);if(m_device)m_device->Commit();m_visual.Reset();m_effect.Reset();m_target.Reset();m_device.Reset();m_opacity=-1.f;}
};
}
