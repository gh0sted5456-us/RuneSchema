#pragma once
#include "Generator/HelpyImGuiTheme.h"
#include "Core/HelpyTaskbar.h"
#include "Core/HelpyPreviewHooks.h"
#include <imgui_internal.h>

namespace PS::HelpyWindowChrome {
struct State {bool Maximized=false,RestoreWanted=false;ImVec2 RestorePos{},RestoreSize{};};
inline ImVec2 Origin{},Size{};
inline void Bounds(ImVec2 origin,ImVec2 size){Origin=origin;Size=size;}
inline void Prepare(State& state){
    if(state.Maximized&&Size.x>0&&Size.y>0){ImGui::SetNextWindowPos({Origin.x+8,Origin.y+8});ImGui::SetNextWindowSize({std::max(1.f,Size.x-16),std::max(1.f,Size.y-16)});}
    else if(state.RestoreWanted){ImGui::SetNextWindowPos(state.RestorePos);ImGui::SetNextWindowSize(state.RestoreSize);state.RestoreWanted=false;}
}
// Docked floating windows use the same title-bar placement as ImGui's X.
// Detached native windows retain their operating-system title-bar controls.
inline void Render(State& state,HelpyTaskbar::Window task){
    auto* window=ImGui::GetCurrentWindow();const auto title=window->TitleBarRect();const auto& style=ImGui::GetStyle();
    const float side=ImGui::GetFontSize(),gap=style.ItemInnerSpacing.x;
    const ImVec2 close{title.Max.x-style.FramePadding.x-side,title.Min.y+style.FramePadding.y};
    const auto last=GImGui->LastItemData;
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::WindowControls[std::string(window->Name)+"/window"]={window->Pos.x,window->Pos.y,window->Pos.x+window->Size.x,window->Pos.y+window->Size.y};
#endif
    ImGui::PushClipRect(title.Min,title.Max,false);
    const auto button=[&](const char* id,const char* label,int column,bool maximize){
        const ImVec2 pos{close.x-column*(side+gap),close.y};const ImRect rect{pos,{pos.x+side,pos.y+side}};
        bool hover=false,held=false;const auto key=window->GetID(id);bool pressed=false;
        if(ImGui::ItemAdd(rect,key))pressed=ImGui::ButtonBehavior(rect,key,&hover,&held,ImGuiButtonFlags_NoNavFocus);
        auto* draw=window->DrawList;if(hover||held)draw->AddRectFilled(rect.Min,rect.Max,ImGui::GetColorU32(ImGuiCol_ButtonHovered),2);
        const auto color=ImGui::GetColorU32(ImGuiCol_Text);const float inset=side*.22f;
        if(maximize){draw->AddRect({pos.x+inset,pos.y+inset},{pos.x+side-inset,pos.y+side-inset},color,0,0,1);
            if(state.Maximized)draw->AddLine({pos.x+inset*1.5f,pos.y+inset*.5f},{pos.x+side-inset*.5f,pos.y+inset*.5f},color);}
        else draw->AddLine({pos.x+inset,pos.y+side*.65f},{pos.x+side-inset,pos.y+side*.65f},color,1.5f);
        if(hover)ImGui::SetTooltip("%s",label);
#if defined(HELPY_PREVIEW)
        const auto bounds=ImVec4(rect.Min.x,rect.Min.y,rect.Max.x,rect.Max.y);
        HelpyPreviewHooks::WindowControls[std::string(window->Name)+(maximize?"/maximize":"/minimize")]=bounds;
        if(!maximize&&task==HelpyTaskbar::Window::Runtime)HelpyPreviewHooks::RuntimeMinimize=bounds;
#endif
        return pressed;
    };
    if(button("##title-minimize","Minimize to Helpy taskbar",2,false))HelpyTaskbar::Current.Minimize(task);
    if(button("##title-maximize",state.Maximized?"Restore size":"Maximize",1,true)){
        if(state.Maximized){state.Maximized=false;state.RestoreWanted=true;}
        else{state.RestorePos=window->Pos;state.RestoreSize=window->Size;state.Maximized=true;}
    }
#if defined(HELPY_PREVIEW)
    const ImVec4 closeBounds{close.x,close.y,close.x+side,close.y+side};
    if(task==HelpyTaskbar::Window::Runtime)HelpyPreviewHooks::RuntimeClose=closeBounds;
    if(task==HelpyTaskbar::Window::IconCache)HelpyPreviewHooks::IconCacheClose=closeBounds;
#endif
    ImGui::PopClipRect();GImGui->LastItemData=last;
}
}
