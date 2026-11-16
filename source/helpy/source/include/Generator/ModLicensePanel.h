#pragma once
#include "Core/ModLicenses.h"
#include "Core/ProcessExitFuture.h"
#include "Generator/HelpyMenu.h"
#include "ModDownloadLink.h"

namespace PS::ModLicensePanel {
namespace fs=std::filesystem;
inline fs::path Root,Folder;
inline std::string SelectedName,Website,Selection,Reading;
inline ModLicenses::Result Data;
inline ProcessExitFuture<ModLicenses::Result> Worker;
inline bool Requested=false;
inline size_t NoticeIndex=0;
inline std::string Key(const fs::path& path){return ModManager::Order::Fold(path.lexically_normal().generic_string());}
inline void Context(){const auto root=HostServices::ModDirectory();if(Key(root)!=Key(Root)){Root=root;Folder.clear();Selection.clear();SelectedName.clear();Website.clear();Data={};Requested=false;NoticeIndex=0;}}
inline bool Selected(const nlohmann::json& row){return !Selection.empty()&&Selection==Key(fs::u8path(row.value("Folder",std::string{})));}
inline void Select(const nlohmann::json& row,const std::string& name){
    const auto folder=fs::u8path(row.value("Folder",std::string{}));const auto key=Key(folder);if(key.empty())return;
    SelectedName=name;Website=row.value("Metadata",nlohmann::json::object()).value("website",std::string{});
    if(Selection==key)return;Folder=folder;Selection=key;Data={};Requested=true;NoticeIndex=0;
}
inline void Title(const nlohmann::json& row,const std::string& name,ImVec2 size){
    if(ImGui::InvisibleButton("##mod-title",size,ImGuiButtonFlags_EnableNav))Select(row,name);
}
inline void Outline(ImVec2 a,ImVec2 b){const auto& theme=HelpyImGuiTheme::Current;HelpyFocusHighlight::Draw(ImGui::GetWindowDrawList(),a,b,ImGui::GetColorU32(theme.accent),ImGui::GetStyle().ChildRounding,false,theme.Y(2));}
inline void Render(const std::function<void(const std::string&)>& open){
    ImGui::SeparatorText("Mod license");ImGui::TextWrapped("%s",SelectedName.c_str());
    if(Selection.empty()){ImGui::TextWrapped("Select an installed mod to view its license.");return;}
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=Worker.get();if(Reading==Selection){Data=std::move(result);NoticeIndex=0;}}
    ImGui::BeginDisabled(Worker.valid());if(HelpyImGuiTheme::SoftButton("Refresh license")){Data={};Requested=true;NoticeIndex=0;}ImGui::EndDisabled();
    if(ModDownloadLink::Valid(Website)){HelpyMenu::NextControl("Open mod page");if(HelpyImGuiTheme::SoftButton("Open mod page"))open(Website);}
    if(Requested&&!Worker.valid()){const auto root=Root,folder=Folder;Reading=Selection;Requested=false;Worker=std::async(std::launch::async,[root,folder]{try{return ModLicenses::Read(root,folder);}catch(const std::exception& error){return ModLicenses::Result{{},{error.what()}};}});}
    if(Worker.valid()||Requested){ImGui::TextDisabled("Reading installed license files...");return;}
    if(Data.Notices.empty())ImGui::TextWrapped("No readable license file was found in this mod. Check the author's mod page for permissions and conditions.");
    for(const auto& issue:Data.Issues)ImGui::TextWrapped("%s",issue.c_str());
    if(!Data.Notices.empty()){
        NoticeIndex=std::min(NoticeIndex,Data.Notices.size()-1);
        if(Data.Notices.size()>1&&ImGui::BeginCombo("License file",Data.Notices[NoticeIndex].Name.c_str())){
            for(size_t i=0;i<Data.Notices.size();++i)if(ImGui::Selectable(Data.Notices[i].Name.c_str(),NoticeIndex==i))NoticeIndex=i;ImGui::EndCombo();
        }
        const auto& notice=Data.Notices[NoticeIndex];ImGui::SeparatorText(notice.Name.c_str());
        if(HelpyImGuiTheme::SoftButton("Copy license"))ImGui::SetClipboardText(notice.Text.c_str());ImGui::TextWrapped("%s",notice.Text.c_str());
    }
}
inline void Shutdown(){if(Worker.valid())try{(void)Worker.get();}catch(...){};}
}
