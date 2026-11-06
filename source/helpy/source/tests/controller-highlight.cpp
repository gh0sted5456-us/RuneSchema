#include <imgui_internal.h>
#include "Generator/HelpyFocusHighlight.h"
#include <iostream>
#include <stdexcept>
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void Contained(ImDrawList* draw,int first,ImRect bounds,bool fill){
    bool painted=false,translucent=false;constexpr ImU32 rgb=IM_COL32(214,177,83,0)&~IM_COL32_A_MASK;
    for(const auto& command:draw->CmdBuffer)for(unsigned index=0;index<command.ElemCount;++index){
        const auto vertexIndex=command.VtxOffset+draw->IdxBuffer[command.IdxOffset+index];if(vertexIndex<static_cast<unsigned>(first))continue;
        const auto& vertex=draw->VtxBuffer[vertexIndex];const auto alpha=(vertex.col>>IM_COL32_A_SHIFT)&255;
        if((vertex.col&~IM_COL32_A_MASK)!=rgb||!alpha)continue;painted=true;translucent|=alpha<64;
        if(fill)Check(command.ClipRect.x>=bounds.Min.x&&command.ClipRect.y>=bounds.Min.y&&command.ClipRect.z<=bounds.Max.x&&command.ClipRect.w<=bounds.Max.y,"Focus scissor escaped its control");
        Check(vertex.pos.x>=bounds.Min.x-.01f&&vertex.pos.y>=bounds.Min.y-.01f&&vertex.pos.x<=bounds.Max.x+.01f&&vertex.pos.y<=bounds.Max.y+.01f,"Visible focus geometry escaped its control");
    }
    Check(painted,"Focused control has no highlight");if(fill)Check(translucent,"Focus has no translucent accent fill");
}
int main()try{
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={900,600};io.DeltaTime=1.f/60;io.Fonts->Build();
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad;io.BackendFlags|=ImGuiBackendFlags_HasGamepad;
    for(float scale:{1.f,1.5f,2.f}){
        ImGui::GetStyle()=ImGuiStyle{};ImGui::GetStyle().ScaleAllSizes(scale);ImGui::GetStyle().Colors[ImGuiCol_NavCursor]={214/255.f,177/255.f,83/255.f,1};
        ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({900,600});ImGui::Begin("Focus fixture");auto* window=ImGui::GetCurrentWindow();auto* draw=window->DrawList;
        const auto focus=[&](ImGuiID id){ImGui::FocusWindow(window);ImGui::SetFocusID(id,window);GImGui->NavInputSource=ImGuiInputSource_Gamepad;ImGui::SetNavCursorVisible(true);};
        const ImRect bounds{{100,100},{100+180*scale,100+32*scale}};focus(12345);
        for(int flags:{0,int(ImGuiNavRenderCursorFlags_Compact),int(ImGuiNavRenderCursorFlags_NoRounding)}){const int first=draw->VtxBuffer.Size;ImGui::RenderNavCursor(bounds,12345,flags);Contained(draw,first,bounds,true);}
        const ImRect clipped{{-10,-10},{120,90}};auto expected=clipped;expected.ClipWith(window->ClipRect);const int beforeClip=draw->VtxBuffer.Size;ImGui::RenderNavCursor(clipped,12345);Contained(draw,beforeClip,expected,true);
        const int beforePane=draw->VtxBuffer.Size,commands=draw->CmdBuffer.Size;PS::HelpyFocusHighlight::Draw(draw,bounds.Min,bounds.Max,IM_COL32(214,177,83,255),8*scale,false,2*scale);Contained(draw,beforePane,bounds,false);Check(draw->CmdBuffer.Size==commands,"Inset card borders split rendering into extra draw batches");
        ImGui::SetCursorScreenPos({40,220});const ImRect selectable{{40,220},{40+180*scale,220+32*scale}};focus(ImGui::GetID("Selectable"));const int beforeSelectable=draw->VtxBuffer.Size;
        ImGui::Selectable("Selectable",false,0,{180*scale,32*scale});Contained(draw,beforeSelectable,selectable,true);
        ImGui::SetCursorScreenPos({40,330});const ImRect button{{40,330},{40+180*scale,330+32*scale}};focus(ImGui::GetID("Button"));const int beforeButton=draw->VtxBuffer.Size;
        ImGui::Button("Button",{180*scale,32*scale});Contained(draw,beforeButton,button,true);
        ImGui::SetCursorScreenPos({500,40});ImGui::BeginChild("Scrolling pane",{260,120},ImGuiChildFlags_Borders);
        auto* child=ImGui::GetCurrentWindow();const ImRect childBounds{child->Pos,{child->Pos.x+child->Size.x,child->Pos.y+child->Size.y}};
        ImGui::Dummy({20,1000});child->DC.NavLayersActiveMask=0;child->DC.NavWindowHasScrollY=true;GImGui->NavWindow=child;GImGui->NavId=12346;ImGui::SetNavCursorVisible(true);
        const int beforeChild=draw->VtxBuffer.Size;ImGui::EndChild();Contained(draw,beforeChild,childBounds,true);
        focus(12345);GImGui->NavCursorVisible=false;const int hidden=draw->VtxBuffer.Size;ImGui::RenderNavCursor(bounds,12345);Check(draw->VtxBuffer.Size==hidden,"Hidden focus unexpectedly painted");
        ImGui::SetNavCursorVisible(true);ImGui::PushStyleColor(ImGuiCol_NavCursor,{0,0,0,0});ImGui::RenderNavCursor(bounds,12345);ImGui::PopStyleColor();Check(draw->VtxBuffer.Size==hidden,"Suppressed custom-control focus unexpectedly painted");
        PS::HelpyFocusHighlight::Draw(draw,{100,100},{100,100},IM_COL32(214,177,83,255),4);Check(draw->VtxBuffer.Size==hidden,"Empty focus bounds emitted geometry");
        ImGui::End();ImGui::Render();
    }
    ImGui::DestroyContext();std::cout<<"Inset translucent focus passed for buttons, selectables, clipping, panels and 100/150/200 percent scale.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
