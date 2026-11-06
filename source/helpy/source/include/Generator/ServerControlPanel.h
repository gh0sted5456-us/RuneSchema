#pragma once
#include "Generator/DedicatedServerPanel.h"
#include "Generator/ServerPlayersPanel.h"
#include <functional>

namespace PS::ServerControlPanel {
inline int Category=-1;
inline void Render(const std::function<void()>& renderAccess){
    if(Category<0)Category=HelpyEnvironment::Server()?0:1;
    constexpr std::array categories{"Settings","Players","RuneSchema access"};
    HelpyMenu::Categories("server-control",categories,Category);
    const auto& theme=HelpyImGuiTheme::Current;auto surface=theme.panel;surface.w=std::max(.94f,surface.w);ImGui::PushStyleColor(ImGuiCol_ChildBg,surface);
    ImGui::PushID(Category);
    if(HelpyImGuiTheme::BeginPanel("##server-control-details",{0,0},true,0,true)){
        if(Category==0){
            DedicatedServerPanel::Render();
            if(!HelpyEnvironment::Server()&&HelpyImGuiTheme::SoftButton("View connected players & roles"))Category=1;
        }
        else if(Category==1)ServerPlayersPanel::Render();
        else if(renderAccess)renderAccess();
    }
    HelpyImGuiTheme::EndPanel();ImGui::PopID();ImGui::PopStyleColor();
}
}
