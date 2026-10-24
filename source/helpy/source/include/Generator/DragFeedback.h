#pragma once
#include "Generator/HelpyImGuiTheme.h"
namespace PS::DragFeedback {
#if defined(HELPY_PREVIEW)
inline int LastFrame=-1;inline std::string Kind,Label;inline ImVec4 Card{};
#endif
inline void Held(const char* kind,const char* label,ImTextureRef texture={}){
    const auto& theme=HelpyImGuiTheme::Current;auto* draw=ImGui::GetForegroundDrawList();const auto source=ImGui::GetItemRectMin(),sourceEnd=ImGui::GetItemRectMax();
    draw->AddRect(source,sourceEnd,ImGui::GetColorU32(theme.accentHover),theme.X(5),0,theme.X(2));ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    const auto* viewport=ImGui::GetMainViewport();const auto mouse=ImGui::GetMousePos();const float width=std::min(theme.X(280),std::max(1.f,viewport->WorkSize.x-theme.X(16))),height=theme.Y(76);
    const ImVec2 a{std::clamp(mouse.x+theme.X(18),viewport->WorkPos.x,std::max(viewport->WorkPos.x,viewport->WorkPos.x+viewport->WorkSize.x-width)),std::clamp(mouse.y+theme.Y(20),viewport->WorkPos.y,std::max(viewport->WorkPos.y,viewport->WorkPos.y+viewport->WorkSize.y-height))},b{a.x+width,a.y+height};
    draw->AddRectFilled({a.x+theme.X(4),a.y+theme.Y(5)},{b.x+theme.X(4),b.y+theme.Y(5)},IM_COL32(0,0,0,160),theme.X(6));
    draw->AddRectFilled(a,b,ImGui::GetColorU32(theme.panelAlt),theme.X(6));draw->AddRect(a,b,ImGui::GetColorU32(theme.accentHover),theme.X(6),0,theme.X(2));
    const float side=theme.Y(48);const ImVec2 art{a.x+theme.X(10),a.y+(height-side)*.5f};
    if(texture.GetTexID())draw->AddImage(texture,art,{art.x+side,art.y+side});
    else{draw->AddRect({art.x+side*.2f,art.y+side*.2f},{art.x+side*.8f,art.y+side*.8f},ImGui::GetColorU32(theme.accent),theme.X(4),0,theme.X(2));for(int i=0;i<3;++i)draw->AddLine({art.x+side*.35f,art.y+side*(.35f+i*.15f)},{art.x+side*.65f,art.y+side*(.35f+i*.15f)},ImGui::GetColorU32(theme.accent));}
    const float x=art.x+side+theme.X(10);draw->PushClipRect(a,b,true);
    draw->AddText({x,a.y+theme.Y(10)},ImGui::GetColorU32(theme.accentHover),kind);
    draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{x,a.y+theme.Y(30)},ImGui::GetColorU32(theme.text),label,nullptr,std::max(1.f,b.x-x-theme.X(10)));
    draw->PopClipRect();
#if defined(HELPY_PREVIEW)
    LastFrame=ImGui::GetFrameCount();Kind=kind;Label=label;Card={a.x,a.y,b.x,b.y};
#endif
}
}
