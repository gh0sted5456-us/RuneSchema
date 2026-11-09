#pragma once
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/HelpyControllerInput.h"
#include "Core/HelpyPreviewHooks.h"
#include <array>
#include <span>

namespace PS::HelpyMenu {
inline int PopupDepthAtFrameStart=0;
inline bool WidgetActiveAtFrameStart=false;
inline int EnterContentFrame=-1;
inline void FocusContent(bool childBody=true){if(EnterContentFrame==ImGui::GetFrameCount()){EnterContentFrame=-1;if(childBody)HelpyControllerInput::EnterSection();else HelpyControllerInput::FocusNext();}}
inline bool BeginContextItem(const char* id){
    if(ImGui::IsItemFocused()&&ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft,false))ImGui::OpenPopup(id);
    return ImGui::BeginPopupContextItem(id);
}
inline bool BeginMore(const char* id,const char* label="More"){
    const auto button=std::string(label)+"##"+id,popup=std::string("Actions##")+id;if(HelpyImGuiTheme::SoftButton(button.c_str()))ImGui::OpenPopup(popup.c_str());
    const auto area=ImGui::GetMainViewport()->WorkSize;ImGui::SetNextWindowSizeConstraints({0,0},{std::max(1.f,area.x-24),std::max(1.f,area.y-24)});
    return ImGui::BeginPopup(popup.c_str());
}
inline void NextControl(const char* label){ImGui::SameLine();if(ImGui::GetContentRegionAvail().x<ImGui::CalcTextSize(label).x+ImGui::GetStyle().FramePadding.x*2)ImGui::NewLine();}
template<class Labels>inline void Categories(const char* id,const Labels& labels,int& selected,const char* footer=nullptr){
    const auto& theme=HelpyImGuiTheme::Current;selected=std::clamp(selected,0,int(labels.size())-(footer?0:1));
    bool enterContent=false;
    const float width=std::max(1.f,std::min(theme.X(180),ImGui::GetContentRegionAvail().x*.30f));
    const bool entering=HelpyImGuiTheme::EnterNextSection;
    const auto pending=HelpyControllerPanes::Pending;
    ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panel);
    if(HelpyImGuiTheme::BeginPanel(id,{width,0},true,0,true)){
        const bool arriving=entering||(pending&&pending==ImGui::GetCurrentWindow()->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign,{.5f,.5f});
        const auto item=[&](int i,const char* label){
            ImGui::PushID(i);
            if(ImGui::Selectable(label,selected==i,ImGuiSelectableFlags_NoAutoClosePopups,{0,theme.Y(32)})){selected=i;if(HelpyControllerInput::Selection.Controller&&ImGui::GetCurrentContext()->NavInputSource==ImGuiInputSource_Gamepad)enterContent=true;}
            if(selected==int(i))HelpyControllerInput::DefaultFocus(arriving);
            // Directional input stays in this category list; Confirm enters its content.
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::MenuTabs[std::string(id)+"/"+label]=HelpyPreviewHooks::LastRect();
            HelpyPreviewHooks::MenuFocus[std::string(id)+"/"+label]={ImGui::GetItemID(),ImGui::GetCurrentWindow()->ID};
#endif
            ImGui::PopID();
        };
        for(size_t i=0;i<labels.size();++i)item(int(i),labels[i]);
        if(footer){ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY()+theme.Y(12),ImGui::GetCursorPosY()+ImGui::GetContentRegionAvail().y-theme.Y(32)-ImGui::GetStyle().ItemSpacing.y*2-1));ImGui::Separator();ImGui::PushID("footer");item(int(labels.size()),footer);ImGui::PopID();}
        ImGui::PopStyleVar();
    }PS::HelpyImGuiTheme::EndPanel();ImGui::PopStyleColor();ImGui::SameLine();
    if(enterContent)HelpyControllerInput::EnterSection();
}
inline bool Tab(const char* label,bool selected,float& used,float width,bool* down=nullptr,bool arriving=false){
    const auto& theme=HelpyImGuiTheme::Current;const auto& style=ImGui::GetStyle();
    const float size=std::min(width,ImGui::CalcTextSize(label).x+style.FramePadding.x*2+theme.X(12));
    if(used>0&&used+style.ItemSpacing.x+size<=width){ImGui::SameLine();used+=style.ItemSpacing.x;}else used=0;
    ImGui::PushStyleColor(ImGuiCol_Header,selected?theme.selection:theme.control);
    ImGui::PushStyleColor(ImGuiCol_Text,selected?theme.accentHover:theme.textDisabled);
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign,{.5f,.5f});
    const bool clicked=ImGui::Selectable(label,selected,ImGuiSelectableFlags_NoAutoClosePopups,{size,ImGui::GetFrameHeight()});
    if(selected)HelpyControllerInput::DefaultFocus(arriving);
    if(down&&HelpyControllerInput::Selection.Controller&&HelpyControllerInput::NavigationSourceItem()&&
        ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown,false)){
        ImGui::NavMoveRequestCancel();*down=true;
    }
    const bool enter=clicked&&HelpyControllerInput::Selection.Controller&&ImGui::GetCurrentContext()->NavInputSource==ImGuiInputSource_Gamepad;if(enter)EnterContentFrame=ImGui::GetFrameCount();
    ImGui::PopStyleVar();ImGui::PopStyleColor(2);
    if(selected){const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();ImGui::GetWindowDrawList()->AddLine(a,{b.x,a.y},ImGui::GetColorU32(theme.accent),theme.Y(2));}
    used+=size;return clicked||enter;
}
template<class Labels> inline bool Tabs(const char* id,const Labels& labels,int& selected,bool childBody=true){
    selected=std::clamp(selected,0,int(labels.size())-1);bool changed=false;float used=0;
    struct Target {ImGuiID Id,Scope;ImRect Bounds;};
    std::vector<Target> targets;int focus=-1;
    auto& nav=*ImGui::GetCurrentContext();
    const int step=HelpyControllerInput::Selection.Controller&&nav.NavMoveSubmitted&&nav.NavInputSource==ImGuiInputSource_Gamepad?
        int(nav.NavMoveDir==ImGuiDir_Right)-int(nav.NavMoveDir==ImGuiDir_Left):0;
    const float width=std::max(1.f,ImGui::GetContentRegionAvail().x);ImGui::PushID(id);
    for(size_t i=0;i<labels.size();++i){
        bool down=false;
        if(Tab(labels[i],selected==int(i),used,width,&down)||down){changed|=selected!=int(i);selected=int(i);}
        if(down)EnterContentFrame=ImGui::GetFrameCount();
        if(step){
            targets.push_back({ImGui::GetItemID(),nav.CurrentFocusScopeId,{ImGui::GetItemRectMin(),ImGui::GetItemRectMax()}});
            if(HelpyControllerInput::NavigationSourceItem())focus=int(i);
        }
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::MenuTabs[std::string(id)+"/"+labels[i]]=HelpyPreviewHooks::LastRect();
        HelpyPreviewHooks::MenuFocus[std::string(id)+"/"+labels[i]]={ImGui::GetItemID(),ImGui::GetCurrentWindow()->ID};
#endif
    }
    if(focus>=0){
        const int next=std::clamp(focus+step,0,int(targets.size())-1);
        const auto& target=targets[next];
        ImGui::NavMoveRequestCancel();
        auto* window=ImGui::GetCurrentWindow();
        ImGui::SetNavID(target.Id,nav.NavLayer,target.Scope,ImGui::WindowRectAbsToRel(window,target.Bounds));
        ImGui::SetNavCursorVisible(true);
        changed|=selected!=next;selected=next;
    }
    ImGui::PopID();ImGui::Separator();FocusContent(childBody);return changed;
}
inline bool BeginBody(const char* id,int selected){
    ImGui::PushID(selected);auto panel=HelpyImGuiTheme::Current.panel;panel.w=std::max(.94f,panel.w);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,panel);
    const bool visible=HelpyImGuiTheme::BeginPanel(id,{0,0},false,ImGuiWindowFlags_AlwaysUseWindowPadding,true);ImGui::PopStyleColor();ImGui::PushTextWrapPos(0);return visible;
}
inline void EndBody(){ImGui::PopTextWrapPos();PS::HelpyImGuiTheme::EndPanel();ImGui::PopID();}
}
