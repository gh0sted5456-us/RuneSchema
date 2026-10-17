#pragma once
#include "Core/ItemPresentation.h"
#include <algorithm>
#include <string>
#include <cctype>

namespace PS::BuildingMenu {
inline bool Matches(const nlohmann::json& row,std::string query,int category=-1) {
    if(category>=0&&row.value("CategoryId",-1)!=category)return false;
    auto text=ItemPresentation::Text(row,"Name")+" "+ItemPresentation::Text(row,"Category")+" "+ItemPresentation::Text(row,"Group")+" "+ItemPresentation::Text(row,"Path");
    const auto lower=[](unsigned char c){return static_cast<char>(std::tolower(c));};
    std::transform(text.begin(),text.end(),text.begin(),lower);std::transform(query.begin(),query.end(),query.begin(),lower);
    return text.find(query)!=std::string::npos;
}
inline std::string BlockedReason(bool authority,bool ground,bool projection,bool choice,bool player,bool busy,bool pending) {
    if(!authority)return "Building placement requires the host's RSMK.";
    if(!ground)return "Placement blocked: this area's ground is not loaded. Move near the point (or teleport there), then Retry ground. Nothing has been placed.";
    if(!projection)return "Placement blocked: native map alignment is unavailable. Refresh the map.";
    if(!choice)return "Choose a building piece from the building menu.";
    if(!player)return "Choose a player context before placing a building.";
    if(busy||pending)return "Waiting for the current runtime request to finish. No second placement has been sent.";
    return {};
}
}
