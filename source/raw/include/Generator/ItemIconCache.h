#pragma once

#include "Generator/ToolRequest.h"
#include "Generator/HelpyIconTrace.h"
#include "Generator/HelpyRuntimePreferences.h"
#include "Runtime/HostServices.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <functional>
#include <fstream>
#include <format>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <imgui.h>
#include <imgui_internal.h>

namespace PS::ItemIconCache {
inline std::function<bool(const std::string&)> RemoteSubmit;
inline std::function<bool(nlohmann::json&)> RemoteRead;
inline void ConfigureRemote(std::function<bool(const std::string&)> submit,std::function<bool(nlohmann::json&)> read){RemoteSubmit=std::move(submit);RemoteRead=std::move(read);}
inline void ClearRemote(){RemoteSubmit={};RemoteRead={};}
namespace detail {
    constexpr std::size_t MaxActiveRemote=4;
    constexpr std::size_t MaxQueued=96;
    constexpr std::size_t MaxResidentTextures=256;
    constexpr int ProtectRecentFrames=8;
    struct CacheState {
        std::unordered_map<std::string,ImTextureData*> Textures;
        std::vector<ImTextureData*> RetiredTextures;
        std::unordered_set<std::string> Failed,Queued,Pending,DiskChecked,ReadyTraced;
        std::unordered_map<std::string,int> WantedFrame,Attempts;
        std::deque<std::string> High,Normal;
        std::uint64_t Generation=0;
        int CacheMode=-1;
        std::chrono::steady_clock::time_point NextSubmit{};
    };
    inline CacheState& State(){static auto* state=new CacheState();return *state;}
    inline bool BackendSupportsTextures(){return ImGui::GetCurrentContext()!=nullptr;}
    inline std::uint64_t HashPath(const std::string& value){std::uint64_t hash=1469598103934665603ull;for(unsigned char byte:value){hash^=byte;hash*=1099511628211ull;}return hash;}
    inline std::filesystem::path DiskFile(const std::string& iconPath){const bool liveOnly=HelpyRuntimePreferences::IconCacheLimitMB.load()==0;return HostServices::CacheDirectory()/(liveOnly?"item-icons-live":"item-icons")/std::format("{:016x}.rsicon",HashPath(iconPath));}
    inline bool ReadThumbnail(const std::filesystem::path& file,int& width,int& height,std::vector<unsigned char>& pixels) {
        std::ifstream input(file,std::ios::binary|std::ios::ate);if(!input)return false;const auto size=input.tellg();
        if(size<16||size>16+256*256*4)return false;
        input.seekg(0);char magic[4]{};uint32_t w=0,h=0,bytes=0;
        input.read(magic,4);input.read(reinterpret_cast<char*>(&w),sizeof(w));input.read(reinterpret_cast<char*>(&h),sizeof(h));input.read(reinterpret_cast<char*>(&bytes),sizeof(bytes));
        if(!input||std::string(magic,4)!="RSI1"||w<1||h<1||w>256||h>256||bytes!=w*h*4||size!=static_cast<std::streamoff>(16+bytes))return false;
        pixels.resize(bytes);input.read(reinterpret_cast<char*>(pixels.data()),bytes);if(!input)return false;
        std::size_t alphaNonZero=0,rgbNonZero=0;
        for(std::size_t i=0;i+3<pixels.size();i+=4){
            if(pixels[i+3]!=0)++alphaNonZero;
            if(pixels[i]!=0||pixels[i+1]!=0||pixels[i+2]!=0)++rgbNonZero;
        }
        if(alphaNonZero==0&&rgbNonZero==0)return false;
        if(alphaNonZero==0&&rgbNonZero>0){
            for(std::size_t i=0;i+3<pixels.size();i+=4)pixels[i+3]=std::max({pixels[i],pixels[i+1],pixels[i+2]});
        }
        width=static_cast<int>(w);height=static_cast<int>(h);return true;
    }
    inline void ReapRetired(CacheState& state) {
        for(auto it=state.RetiredTextures.begin();it!=state.RetiredTextures.end();) {
            auto* texture=*it;
            if(!texture){it=state.RetiredTextures.erase(it);continue;}
            if(texture->Status!=ImTextureStatus_Destroyed){++it;continue;}
            ImGui::UnregisterUserTexture(texture);IM_DELETE(texture);it=state.RetiredTextures.erase(it);
        }
    }
    inline bool EvictOne(CacheState& state,const std::string& incoming) {
        if(state.Textures.size()<MaxResidentTextures)return true;
        const int frame=ImGui::GetFrameCount();
        auto victim=state.Textures.end();int oldest=frame+1;
        for(auto it=state.Textures.begin();it!=state.Textures.end();++it) {
            if(!it->second||it->second->Status!=ImTextureStatus_OK)continue;
            const auto wanted=state.WantedFrame.find(it->first);const int last=wanted==state.WantedFrame.end()?-1000000:wanted->second;
            if(frame-last<=ProtectRecentFrames)continue;
            if(last<oldest){oldest=last;victim=it;}
        }
        if(victim==state.Textures.end()) {
            HelpyIconTrace::Event("gpu-evict-skip",incoming,"reason='all-recent-or-pending' resident="+std::to_string(state.Textures.size()));return false;
        }
        const auto key=victim->first;auto* texture=victim->second;state.Textures.erase(victim);state.ReadyTraced.erase(key);state.DiskChecked.erase(key);
        if(texture){texture->SetStatus(ImTextureStatus_WantDestroy);state.RetiredTextures.push_back(texture);}
        HelpyIconTrace::Event("gpu-evict",key,"for='"+incoming+"' resident="+std::to_string(state.Textures.size()));
        HelpyIconTrace::Event("gpu-count",incoming,"resident="+std::to_string(state.Textures.size())+" retired="+std::to_string(state.RetiredTextures.size()));return true;
    }
    inline bool Install(const std::string& iconPath,const std::filesystem::path& file) {
        auto& state=State();if(state.Textures.contains(iconPath))return true;
        ReapRetired(state);
        if(state.Textures.size()>=MaxResidentTextures&&!EvictOne(state,iconPath)){HelpyIconTrace::Event("ui-install-fail",iconPath,"reason='resident-busy'");return false;}
        int width=0,height=0;std::vector<unsigned char> pixels;
        if(!ReadThumbnail(file,width,height,pixels)){HelpyIconTrace::Event("ui-install-fail",iconPath,"reason='cache-read' file='"+file.string()+"'");return false;}
        if(!BackendSupportsTextures()){HelpyIconTrace::Event("ui-install-fail",iconPath,"reason='no-imgui-context'");return false;}
        auto* texture=IM_NEW(ImTextureData)();texture->Create(ImTextureFormat_RGBA32,width,height);std::memcpy(texture->GetPixels(),pixels.data(),pixels.size());
        texture->UseColors=true;texture->RefCount=1;texture->SetStatus(ImTextureStatus_WantCreate);ImGui::RegisterUserTexture(texture);state.Textures.emplace(iconPath,texture);
        HelpyIconTrace::Event("ui-install",iconPath,"file='"+file.string()+"' size="+std::to_string(width)+"x"+std::to_string(height));
        HelpyIconTrace::Event("gpu-count",iconPath,"resident="+std::to_string(state.Textures.size())+" retired="+std::to_string(state.RetiredTextures.size()));return true;
    }
    inline bool TryInstallDisk(const std::string& iconPath) {
        auto& state=State();if(state.Textures.contains(iconPath))return true;if(state.DiskChecked.contains(iconPath))return false;state.DiskChecked.insert(iconPath);
        const auto file=DiskFile(iconPath);
        std::error_code existsError;const bool exists=std::filesystem::exists(file,existsError)&&!existsError;
        if(!exists){HelpyIconTrace::Event("disk-miss",iconPath,"file='"+file.string()+"'");return false;}
        if(!Install(iconPath,file)){HelpyIconTrace::Event("disk-invalid",iconPath,"file='"+file.string()+"'");return false;}
        HelpyIconTrace::Event("disk-hit",iconPath,"file='"+file.string()+"'");std::error_code ec;std::filesystem::last_write_time(file,std::filesystem::file_time_type::clock::now(),ec);return true;
    }
    inline void DropStaleQueued(CacheState& state) {
        if(!ImGui::GetCurrentContext())return;
        const int frame=ImGui::GetFrameCount();
        const auto stale=[&](const std::string& icon){const auto it=state.WantedFrame.find(icon);return it==state.WantedFrame.end()||frame-it->second>60;};
        auto prune=[&](std::deque<std::string>& queue){for(auto it=queue.begin();it!=queue.end();){if(stale(*it)){state.Queued.erase(*it);it=queue.erase(it);}else ++it;}};
        prune(state.High);prune(state.Normal);
        if(state.WantedFrame.size()>1024)for(auto it=state.WantedFrame.begin();it!=state.WantedFrame.end();){
            if(frame-it->second>360&&!state.Textures.contains(it->first)&&!state.Failed.contains(it->first)&&!state.Pending.contains(it->first))it=state.WantedFrame.erase(it);else ++it;
        }
    }
    inline std::string PopNext(CacheState& state) {
        DropStaleQueued(state);std::string value;
        if(!state.High.empty()){value=std::move(state.High.front());state.High.pop_front();}
        else if(!state.Normal.empty()){value=std::move(state.Normal.front());state.Normal.pop_front();}
        if(!value.empty())state.Queued.erase(value);
        return value;
    }
    inline void ApplyResult(CacheState& state,nlohmann::json result) {
        const auto icon=result.value("Icon",std::string{});if(icon.empty())return;const auto status=result.value("Status",std::string{});
        if(status.starts_with("World changed")){state.Pending.clear();state.High.clear();state.Normal.clear();state.Queued.clear();state.Failed.clear();return;}
        if(status=="Ready") {
            state.Pending.erase(icon);state.Attempts.erase(icon);const auto file=result.value("File",std::string{});
            HelpyIconTrace::Event("plugin-result",icon,"status='Ready' file='"+file+"' cacheHit="+(result.value("CacheHit",false)?std::string("true"):std::string("false")));
            if(file.empty()||!Install(icon,std::filesystem::path(file)))state.Failed.insert(icon);
        }
        else if(status=="Error") {
            state.Pending.erase(icon);const int attempt=++state.Attempts[icon];const auto error=result.value("Error",std::string{});
            const bool blank=error.find("blank item icon")!=std::string::npos||error.find("blank-payload")!=std::string::npos;
            const int maxAttempts=blank?3:3;
            HelpyIconTrace::Event("plugin-result-error",icon,"attempt="+std::to_string(attempt)+" error='"+error+"'");
            if(attempt>=maxAttempts)state.Failed.insert(icon);
            else {
                const int delayMs=blank?(60*attempt*attempt):(90*attempt);
                if(blank)HelpyIconTrace::Event("defer-warm",icon,"attempt="+std::to_string(attempt)+" delayMs="+std::to_string(delayMs));
                state.Queued.insert(icon);state.High.push_back(icon);state.NextSubmit=std::chrono::steady_clock::now()+std::chrono::milliseconds(delayMs);
            }
        }
    }
}

inline void Update() {
    auto& state=detail::State();detail::ReapRetired(state);
    const int cacheMode=HelpyRuntimePreferences::IconCacheLimitMB.load()==0?0:1;
    if(state.CacheMode!=cacheMode){state.CacheMode=cacheMode;state.DiskChecked.clear();HelpyIconTrace::Event("cache-mode","",std::string("persistent=")+(cacheMode?"true":"false"));}
    const auto generation=SpawnToolRequests::CurrentGeneration();
    if(state.Generation!=generation) {
        state.Generation=generation;state.Pending.clear();state.High.clear();state.Normal.clear();state.Queued.clear();state.Failed.clear();state.WantedFrame.clear();state.Attempts.clear();state.NextSubmit={};
    }
    for(auto& [icon,texture]:state.Textures)if(texture&&texture->Status==ImTextureStatus_OK){
        if(state.ReadyTraced.insert(icon).second)HelpyIconTrace::Event("texture-ready",icon);
        if(texture->Pixels)texture->DestroyPixels();
    }
    if(RemoteRead) {
        for(int i=0;i<16;++i){nlohmann::json result;if(!RemoteRead(result))break;detail::ApplyResult(state,std::move(result));}
    } else if(!state.Pending.empty()) {
        auto result=ItemIconRequests::Read();const auto icon=result.value("Icon",std::string{});if(state.Pending.contains(icon))detail::ApplyResult(state,std::move(result));
    }
    const auto now=std::chrono::steady_clock::now();if(now<state.NextSubmit)return;
    const std::size_t maxActive=RemoteSubmit?detail::MaxActiveRemote:1;
    while(state.Pending.size()<maxActive) {
        auto next=detail::PopNext(state);if(next.empty())return;
        if(state.Textures.contains(next)||state.Failed.contains(next)||state.Pending.contains(next))continue;
        const bool accepted=RemoteSubmit?RemoteSubmit(next):ItemIconRequests::TrySubmit({{"Icon",next}});
        if(!accepted){HelpyIconTrace::Event("submit-rejected",next);state.Queued.insert(next);state.High.push_front(std::move(next));state.NextSubmit=now+std::chrono::milliseconds(16);return;}
        HelpyIconTrace::Event("submit",next,"accepted=true");state.Pending.insert(std::move(next));state.NextSubmit={};
    }
}
inline ImTextureData* Get(const std::string& iconPath){const auto& textures=detail::State().Textures;const auto found=textures.find(iconPath);return found==textures.end()?nullptr:found->second;}
inline bool Failed(const std::string& iconPath){return detail::State().Failed.contains(iconPath);}
inline bool PromoteQueued(detail::CacheState& state,const std::string& iconPath) {
    if(!state.Queued.contains(iconPath))return false;
    const auto highIt=std::find(state.High.begin(),state.High.end(),iconPath);
    if(highIt!=state.High.end())return true;
    const auto normalIt=std::find(state.Normal.begin(),state.Normal.end(),iconPath);
    if(normalIt!=state.Normal.end())state.Normal.erase(normalIt);
    state.High.push_front(iconPath);
    HelpyIconTrace::Event("queue-promote",iconPath,"priority='high'");
    return true;
}
inline bool MakeRoomForVisible(detail::CacheState& state,const std::string& iconPath) {
    if(state.Queued.size()<detail::MaxQueued)return true;
    while(!state.Normal.empty()) {
        auto victim=std::move(state.Normal.back());state.Normal.pop_back();
        if(!state.Queued.erase(victim))continue;
        HelpyIconTrace::Event("queue-evict-prefetch",victim,"for='"+iconPath+"'");
        return true;
    }
    return false;
}
inline void Queue(const std::string& iconPath,bool highPriority=false) {
    if(iconPath.empty()||!detail::BackendSupportsTextures())return;
    auto& state=detail::State();state.WantedFrame[iconPath]=ImGui::GetFrameCount();
    HelpyIconTrace::Event("icon-demand",iconPath,std::string("priority='")+(highPriority?"high":"normal")+"' resident="+std::to_string(state.Textures.size()));
    if(state.Textures.contains(iconPath)){HelpyIconTrace::Event("icon-gate",iconPath,"reason='resident'");return;}
    if(detail::TryInstallDisk(iconPath))return;
    if(state.Failed.contains(iconPath)){HelpyIconTrace::Event("icon-gate",iconPath,"reason='failed'");return;}
    if(state.Pending.contains(iconPath)){HelpyIconTrace::Event("icon-gate",iconPath,"reason='pending'");return;}
    if(state.Queued.contains(iconPath)){if(highPriority)PromoteQueued(state,iconPath);return;}
    if(state.Queued.size()>=detail::MaxQueued&&(!highPriority||!MakeRoomForVisible(state,iconPath))){HelpyIconTrace::Event("queue-drop",iconPath,std::string("reason='queue-cap' priority='")+(highPriority?"high":"normal")+"'");return;}
    HelpyIconTrace::Event("queue",iconPath,std::string("priority='")+(highPriority?"high":"normal")+"'");
    state.Queued.insert(iconPath);(highPriority?state.High:state.Normal).push_back(iconPath);
}
inline bool Draw(const std::string& iconPath,const ImVec2& minimum,const ImVec2& maximum,bool highPriority=false) {
    if(!iconPath.empty())HelpyIconTrace::Event("visible-request",iconPath,std::string("priority='")+(highPriority?"high":"normal")+"'");
    if(auto* texture=Get(iconPath)){
        auto& state=detail::State();state.WantedFrame[iconPath]=ImGui::GetFrameCount();
        if(texture->Status==ImTextureStatus_OK){
            if(state.ReadyTraced.insert(iconPath).second)HelpyIconTrace::Event("texture-ready",iconPath);
            ImGui::GetWindowDrawList()->AddImage(texture->GetTexRef(),minimum,maximum);return true;
        }
        HelpyIconTrace::Event("texture-wait",iconPath,"status="+std::to_string(static_cast<int>(texture->Status)));
        return false;
    }
    Queue(iconPath,highPriority);return false;
}

struct DiskStats {std::uintmax_t Bytes=0;std::size_t Files=0;};
inline std::filesystem::path ActiveDiskFolder(){return HostServices::CacheDirectory()/(HelpyRuntimePreferences::IconCacheLimitMB.load()==0?"item-icons-live":"item-icons");}
inline DiskStats CurrentDiskStats(){DiskStats stats;std::error_code ec;const auto folder=ActiveDiskFolder();if(!std::filesystem::is_directory(folder,ec)||ec)return stats;
    for(std::filesystem::directory_iterator it(folder,ec),end;!ec&&it!=end;it.increment(ec)){if(!it->is_regular_file(ec)||ec){ec.clear();continue;}if(it->path().extension()!=L".rsicon")continue;const auto size=it->file_size(ec);if(ec){ec.clear();continue;}stats.Bytes+=size;++stats.Files;}return stats;}
inline bool CleanDiskCache(){std::error_code persistentError,liveError;std::filesystem::remove_all(HostServices::CacheDirectory()/"item-icons",persistentError);std::filesystem::remove_all(HostServices::CacheDirectory()/"item-icons-live",liveError);
    auto& state=detail::State();state.DiskChecked.clear();state.Failed.clear();HelpyRuntimePreferences::IconCacheEpoch.fetch_add(1);return !persistentError&&!liveError;}
inline size_t Count(){return detail::State().Textures.size();}
inline size_t QueuedCount(){auto& state=detail::State();return state.Queued.size()+state.Pending.size();}
}
