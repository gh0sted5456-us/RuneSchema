#pragma once
#include "Generator/HelpyFeatureSettings.h"
#include "Generator/HelpyCoordinates.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Core/MapPresentationCache.h"
#include "Core/ProcessExitFuture.h"
#include <cstring>

namespace PS::HelpyMiniMap {
using Json=nlohmann::json;using Clock=std::chrono::steady_clock;
struct Loaded {Json Reference;std::string Pixels,Error;uint64_t Revision=0;};
inline ProcessExitFuture<Loaded> Worker;inline uint64_t LoadedRevision=~uint64_t{};inline ImTextureData* Texture=nullptr;inline std::vector<ImTextureData*> Retired;
inline MapProjection::Projection Projection;inline std::string Status;
inline void Retire(){if(Texture){Texture->SetStatus(ImTextureStatus_WantDestroy);Retired.push_back(Texture);Texture=nullptr;}}
inline void Reap(){for(auto it=Retired.begin();it!=Retired.end();){if((*it)->Status==ImTextureStatus_Destroyed){ImGui::UnregisterUserTexture(*it);IM_DELETE(*it);it=Retired.erase(it);}else ++it;}}
inline void Poll(bool enabled){
    Reap();if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=Worker.get();Status=result.Error;
        if(enabled&&result.Revision==MapPresentationCache::Revision.load()&&!result.Pixels.empty()){uint32_t width{},height{};std::memcpy(&width,result.Pixels.data()+4,4);std::memcpy(&height,result.Pixels.data()+8,4);Retire();Texture=IM_NEW(ImTextureData)();Texture->Create(ImTextureFormat_RGBA32,width,height);std::memcpy(Texture->GetPixels(),result.Pixels.data()+16,result.Pixels.size()-16);Texture->UseColors=true;Texture->RefCount=1;Texture->SetStatus(ImTextureStatus_WantCreate);ImGui::RegisterUserTexture(Texture);Projection=MapProjection::Projection::From(result.Reference.at("Projection"));}
    }
    if(!enabled){Retire();LoadedRevision=~uint64_t{};return;}
    const auto revision=MapPresentationCache::Revision.load();if(!Worker.valid()&&LoadedRevision!=revision){LoadedRevision=revision;const auto cache=HostServices::CacheDirectory()/"item-icons";
        Worker=std::async(std::launch::async,[cache,revision]{SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);Loaded result;result.Revision=revision;try{result.Reference=MapPresentationCache::Reference(cache);if(result.Reference.empty())result.Error="Download or import the world-map cache to enable the minimap.";else result.Pixels=MapPresentationCache::Bytes(cache,result.Reference);}catch(const std::exception& error){result.Error=error.what();}return result;});
    }
}
inline void Draw(const HelpyImGuiTheme::Theme& theme,ImVec2 viewport,ImVec2 origin={}){
    if(!HelpyFeatureSettings::Hud.Minimap||!HelpyCoordinates::HasPosition)return;
    const auto box=HelpyHudLayout::Map(viewport.x,viewport.y,HelpyFeatureSettings::Hud);const ImVec2 a{box.X-origin.x,box.Y-origin.y},b{a.x+box.W,a.y+box.H};auto* draw=ImGui::GetForegroundDrawList();auto background=theme.panelAlt;background.w=.94f;draw->AddRectFilled(a,b,ImGui::GetColorU32(background),theme.X(6));
    if(Texture&&Texture->Status==ImTextureStatus_OK&&Projection.Valid()){
        const auto position=HelpyCoordinates::Motion.At(HelpyCoordinates::Seconds());const auto center=Projection.ToUV({position.X,position.Y});const auto& settings=HelpyFeatureSettings::Hud;
        const double focusX=settings.FollowPlayer?center.X:settings.MapFocus.X,focusY=settings.FollowPlayer?center.Y:settings.MapFocus.Y;
        const double half=.5/settings.MapZoom,u=std::clamp(focusX-half,0.,1.-2*half),v=std::clamp(focusY-half,0.,1.-2*half);
        draw->PushClipRect(a,b,true);draw->AddImage(Texture->GetTexRef(),a,b,{float(u),float(v)},{float(u+2*half),float(v+2*half)});
        if(center.X>=u&&center.X<=u+2*half&&center.Y>=v&&center.Y<=v+2*half){const ImVec2 at{a.x+float((center.X-u)/(2*half))*box.W,a.y+float((center.Y-v)/(2*half))*box.H};const double yaw=position.Yaw*3.141592653589793/180;const float dx=float(std::cos(yaw)*Projection.Right.X+std::sin(yaw)*Projection.Right.Y),dy=-float(std::cos(yaw)*Projection.Up.X+std::sin(yaw)*Projection.Up.Y);draw->AddCircleFilled(at,theme.X(7),IM_COL32(0,0,0,230));draw->AddTriangleFilled({at.x+dx*theme.X(11),at.y+dy*theme.Y(11)},{at.x-dx*theme.X(6)-dy*theme.X(5),at.y-dy*theme.Y(6)+dx*theme.Y(5)},{at.x-dx*theme.X(6)+dy*theme.X(5),at.y-dy*theme.Y(6)-dx*theme.Y(5)},ImGui::GetColorU32(theme.accent));}
        draw->AddText({a.x+8,a.y+6},IM_COL32(255,255,255,255),"N");draw->PopClipRect();
    }else {draw->PushClipRect(a,b,true);draw->AddText({a.x+12,a.y+12},ImGui::GetColorU32(theme.text),"Preparing minimap...");draw->PopClipRect();}
    draw->AddRect(a,b,ImGui::GetColorU32(theme.accent),theme.X(6),0,theme.X(1.5f));
}
inline void Shutdown(){if(Worker.valid())try{Worker.get();}catch(...){}if(Texture){ImGui::UnregisterUserTexture(Texture);IM_DELETE(Texture);Texture=nullptr;}for(auto* texture:Retired){ImGui::UnregisterUserTexture(texture);IM_DELETE(texture);}Retired.clear();LoadedRevision=~uint64_t{};}
}
