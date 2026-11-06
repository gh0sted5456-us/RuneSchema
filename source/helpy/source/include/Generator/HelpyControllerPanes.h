#pragma once
#include <imgui_internal.h>
#include <vector>
#include <algorithm>

namespace PS::HelpyControllerPanes {
inline ImGuiContext* Context=nullptr;
inline int Frame=-1,Resolved=-1;
inline ImGuiID Pending=0;inline int PendingFrame=-1;
inline std::vector<ImGuiWindow*> Panes;
inline void Register(ImGuiWindow* window){
    auto* context=ImGui::GetCurrentContext();
    if(Context!=context||context->FrameCount<Frame||context->FrameCount>PendingFrame)Pending=0;
    if(Context!=context||Frame!=context->FrameCount){Context=context;Frame=context->FrameCount;Panes.clear();Resolved=-1;}
    if(std::find(Panes.begin(),Panes.end(),window)==Panes.end())Panes.push_back(window);
    if(Pending==window->ID){
        Pending=0;ImGui::FocusWindow(window,ImGuiFocusRequestFlags_UnlessBelowModal);
        ImGui::SetNavWindow(window);ImGui::NavInitWindow(window,false);
        ImGui::NavClearPreferredPosForAxis(ImGuiAxis_X);ImGui::NavClearPreferredPosForAxis(ImGuiAxis_Y);
        ImGui::SetNavCursorVisible(true);context->NavInputSource=ImGuiInputSource_Gamepad;
    }
}
inline bool Move(){
    auto& context=*ImGui::GetCurrentContext();
    if(Context!=&context||Frame!=context.FrameCount||Resolved==Frame)return false;
    Resolved=Frame;
    if(context.IO.WantTextInput||context.ActiveId||context.DragDropActive||!context.OpenPopupStack.empty())return false;
    const int step=int(ImGui::IsKeyPressed(ImGuiKey_GamepadR1,false))-int(ImGui::IsKeyPressed(ImGuiKey_GamepadL1,false));
    if(!step)return false;
    std::vector<ImGuiWindow*> visible;
    for(auto* pane:Panes)if(pane->Active&&!pane->SkipItems&&pane->DC.NavLayersActiveMaskNext&&(pane->ClipRect.GetWidth()>8&&pane->ClipRect.GetHeight()>8))visible.push_back(pane);
    if(visible.empty())return false;
    int selected=-1,nearest=INT_MAX;
    for(size_t i=0;i<visible.size();++i){int depth=0;for(auto* nav=context.NavWindow;nav;nav=nav->ParentWindow,++depth)if(nav==visible[i]){if(depth<nearest){selected=int(i);nearest=depth;}break;}}
    const int index=selected<0?(step>0?0:int(visible.size())-1):(selected+step+int(visible.size()))%int(visible.size());
    // Focus before the target's controls are submitted on the next frame.
    Pending=visible[index]->ID;PendingFrame=context.FrameCount+1;
    return true;
}
}
