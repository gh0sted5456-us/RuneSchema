#pragma once
#include "Core/LoadingIndicator.h"
#include "Core/HelpyPreviewHooks.h"
#include "Generator/HelpyImGuiTheme.h"
namespace PS::HelpyLoadingIndicator {
// Uploaded once with the other local UI assets. Never requests an engine icon.
inline ImTextureRef Texture;
inline void Details(const std::string& stage,const std::string& detail,ImVec2 minimum,ImVec2 maximum){
    HelpyImGuiTheme::BeginDetailsTooltip(minimum,maximum);
    HelpyImGuiTheme::DetailsTextUnformatted(stage.c_str());ImGui::Separator();
    HelpyImGuiTheme::DetailsText("%s",detail.empty()?"Working in the background. A reliable completion percentage is not available yet.":detail.c_str());
#if defined(HELPY_PREVIEW)
    const auto at=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();HelpyPreviewHooks::LoadingDetailsRect={at.x,at.y,at.x+size.x,at.y+size.y};
#endif
    HelpyImGuiTheme::EndDetailsTooltip();
#if defined(HELPY_PREVIEW)
    ++HelpyPreviewHooks::LoadingDetailsShown;HelpyPreviewHooks::LoadingDetailsText=stage+"\n"+detail;
#endif
}
inline void HoverDetails(const std::string& stage,const std::string& detail){if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))Details(stage,detail,ImGui::GetItemRectMin(),ImGui::GetItemRectMax());}
inline void Draw(ImVec2 center,float radius,const HelpyImGuiTheme::Theme& theme,const std::string& stage="Loading",const std::string& detail={},bool spinning=true){
#if defined(HELPY_PREVIEW)
    if(spinning)++HelpyPreviewHooks::LoadingRings;HelpyPreviewHooks::LoadingRect={center.x-radius,center.y-radius,center.x+radius,center.y+radius};
#endif
    auto* draw=ImGui::GetWindowDrawList();const float angle=spinning?static_cast<float>(std::fmod(ImGui::GetTime(),10.0)*3.141592653589793):0;
    if(Texture.GetTexID()!=ImTextureID{}){
        const auto q=LoadingIndicator::Quad({center.x,center.y},radius,angle);
        draw->AddImageQuad(Texture,{q[0].X,q[0].Y},{q[1].X,q[1].Y},{q[2].X,q[2].Y},{q[3].X,q[3].Y},{0,0},{1,0},{1,1},{0,1},ImGui::GetColorU32(theme.accent));
    }else{
        draw->PathClear();draw->PathArcTo(center,radius,angle,angle+4.65f,28);draw->PathStroke(ImGui::GetColorU32(theme.accent),0,theme.X(theme.loadingSpinnerThickness));
    }
    const ImVec2 minimum{center.x-radius,center.y-radius},maximum{center.x+radius,center.y+radius};
    if(ImGui::IsWindowHovered()&&ImGui::IsMouseHoveringRect(minimum,maximum))Details(stage,detail,minimum,maximum);
}
inline bool RefreshButton(const char* label,bool busy,const std::string& detail){
    const auto& theme=HelpyImGuiTheme::Current;const float icon=ImGui::GetFrameHeight()*.7f;
    const bool clicked=HelpyImGuiTheme::SoftButton(label);HoverDetails(busy?"Loading details":label,detail);ImGui::SameLine();
    const auto at=ImGui::GetCursorScreenPos();ImGui::Dummy({icon,icon});Draw({at.x+icon*.5f,at.y+icon*.5f},icon*.45f,theme,label,detail,busy);
    return clicked&&!busy;
}
}
