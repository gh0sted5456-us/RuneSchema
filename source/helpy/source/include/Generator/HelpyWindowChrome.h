#pragma once
#include "Generator/HelpyImGuiTheme.h"
#include "Core/HelpyTaskbar.h"
#include "Core/HelpyPreviewHooks.h"
#include "Core/HelpyNativeFrame.h"
#include <imgui_internal.h>

namespace PS::HelpyWindowChrome {
struct State {bool Maximized=false,RestoreWanted=false;ImVec2 RestorePos{},RestoreSize{};};
inline ImVec2 Origin{},Size{};
inline void Bounds(ImVec2 origin,ImVec2 size){Origin=origin;Size=size;}
// Opaque artwork hides DWM's paint, not its controls. Paint glyphs over the native hit targets.
inline void RenderNativeCaption(const HelpyNativeFrame::Metrics& frame,ImVec2 origin,const HelpyImGuiTheme::Theme& theme){
    auto* draw=ImGui::GetWindowDrawList();const float glyph=std::max(8.f,10.f*frame.Dpi/96.f),stroke=std::max(1.f,frame.Dpi/96.f);
    for(int column=0;column<3;++column){
        const auto bounds=frame.ControlRect(column);
        const ImVec2 min{origin.x+bounds.left,origin.y+bounds.top},max{origin.x+bounds.right,origin.y+bounds.bottom};
        if(max.x<=min.x||max.y<=min.y)continue;
        if(ImGui::IsMouseHoveringRect(min,max,false))draw->AddRectFilled(min,max,ImGui::GetColorU32(column==2?ImVec4(.8f,.1f,.1f,.9f):theme.navigationHover));
        const ImVec2 a{std::floor((min.x+max.x-glyph)*.5f)+.5f,std::floor((min.y+max.y-glyph)*.5f)+.5f},b{a.x+glyph,a.y+glyph};
        const auto color=ImGui::GetColorU32(theme.text);
        if(column==0)draw->AddLine({a.x,(a.y+b.y)*.5f},{b.x,(a.y+b.y)*.5f},color,stroke);
        else if(column==1){
            if(frame.Maximized){draw->AddRect({a.x+glyph*.25f,a.y},{b.x,b.y-glyph*.25f},color,0,0,stroke);draw->AddRectFilled({a.x,a.y+glyph*.25f},{b.x-glyph*.25f,b.y},ImGui::GetColorU32(theme.window));draw->AddRect({a.x,a.y+glyph*.25f},{b.x-glyph*.25f,b.y},color,0,0,stroke);}
            else draw->AddRect(a,b,color,0,0,stroke);
        }else{draw->AddLine(a,b,color,stroke);draw->AddLine({a.x,b.y},{b.x,a.y},color,stroke);}
#if defined(HELPY_PREVIEW)
        constexpr const char* names[]{"main/minimize","main/maximize","main/close"};
        HelpyPreviewHooks::WindowControls[names[column]]={min.x,min.y,max.x,max.y};
#endif
    }
}
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
        if(hover)PS::HelpyImGuiTheme::Tooltip("%s",label);
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
