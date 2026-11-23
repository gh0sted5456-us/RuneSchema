#pragma once
// Home tile and sidebar icons for plugin tabs. A plugin may ship one PNG in its
// own folder (RuneSchemaNavTabV1::IconImage); otherwise its short Icon text is drawn.
#include "Core/HelpyPluginNavigation.h"
#include "Core/RuneSchemaDataRoot.h"
#include <imgui.h>
#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <fstream>
#include <map>
#include <vector>

namespace PS::HelpyPluginIcons {
inline constexpr uintmax_t MaxFileBytes = 1024 * 1024;
inline constexpr UINT MaxSide = 256;
struct Entry { Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> View; bool Failed = false; };
inline ID3D11Device* Device = nullptr; // set by the window each frame
inline std::map<std::wstring, Entry> Cache;
inline void Release() { Cache.clear(); }
inline ID3D11ShaderResourceView* Load(const std::filesystem::path& path) {
    if (path.empty() || !Device) return nullptr;
    auto& entry = Cache[path.wstring()];
    if (entry.View || entry.Failed) return entry.View.Get();
    entry.Failed = true; // one attempt per path until the device is recreated
    try {
        RuneSchemaData::Safe(path);
        std::error_code error;
        const auto bytes = std::filesystem::file_size(path, error);
        if (error || bytes == 0 || bytes > MaxFileBytes) return nullptr;
        std::vector<BYTE> data(static_cast<size_t>(bytes));
        std::ifstream file(path, std::ios::binary);
        if (!file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()))) return nullptr;
        using Microsoft::WRL::ComPtr;
        ComPtr<IWICImagingFactory> wic; ComPtr<IWICStream> stream; ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICBitmapFrameDecode> frame; ComPtr<IWICFormatConverter> converter;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)))
            || FAILED(wic->CreateStream(&stream)) || FAILED(stream->InitializeFromMemory(data.data(), static_cast<DWORD>(data.size())))
            || FAILED(wic->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder))) return nullptr;
        GUID container{};
        if (FAILED(decoder->GetContainerFormat(&container)) || container != GUID_ContainerFormatPng) return nullptr;
        UINT width = 0, height = 0;
        if (FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&width, &height))
            || !width || !height || width > MaxSide || height > MaxSide) return nullptr;
        if (FAILED(wic->CreateFormatConverter(&converter)) || FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return nullptr;
        std::vector<BYTE> pixels(static_cast<size_t>(width) * height * 4);
        if (FAILED(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data()))) return nullptr;
        D3D11_TEXTURE2D_DESC info{};
        info.Width = width; info.Height = height; info.MipLevels = info.ArraySize = 1; info.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        info.SampleDesc.Count = 1; info.Usage = D3D11_USAGE_IMMUTABLE; info.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        const D3D11_SUBRESOURCE_DATA initial{pixels.data(), width * 4, 0};
        ComPtr<ID3D11Texture2D> texture;
        if (FAILED(Device->CreateTexture2D(&info, &initial, &texture)) || FAILED(Device->CreateShaderResourceView(texture.Get(), nullptr, &entry.View))) return nullptr;
        entry.Failed = false;
        return entry.View.Get();
    } catch (...) { return nullptr; }
}
// Draws the tab's icon centered at `center` within a square of `size`. A PNG is tinted
// like Helpy's own icons unless the plugin asked for full colour.
inline void Draw(ImDrawList* draw, const HelpyPluginNavigation::Tab& tab, ImVec2 center, float size, ImVec4 tint) {
    if (auto* view = Load(tab.IconPath)) {
        const ImVec4 color = (tab.IconFlags & RS_NAV_ICON_FULL_COLOR) ? ImVec4(1, 1, 1, tint.w) : tint;
        draw->AddImage(ImTextureRef(reinterpret_cast<ImTextureID>(view)), {center.x - size * .5f, center.y - size * .5f},
            {center.x + size * .5f, center.y + size * .5f}, {0, 0}, {1, 1}, ImGui::GetColorU32(color));
        return;
    }
    std::string glyph = tab.Icon;
    if (glyph.empty() && !tab.Title.empty()) glyph = tab.Title.substr(0, 1);
    if (glyph.empty()) return;
    auto* font = ImGui::GetFont();
    const float fontSize = size * (glyph.size() > 2 ? .42f : .62f);
    const auto extent = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, glyph.c_str());
    draw->AddText(font, fontSize, {center.x - extent.x * .5f, center.y - extent.y * .5f}, ImGui::GetColorU32(tint), glyph.c_str());
}
}
