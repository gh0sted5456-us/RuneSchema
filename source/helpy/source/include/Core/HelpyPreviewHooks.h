#pragma once
#if defined(HELPY_PREVIEW)
#include <imgui.h>
#include <map>
#include <string>
namespace PS::HelpyPreviewHooks {
inline std::map<std::string,ImVec4> ItemRects,CartRects;
inline ImVec4 Browser{},Cart{},Inputs{},Outputs{},Give{},AddCart{};
inline ImVec4 Map{},Tooltip{},MapSelected{},MapLodestone{};
inline ImVec4 MapSpawn{};
inline ImVec4 AuthoringSource{},AuthoringAppearance{},AuthoringCreate{},TransmogTab{},CreationTab{};
inline bool MouseEnabled=false,MouseDown=false,RightDown=false;
inline std::string TooltipPath;
inline size_t TooltipFacts=0;
inline float TooltipScroll=0,TooltipScrollMax=0;
inline ImVec2 Mouse{-100,-100};
inline ImVec4 LastRect(){const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();return {a.x,a.y,b.x,b.y};}
inline ImVec4 WindowRect(){const auto a=ImGui::GetWindowPos(),s=ImGui::GetWindowSize();return {a.x,a.y,a.x+s.x,a.y+s.y};}
}
#endif
