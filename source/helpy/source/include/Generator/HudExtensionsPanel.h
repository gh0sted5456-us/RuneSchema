#pragma once
#include "Generator/RuneSchemaSettingsPanel.h"

namespace PS::HudExtensionsPanel {
inline bool HideRequested=false;
inline nlohmann::json Clients;
inline std::string Status;
inline bool Request(const nlohmann::json& payload,nlohmann::json& result){
    return RuneSchemaSettingsPanel::Remote&&RuneSchemaSettingsPanel::Remote({{"Action","HudExtensions"},{"Payload",payload}},result);
}
inline void Render(){
    ImGui::SeparatorText("HUD extensions");
    ImGui::TextWrapped("Open a registered app through the shared in-game HUD. The app owns its interface and data.");
    const bool refresh=HelpyImGuiTheme::SoftButton("Refresh extensions")||Clients.is_null();
    if(refresh){nlohmann::json result;if(Request({{"Action","List"}},result)&&result.value("Available",false)){Clients=result.value("Clients",nlohmann::json::array());Status.clear();}else{Clients=nlohmann::json::array();Status="Connect to a game with RuneSchema.HUD enabled to use extensions.";}}
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    if(Clients.empty()){ImGui::TextDisabled("No UI extensions are registered.");return;}
    for(const auto& client:Clients){
        const auto id=client.at("Client").get<uint64_t>();const auto label=client.value("Owner",std::string("Extension"));const bool ready=client.value("UiReady",false);
        ImGui::PushID(std::to_string(id).c_str());ImGui::SeparatorText(label.c_str());
        ImGui::TextDisabled("%s",ready?"UI producer ready":"Integration pending");ImGui::BeginDisabled(!ready);
        if(HelpyImGuiTheme::SoftButton("Open in HUD")){nlohmann::json result;if(Request({{"Action","Activate"},{"Client",id}},result)&&result.value("Accepted",false)){HideRequested=true;Status="Activation requested. The extension owns its ready/open lifecycle.";}else Status=result.value("Error",std::string("The extension could not be activated"));}
        ImGui::EndDisabled();
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::MenuTabs["hud-open-"+std::to_string(id)]=HelpyPreviewHooks::LastRect();
#endif
        if(HelpyImGuiTheme::SoftButton("Close extension")){nlohmann::json result;Request({{"Action","Deactivate"},{"Client",id}},result);}
        ImGui::PopID();
    }
}
}
