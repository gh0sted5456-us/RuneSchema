#pragma once

#include "Generator/ToolRequest.h"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <imgui.h>
#include <imgui_internal.h>

namespace PS::ItemIconCache {
namespace detail {
    struct CacheState {
        std::unordered_map<std::string, ImTextureData*> Textures;
        std::unordered_set<std::string> Failed;
        std::string Pending;
        std::chrono::steady_clock::time_point LastRequest{};
    };

    // Deliberately process-lifetime storage. UE4SS owns the ImGui renderer and
    // may outlive a hot-reloaded RuneSchema DLL. Keeping registered texture
    // descriptors alive is safer than leaving dangling UserTextures pointers.
    inline CacheState& State() {
        static auto* state=new CacheState();
        return *state;
    }

    inline bool BackendSupportsTextures() {
        // The UE4SS-hosted ImGui renderer can register user textures even when
        // BackendFlags_RendererHasTextures is not advertised. Requiring the flag
        // prevented icon requests from ever starting on some builds.
        return ImGui::GetCurrentContext()!=nullptr;
    }

    inline bool ReadThumbnail(const std::filesystem::path& file,int& width,int& height,std::vector<unsigned char>& pixels) {
        std::ifstream input(file,std::ios::binary|std::ios::ate);
        if(!input)return false;
        const auto size=input.tellg();
        if(size<16 || size>16+256*256*4)return false;
        input.seekg(0);
        char magic[4]{};uint32_t w=0,h=0,bytes=0;
        input.read(magic,4);
        input.read(reinterpret_cast<char*>(&w),sizeof(w));
        input.read(reinterpret_cast<char*>(&h),sizeof(h));
        input.read(reinterpret_cast<char*>(&bytes),sizeof(bytes));
        if(!input || std::string(magic,4)!="RSI1" || w<1 || h<1 || w>256 || h>256
            || bytes!=w*h*4 || size!=static_cast<std::streamoff>(16+bytes))return false;
        pixels.resize(bytes);input.read(reinterpret_cast<char*>(pixels.data()),bytes);
        if(!input)return false;width=static_cast<int>(w);height=static_cast<int>(h);return true;
    }

    inline bool Install(const std::string& iconPath,const std::filesystem::path& file) {
        auto& state=State();
        if(state.Textures.contains(iconPath))return true;
        if(state.Textures.size()>=512)return false;
        int width=0,height=0;std::vector<unsigned char> pixels;
        if(!ReadThumbnail(file,width,height,pixels) || !BackendSupportsTextures())return false;
        auto* texture=IM_NEW(ImTextureData)();
        texture->Create(ImTextureFormat_RGBA32,width,height);
        std::memcpy(texture->GetPixels(),pixels.data(),pixels.size());
        texture->UseColors=true;
        texture->RefCount=1;
        texture->SetStatus(ImTextureStatus_WantCreate);
        ImGui::RegisterUserTexture(texture);
        state.Textures.emplace(iconPath,texture);
        return true;
    }
}

inline void Update() {
    auto& state=detail::State();
    for(auto& [_,texture]:state.Textures)
        if(texture && texture->Status==ImTextureStatus_OK && texture->Pixels)
            texture->DestroyPixels();
    if(state.Pending.empty())return;
    const auto result=ItemIconRequests::Read();
    const auto status=result.value("Status",std::string{});
    if(status.starts_with("World changed")){state.Pending.clear();state.Failed.clear();return;}
    if(result.value("Icon",std::string{})!=state.Pending)return;
    if(status=="Ready") {
        const auto file=result.value("File",std::string{});
        if(file.empty() || !detail::Install(state.Pending,std::filesystem::path(file)))state.Failed.insert(state.Pending);
    } else if(status=="Error") state.Failed.insert(state.Pending);
    else return;
    state.Pending.clear();
}

inline ImTextureData* Get(const std::string& iconPath) {
    const auto& textures=detail::State().Textures;
    const auto found=textures.find(iconPath);return found==textures.end()?nullptr:found->second;
}

inline bool Failed(const std::string& iconPath) {
    return detail::State().Failed.contains(iconPath);
}

inline void Queue(const std::string& iconPath) {
    if(iconPath.empty() || !detail::BackendSupportsTextures())return;
    auto& state=detail::State();
    const auto now=std::chrono::steady_clock::now();
    if(state.Textures.contains(iconPath) || state.Failed.contains(iconPath) || !state.Pending.empty()
        || state.Textures.size()>=512 || !ItemIconRequests::Available.load()
        || (state.LastRequest.time_since_epoch().count()!=0 && now-state.LastRequest<std::chrono::milliseconds(100)))return;
    state.Pending=iconPath;state.LastRequest=now;
    ItemIconRequests::Submit({{"Icon",iconPath}});
}

inline bool Draw(const std::string& iconPath,const ImVec2& minimum,const ImVec2& maximum) {
    if(auto* texture=Get(iconPath)) {
        ImGui::GetWindowDrawList()->AddImage(texture->GetTexRef(),minimum,maximum);
        return true;
    }
    Queue(iconPath);return false;
}

inline size_t Count() {return detail::State().Textures.size();}
}
