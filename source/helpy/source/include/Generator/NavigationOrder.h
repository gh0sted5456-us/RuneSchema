#pragma once
#include "Core/HelpyNavigationOrder.h"
#include "Core/HelpyNavigation.h"
#include "Generator/DragFeedback.h"
#include <imgui.h>
namespace PS::NavigationOrder {
using Page=HelpyNavigation::Page;
template<class Range>inline std::vector<int> Ids(const Range& pages){std::vector<int> ids;for(auto page:pages)ids.push_back(static_cast<int>(page));return ids;}
template<class Range>inline std::vector<Page> Pages(const std::string& surface,const Range& defaults){std::vector<Page> out;for(int id:HelpyNavigationOrder::Ordered(surface,Ids(defaults)))out.push_back(static_cast<Page>(id));return out;}
template<class Range>inline void Drag(const char* surface,Page page,const Range& defaults,const char* label){
    struct Payload {char Surface[64];int Page;};
    if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip)){Payload payload{};strncpy_s(payload.Surface,surface,_TRUNCATE);payload.Page=static_cast<int>(page);ImGui::SetDragDropPayload("HELPY_NAV_ORDER",&payload,sizeof(payload));DragFeedback::Held(std::string(surface)=="home"?"Home tile":std::string(surface)==HelpyNavigationOrder::SidebarSurface?"Navigation":"Tab",label);ImGui::EndDragDropSource();}
    if(ImGui::BeginDragDropTarget()){if(const auto* data=ImGui::AcceptDragDropPayload("HELPY_NAV_ORDER",ImGuiDragDropFlags_AcceptBeforeDelivery|ImGuiDragDropFlags_AcceptNoDrawDefaultRect)){if(data->DataSize==sizeof(Payload)){const auto& payload=*static_cast<const Payload*>(data->Data);if(memchr(payload.Surface,0,sizeof(payload.Surface))&&std::string(payload.Surface)==surface&&payload.Page!=static_cast<int>(page)){
        const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();ImGui::GetWindowDrawList()->AddRect(a,b,ImGui::GetColorU32(HelpyImGuiTheme::Current.accentHover),HelpyImGuiTheme::Current.X(4),0,HelpyImGuiTheme::Current.X(2));
        if(data->IsDelivery())HelpyNavigationOrder::Move(surface,payload.Page,static_cast<int>(page),Ids(defaults));
    }}}ImGui::EndDragDropTarget();}
    if(ImGui::BeginPopupContextItem("navigation-order")){ImGui::TextDisabled("Drag to rearrange");if(ImGui::MenuItem("Reset this navigation order"))HelpyNavigationOrder::Reset(surface);if(!HelpyNavigationOrder::Status.empty())ImGui::TextWrapped("%s",HelpyNavigationOrder::Status.c_str());ImGui::EndPopup();}
}
}
