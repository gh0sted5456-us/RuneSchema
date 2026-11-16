#pragma once
#include "Generator/HelpyFeatureSettings.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/HelpyMiniMap.h"
#include "Core/LoadingIndicator.h"
#include <array>

namespace PS::HelpyHudPanel {
inline ImVec2 Viewport{1920,1080};inline bool Detected=false;
inline void Render(){
    HelpyFeatureSettings::Load();auto& theme=HelpyImGuiTheme::Current;static HelpyHudLayout::Settings draft;static std::filesystem::path root;static int selected=0,drag=-1;static ImVec2 grab;
    if(!std::isfinite(Viewport.x)||!std::isfinite(Viewport.y)||Viewport.x<1||Viewport.y<1){Viewport={1920,1080};Detected=false;}
    if(root!=HelpyFeatureSettings::Root){root=HelpyFeatureSettings::Root;draft=HelpyFeatureSettings::Hud;}
    ImGui::TextWrapped("Drag HUD elements around the game-sized preview, then Save to apply their positions immediately. Arrow-friendly position controls are below.");
    ImGui::TextDisabled("%s %.0f x %.0f",Detected?"Game viewport":"Preview (game not detected):",Viewport.x,Viewport.y);
    if(HelpyImGuiTheme::Toggle("Minimap",&draft.Minimap)){HelpyFeatureSettings::SaveHud(draft);draft.Minimap=HelpyFeatureSettings::Hud.Minimap;}
    if(HelpyImGuiTheme::Toggle("Show player coordinates",&theme.showCoordinates))HelpyImGuiTheme::SaveCurrent();
    HelpyImGuiTheme::Toggle("Coordinates below minimap",&draft.CoordinatesBelowMap);
    ImGui::SetNextItemWidth(theme.X(230));ImGui::SliderFloat("Minimap size",&draft.MapSize,160,600,"%.0f px",ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetNextItemWidth(theme.X(230));ImGui::SliderFloat("Minimap zoom",&draft.MapZoom,1,32,"%.1fx",ImGuiSliderFlags_AlwaysClamp);
    if(HelpyImGuiTheme::SoftButton("Zoom out"))draft.MapZoom=std::max(1.f,draft.MapZoom/1.25f);ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Zoom in"))draft.MapZoom=std::min(32.f,draft.MapZoom*1.25f);
    const auto centerOnPlayer=[&]{if(HelpyCoordinates::HasPosition&&HelpyMiniMap::Projection.Valid()){const auto uv=HelpyMiniMap::Projection.ToUV({HelpyCoordinates::X,HelpyCoordinates::Y});draft.MapFocus={float(std::clamp(uv.X,0.,1.)),float(std::clamp(uv.Y,0.,1.))};}};
    if(HelpyImGuiTheme::Toggle("Keep player centered",&draft.FollowPlayer)&&!draft.FollowPlayer)centerOnPlayer();
    if(!draft.FollowPlayer&&HelpyImGuiTheme::SoftButton("Center view on player"))centerOnPlayer();
    HelpyImGuiTheme::Toggle("Lock minimap position",&draft.MapPositionLocked);
    const float scale=std::min(std::max(1.f,ImGui::GetContentRegionAvail().x)/Viewport.x,theme.Y(310)/Viewport.y);const ImVec2 start=ImGui::GetCursorScreenPos(),size{Viewport.x*scale,Viewport.y*scale};
    ImGui::InvisibleButton("HUD layout preview",size,ImGuiButtonFlags_MouseButtonLeft);auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(start,{start.x+size.x,start.y+size.y},IM_COL32(12,14,16,240),4);draw->AddRect(start,{start.x+size.x,start.y+size.y},ImGui::GetColorU32(theme.border),4);
    const auto loading=LoadingIndicator::FitStartup(Viewport.x,Viewport.y,theme.X(1),theme.fontScale);
    const std::array boxes{HelpyHudLayout::Map(Viewport.x,Viewport.y,draft),HelpyHudLayout::Coordinates(Viewport.x,Viewport.y,700,32,draft),HelpyHudLayout::Place(Viewport.x,Viewport.y,380,160,draft.Notifications),HelpyHudLayout::Place(Viewport.x,Viewport.y,loading.Width,loading.Height,draft.Loading)};
    constexpr std::array names{"Minimap","Coordinates","Notifications","Loading indicator"};
    for(int i=0;i<4;++i){const auto& box=boxes[i];const ImVec2 a{start.x+box.X*scale,start.y+box.Y*scale},b{a.x+box.W*scale,a.y+box.H*scale};draw->AddRectFilled(a,b,ImGui::GetColorU32(i==selected?theme.navigationActive:theme.panelAlt),3);draw->AddRect(a,b,ImGui::GetColorU32(i==selected?theme.accent:theme.border),3,0,1.5f);draw->PushClipRect(a,b,true);if(b.y-a.y>ImGui::GetTextLineHeight()+6)draw->AddText({a.x+3,a.y+3},ImGui::GetColorU32(theme.text),names[i]);draw->PopClipRect();if(ImGui::IsItemHovered()&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&ImGui::IsMouseHoveringRect(a,b)){selected=i;drag=i;grab={ImGui::GetIO().MousePos.x-a.x,ImGui::GetIO().MousePos.y-a.y};}}
    const auto point=[&](int i)->HelpyHudLayout::Point&{return i==0?draft.Map:i==1?draft.Coordinates:i==2?draft.Notifications:draft.Loading;};
    if(drag>=0&&ImGui::IsMouseDown(ImGuiMouseButton_Left)&&!(drag==0&&draft.MapPositionLocked)){const auto& box=boxes[drag];point(drag)=HelpyHudLayout::Drag(Viewport.x,Viewport.y,box.W,box.H,(ImGui::GetIO().MousePos.x-start.x-grab.x)/scale,(ImGui::GetIO().MousePos.y-start.y-grab.y)/scale);if(drag==1)draft.CoordinatesBelowMap=false;}else drag=-1;
    ImGui::SetNextItemWidth(theme.X(230));ImGui::Combo("HUD element",&selected,names.data(),int(names.size()));auto& position=point(selected);ImGui::BeginDisabled(selected==0&&draft.MapPositionLocked);ImGui::SetNextItemWidth(theme.X(230));ImGui::SliderFloat("Across",&position.X,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);ImGui::SetNextItemWidth(theme.X(230));ImGui::SliderFloat("Down",&position.Y,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);ImGui::EndDisabled();
    if(selected==1&&draft.CoordinatesBelowMap)ImGui::TextDisabled("Turn off Coordinates below minimap for independent placement.");
    if(HelpyImGuiTheme::SoftButton("Save HUD layout"))HelpyFeatureSettings::SaveHud(draft);ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Reset preview"))draft={};ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Discard changes"))draft=HelpyFeatureSettings::Hud;
    if(!HelpyFeatureSettings::Status.empty())ImGui::TextWrapped("%s",HelpyFeatureSettings::Status.c_str());
}
}
