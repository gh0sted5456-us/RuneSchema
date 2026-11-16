#pragma once
#include "Core/ModSaveFiles.h"
#include "Generator/ModMetadataPanel.h"
#include "Generator/SaveRecoveryPanel.h"

namespace PS::ModSaveFilesPanel {
namespace fs=std::filesystem;using Json=nlohmann::json;
inline fs::path Folder,Output;inline ModSaveFiles::Roots Roots;inline std::string Owner,Version,Status;
inline Json Current;inline ProcessExitFuture<Json> Worker;inline bool Requested=false;
inline fs::path Roaming(){
    const auto count=GetEnvironmentVariableW(L"APPDATA",nullptr,0);if(!count||count>32768)return {};
    std::vector<wchar_t> value(count);if(GetEnvironmentVariableW(L"APPDATA",value.data(),count)+1!=count)return {};return fs::path(value.data());
}
inline void Open(const Json& snapshot,const Json& row){
    if(Worker.valid())return;Requested=true;Current=Json();Status.clear();Output.clear();Owner=row.at("Name").get<std::string>();Version=row.at("Metadata").value("version",std::string{});
    try{
        const auto root=fs::u8path(snapshot.at("Root").get<std::string>());
        if(root.lexically_normal()!=HostServices::ModDirectory().lexically_normal())throw std::runtime_error("Open this installation's own Helpy to manage its mod saves.");
        Folder=HelpyModMetadata::Folder(root,Owner);const auto platform=SaveRecoveryPanel::DefaultPlatform();
        const auto saved=platform=="server"?SaveRecoveryPanel::WorldRoot(platform).parent_path():RuneSchemaData::GameSavedDirectory();
        Roots={{"Mod",Folder},{"GameSaved",saved},{"LocalAppData",RuneSchemaData::LocalDirectory()},{"RoamingAppData",Roaming()}};
        const auto folder=Folder;const auto roots=Roots;Worker=std::async(std::launch::async,[folder,roots]{try{return ModSaveFiles::Scan(folder,roots);}catch(const std::exception& error){return Json{{"Error",error.what()}};}});
    }catch(const std::exception& error){Status=error.what();}
}
inline void Render(){
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){const auto result=Worker.get();
        if(result.contains("Error"))Status=result.at("Error").get<std::string>();
        else if(result.contains("Output")){Output=fs::u8path(result.at("Output").get<std::string>());Status="Backup verified and saved in AppData.";}
        else{Current=result;Status.clear();}
    }
    if(Requested){Requested=false;ImGui::OpenPopup("Mod saves");}
    const auto& t=HelpyImGuiTheme::Current;const auto area=ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(t.X(600),std::max(1.f,area.x-t.X(24))),std::min(t.Y(450),std::max(1.f,area.y-t.Y(24)))},ImGuiCond_Appearing);
    if(!HelpyImGuiTheme::BeginCenteredModal("Mod saves",nullptr,ImGuiWindowFlags_NoSavedSettings))return;
    ImGui::TextWrapped("%s",Owner.c_str());ImGui::TextWrapped("Review the declared locations before backing up. Files keep their mod-owned format. Close Dragonwilds and any other application writing these saves first.");
    const float reserve=ImGui::GetFrameHeightWithSpacing()*3+t.Y(44);
    if(HelpyImGuiTheme::BeginPanel("declared-save-locations",{0,std::max(t.Y(50),ImGui::GetContentRegionAvail().y-reserve)},true)){
        if(Current.is_object()&&Current.contains("Resolved")){
            if(Current["Resolved"].empty())ImGui::TextWrapped("This mod has no save declarations. Add SaveFileName, SaveFileExtension, SaveFileRoot and SaveFilePath to ID.txt, or provide cache/savefiles.json.");
            for(const auto& entry:Current["Resolved"]){ImGui::TextWrapped("%s",entry.at("Path").get<std::string>().c_str());ImGui::TextDisabled(entry.at("Exists").get<bool>()?"Found":"Not created yet");ImGui::Separator();}
        }
    }HelpyImGuiTheme::EndPanel();
    const bool found=Current.is_object()&&Current.contains("Resolved")&&std::any_of(Current["Resolved"].begin(),Current["Resolved"].end(),[](const auto& row){return row.at("Exists").template get<bool>();});
    ImGui::BeginDisabled(Worker.valid()||!found);
    if(HelpyImGuiTheme::SoftButton("Back up declared saves")){
        const auto folder=Folder;const auto roots=Roots;const auto expected=Current;const auto mod=Owner,version=Version;const auto destination=HostServices::CacheDirectory().parent_path()/"mod-save-backups";Status="Verifying and copying saves...";
        Worker=std::async(std::launch::async,[folder,roots,expected,mod,version,destination]{try{return Json{{"Output",ModSaveFiles::Utf8(ModSaveFiles::Backup(folder,roots,destination,mod,version,expected,SaveRecoveryPanel::GameStopped))}};}catch(const std::exception& error){return Json{{"Error",error.what()}};}});
    }ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(Worker.valid());if(HelpyImGuiTheme::SoftButton("Close"))ImGui::CloseCurrentPopup();ImGui::EndDisabled();
    if(!Output.empty()){ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Open backup folder"))ShellExecuteW(nullptr,L"open",Output.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
    if(Worker.valid())ImGui::TextDisabled("Working...");if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());ImGui::EndPopup();
}
inline void Shutdown(){if(Worker.valid())try{(void)Worker.get();}catch(...){};}
}
