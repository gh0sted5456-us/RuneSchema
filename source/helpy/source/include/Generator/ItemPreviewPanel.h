#pragma once
#include "Generator/AuthoringClient.h"
#include "Generator/CharacterPreviewPanel.h"
#include "Core/ItemPreviewRequest.h"
namespace PS::ItemPreviewPanel {
inline uint64_t Receipt=0,ReceiptRevision=0,SeenRevision=~uint64_t{};inline int Quality=1;
inline void Render(){
    if(!ItemPreviewRequest::Open)return;auto& theme=HelpyImGuiTheme::Current;
    HelpyImGuiTheme::Scope skin(theme);const bool active=AuthoringClient::EnsureActive();
    if(SeenRevision!=ItemPreviewRequest::Revision){CharacterPreviewPanel::Close();SeenRevision=ItemPreviewRequest::Revision;}
    ImGui::SetNextWindowSize({theme.X(480),theme.Y(660)},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({0,0},{ImGui::GetIO().DisplaySize.x-theme.X(24),ImGui::GetIO().DisplaySize.y-theme.Y(24)});
    HelpyImGuiTheme::CenterNextWindow();
    if(ImGui::Begin("Preview Window###shared-item-preview",&ItemPreviewRequest::Open)){
        ImGui::TextWrapped("%s",ItemPresentation::Name(ItemPreviewRequest::Item).c_str());const char* qualities[]={"Economy","Balanced","Detail"};
        if(ImGui::Combo("Quality",&Quality,qualities,3)){ItemPreviewRequest::Requested=false;++ItemPreviewRequest::Revision;}
        if(CharacterPreviewPanel::Reconnect){ItemPreviewRequest::Requested=false;CharacterPreviewPanel::Reconnect=false;++ItemPreviewRequest::Revision;}
        const auto id=AuthoringClient::Snapshot.value("RequestId",uint64_t{});
        if(id&&id==Receipt&&!AuthoringClient::Snapshot.value("Busy",false)&&AuthoringClient::Snapshot.value("Operation",std::string{})=="SaveEditorRender"){
            if(ReceiptRevision==ItemPreviewRequest::Revision){const auto& result=AuthoringClient::Result("SaveEditorRender");
            if(result.contains("Error"))CharacterPreviewPanel::Fail(result.at("Error").get<std::string>());
            else if(result.contains("Channel"))try{CharacterPreviewPanel::Attach(result.at("Channel").get<std::string>());}catch(const std::exception& e){CharacterPreviewPanel::Status=e.what();}}Receipt=0;
        }
        if(active&&!ItemPreviewRequest::Requested&&!AuthoringClient::Busy()){
            CharacterPreviewPanel::Close();
            if(AuthoringClient::Submit("SaveEditorRender",{{"View","Item"},{"Path",ItemPreviewRequest::Item.at("Path")},{"Quality",Quality}})){Receipt=AuthoringClient::Pending;ReceiptRevision=ItemPreviewRequest::Revision;ItemPreviewRequest::Requested=true;}
        }
        CharacterPreviewPanel::Draw(std::min(theme.Y(400),std::max(theme.Y(160),ImGui::GetContentRegionAvail().y-theme.Y(155))));
        if(!AuthoringClient::Error.empty())ImGui::TextWrapped("%s",AuthoringClient::Error.c_str());
        if(!CharacterPreviewPanel::DroppedItem.empty()){ItemPreviewRequest::Show(CharacterPreviewPanel::DroppedItem);CharacterPreviewPanel::DroppedItem=nlohmann::json::object();}
        if(ImGui::Button("Refresh preview")){ItemPreviewRequest::Requested=false;++ItemPreviewRequest::Revision;}ImGui::TextDisabled("Display mesh only · no item is granted, equipped or used");
    }ImGui::End();if(!ItemPreviewRequest::Open){CharacterPreviewPanel::Close();CharacterPreviewPanel::Reconnect=true;Receipt=0;}
}
}
