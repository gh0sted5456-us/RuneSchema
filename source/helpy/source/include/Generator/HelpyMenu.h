#pragma once
#include "Generator/HelpyImGuiTheme.h"
#include "Core/HelpyPreviewHooks.h"
#include <array>
#include <span>

namespace PS::HelpyMenu {
inline bool Tab(const char* label,bool selected,float& used,float width){
    const auto& theme=HelpyImGuiTheme::Current;const auto& style=ImGui::GetStyle();
    const float size=std::min(width,ImGui::CalcTextSize(label).x+style.FramePadding.x*2+theme.X(12));
    if(used>0&&used+style.ItemSpacing.x+size<=width){ImGui::SameLine();used+=style.ItemSpacing.x;}else used=0;
    ImGui::PushStyleColor(ImGuiCol_Header,selected?theme.selection:theme.control);
    ImGui::PushStyleColor(ImGuiCol_Text,selected?theme.accentHover:theme.textDisabled);
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign,{.5f,.5f});
    const bool clicked=ImGui::Selectable(label,selected,ImGuiSelectableFlags_None,{size,ImGui::GetFrameHeight()});
    ImGui::PopStyleVar();ImGui::PopStyleColor(2);
    if(selected){const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();ImGui::GetWindowDrawList()->AddLine(a,{b.x,a.y},ImGui::GetColorU32(theme.accent),theme.Y(2));}
    used+=size;return clicked;
}
template<class Labels> inline bool Tabs(const char* id,const Labels& labels,int& selected){
    selected=std::clamp(selected,0,int(labels.size())-1);bool changed=false;float used=0;
    const float width=std::max(1.f,ImGui::GetContentRegionAvail().x);ImGui::PushID(id);
    for(size_t i=0;i<labels.size();++i){
        if(Tab(labels[i],selected==int(i),used,width)){selected=int(i);changed=true;}
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::MenuTabs[std::string(id)+"/"+labels[i]]=HelpyPreviewHooks::LastRect();
#endif
    }
    ImGui::PopID();ImGui::Separator();return changed;
}
inline bool BeginBody(const char* id,int selected){
    ImGui::PushID(selected);auto panel=HelpyImGuiTheme::Current.panel;panel.w=std::max(.94f,panel.w);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,panel);
    const bool visible=ImGui::BeginChild(id,{0,0},false,ImGuiWindowFlags_AlwaysUseWindowPadding);ImGui::PopStyleColor();ImGui::PushTextWrapPos(0);return visible;
}
inline void EndBody(){ImGui::PopTextWrapPos();ImGui::EndChild();ImGui::PopID();}
}
