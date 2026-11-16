#pragma once
#include "Core/HelpyModMetadata.h"
#include "Core/ProcessExitFuture.h"
#include "Generator/HelpyTextInput.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Runtime/HostServices.h"
#include <array>
#include <future>

namespace PS::ModMetadataPanel {
using Json=nlohmann::json;inline std::filesystem::path Root;inline std::string Owner,Origin,Status;
inline Json Expected;inline std::array<char,65537> Text{};inline bool Requested=false,Ready=false,Track=false,Writing=false,Saved=false;
inline ProcessExitFuture<Json> Worker;
inline void Open(const Json& snapshot,const Json& row){
    if(Worker.valid())return;Root=std::filesystem::u8path(snapshot.at("Root").get<std::string>());
    Owner=row.at("Name").get<std::string>();Origin=row.value("Origin",std::string("Not confirmed"));Status.clear();Ready=false;Requested=true;Writing=false;
    if(Root.lexically_normal()!=HostServices::ModDirectory().lexically_normal()){Status="Metadata editing is limited to Helpy's assigned local installation.";return;}
    const auto root=Root,data=HostServices::CacheDirectory().parent_path();const auto owner=Owner;
    Worker=std::async(std::launch::async,[root,owner,data]{try{return HelpyModMetadata::Read(root,owner,data);}catch(const std::exception& e){return Json{{"Error",e.what()}};}});
}
inline void Render(){
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=Worker.get();if(result.contains("Error")){Status=result["Error"].get<std::string>();}
        else if(Writing){Status="ID.txt saved. Runtime metadata changes apply at the next game launch; Helpy's view refreshes now.";Saved=true;Ready=false;}
        else{Expected=result;const auto text=result.at("Text").get<std::string>();std::copy(text.begin(),text.end(),Text.begin());Text[text.size()]=0;Track=result.value("Track",false);Ready=true;}Writing=false;
    }
    if(Requested){Requested=false;ImGui::OpenPopup("Mod identity — ID.txt");}
    const auto& t=HelpyImGuiTheme::Current;const auto display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize({std::min(t.X(650),display.x-t.X(24)),std::min(t.Y(600),display.y-t.Y(24))},ImGuiCond_Appearing);HelpyImGuiTheme::CenterNextWindow();
    if(HelpyImGuiTheme::BeginCenteredModal("Mod identity — ID.txt",nullptr,ImGuiWindowFlags_NoSavedSettings)){
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::MetadataEditorBounds=HelpyPreviewHooks::WindowRect();
#endif
        ImGui::TextWrapped("%s · %s",Owner.c_str(),Origin.c_str());
        ImGui::TextWrapped("Edit the installed ID.txt. Keep the mod ID stable and leave Version blank when unknown. Existing comments and unknown fields remain in the editor.");
        if(Origin!="Not confirmed")ImGui::TextWrapped("Your manager may replace deployed edits during an update. Preserve this metadata through the manager's external-change workflow; Helpy does not modify its staging database.");
        ImGui::BeginDisabled(!Ready||Worker.valid());
        const float reserve=ImGui::GetFrameHeightWithSpacing()*4+t.Y(54);
        HelpyTextInput::InputTextMultiline("##mod-id-text",Text.data(),Text.size(),{-1,std::max(t.Y(80),ImGui::GetContentRegionAvail().y-reserve)});
        HelpyImGuiTheme::Toggle("Track local content changes",&Track);
        if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("For a locally created ID: retain a change fingerprint in .helpy-source and AppData. This is an update hint, not an integrity or gameplay check.");
        if(HelpyImGuiTheme::SoftButton("Save ID.txt")){
            const auto root=Root,data=HostServices::CacheDirectory().parent_path();const auto owner=Owner,text=std::string(Text.data());const auto expected=Expected;const bool track=Track;Writing=true;
            Worker=std::async(std::launch::async,[root,data,owner,expected,text,track]{try{HelpyModMetadata::Save(root,owner,data,expected,text,track);return Json{{"Saved",true}};}catch(const std::exception& e){return Json{{"Error",e.what()}};}});
        }ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(Worker.valid());if(HelpyImGuiTheme::SoftButton("Close"))ImGui::CloseCurrentPopup();ImGui::EndDisabled();
        if(Worker.valid())ImGui::TextDisabled(Writing?"Saving metadata...":"Reading metadata...");if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());ImGui::EndPopup();
    }
}
inline void Shutdown(){if(Worker.valid())try{(void)Worker.get();}catch(...){};}
}
