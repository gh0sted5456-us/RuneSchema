#pragma once
#if defined(HELPY_PREVIEW)
#include <imgui.h>
#include <map>
#include <string>
namespace PS::HelpyPreviewHooks {
inline size_t BayDetailsFacts=0;
inline std::map<std::string,ImVec4> ItemRects,CartRects;
inline std::map<std::string,ImVec4> RecipientRemove;
inline ImVec4 RecipientsClear{};
inline std::map<std::string,ImVec4> AuthoringButtons;
inline ImVec4 Browser{},Cart{},Inputs{},Outputs{},Give{},AddCart{};
inline ImVec4 Map{},Tooltip{},MapSelected{},MapLodestone{};
inline ImVec4 MapSpawn{};
inline ImVec4 RuntimeFooter{},InformationBar{},InfoButton{};
inline ImVec4 BuildingBrowse{};
inline ImVec4 BuildingUse{};
inline ImVec4 BuildingClose{};
inline std::map<std::string,ImVec4> BuildingCards;
inline ImVec4 AuthoringSource{},AuthoringAppearance{},AuthoringCreate{},TransmogTab{},CreationTab{};
inline ImVec4 BayDock{};
inline ImVec4 CardScale{};
inline ImVec4 SourceFilter{},ViewFamily{};
inline std::map<std::string,ImVec4> SourceOptions;
inline size_t ModBadges=0;
inline std::map<std::string,ImVec4> ResizeHandles;
inline bool MouseEnabled=false,MouseDown=false,RightDown=false;
inline std::string TooltipPath;
inline size_t TooltipFacts=0;
inline float TooltipScroll=0,TooltipScrollMax=0;
inline ImVec2 Mouse{-100,-100};
inline ImVec4 LastRect(){const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();return {a.x,a.y,b.x,b.y};}
inline ImVec4 WindowRect(){const auto a=ImGui::GetWindowPos(),s=ImGui::GetWindowSize();return {a.x,a.y,a.x+s.x,a.y+s.y};}
}
#endif
