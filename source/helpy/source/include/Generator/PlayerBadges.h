#pragma once
#include "Core/PlayerStatus.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/ToolRequest.h"
namespace PS::PlayerBadges {
inline PlayerStatus::Cache Roles;
inline void Update(const nlohmann::json& snapshot) {
    const auto generation=SpawnToolRequests::CurrentGeneration();
    if(Roles.Generation!=generation)Roles.Reset(generation);
    if(snapshot.contains("PlayerRoles"))Roles.Apply(snapshot["PlayerRoles"],generation);
}
inline const nlohmann::json* Find(const nlohmann::json& row) {
    if(Roles.Generation!=SpawnToolRequests::CurrentGeneration())return nullptr;
    try{return Roles.Find(row.value("Path",std::string{}),row.value("Guid",std::string{}));}catch(...){return nullptr;}
}
inline std::string Tooltip(const nlohmann::json& role) {
    const auto name=role.value("Role",std::string("unknown"));
    std::string text=name=="owner"?"Server owner":name=="admin"?"Server administrator":name=="host"?"Co-op / local host":name=="member"?"Player privileges":"Role unavailable";
    if(role.value("Host",false)&&name!="host")text+=" · local host";
    for(const auto& right:role["Privileges"])text+="\n"+right.get<std::string>();
    if(!role.value("NativeKnown",false))text+="\nNative privileges unavailable";
    return text+"\nReported by authority; does not grant Helpy actions.";
}
inline bool Draw(ImDrawList* draw,ImVec2 center,float size,const nlohmann::json* status) {
    if(!status)return false;const auto role=status->value("Role",std::string("unknown"));if(role=="unknown")return false;
    const auto& t=HelpyImGuiTheme::Current;const auto color=ImGui::GetColorU32(role=="member"?t.textDisabled:t.accentHover);const float r=size*.42f;
    if(role=="owner") {
        const ImVec2 points[]{{center.x-r,center.y-r*.5f},{center.x-r*.5f,center.y},{center.x,center.y-r},{center.x+r*.5f,center.y},{center.x+r,center.y-r*.5f},{center.x+r*.75f,center.y+r},{center.x-r*.75f,center.y+r}};
        draw->AddPolyline(points,7,color,ImDrawFlags_Closed,1.5f);
    }else if(role=="host") {
        draw->AddLine({center.x-r,center.y},{center.x,center.y-r},color,1.5f);draw->AddLine({center.x,center.y-r},{center.x+r,center.y},color,1.5f);
        draw->AddRect({center.x-r*.65f,center.y},{center.x+r*.65f,center.y+r},color,0,0,1.5f);
    }else if(role=="admin") {
        const ImVec2 points[]{{center.x-r,center.y-r},{center.x+r,center.y-r},{center.x+r*.8f,center.y+r*.35f},{center.x,center.y+r},{center.x-r*.8f,center.y+r*.35f}};
        draw->AddPolyline(points,5,color,ImDrawFlags_Closed,1.5f);
    }else {draw->AddCircle(center,r,color,12,1.5f);draw->AddCircleFilled(center,size*.1f,color);}
    return true;
}
inline void Inline(const nlohmann::json& row) {
    const auto* role=Find(row);if(!role||role->value("Role",std::string{})=="unknown")return;
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();const float size=ImGui::GetTextLineHeight();
    const auto name=row.value("Name",std::string{});const float right=ImGui::GetWindowPos().x+ImGui::GetWindowContentRegionMax().x;
    const bool fullWidth=b.x>=right-ImGui::GetStyle().ItemSpacing.x;
    const ImVec2 center{fullWidth?std::min(b.x-size*.6f,a.x+ImGui::CalcTextSize(name.c_str()).x+size):std::min(right-size*.6f,b.x+size*.8f),(a.y+b.y)*.5f};
    Draw(ImGui::GetWindowDrawList(),center,size,role);
    if(ImGui::IsWindowHovered()&&ImGui::IsMouseHoveringRect({center.x-size*.5f,center.y-size*.5f},{center.x+size*.5f,center.y+size*.5f}))ImGui::SetTooltip("%s",Tooltip(*role).c_str());
}
}
