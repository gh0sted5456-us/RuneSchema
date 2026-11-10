#pragma once
#include "Generator/HelpyTextInput.h"
#include "SaveWorldBackups.h"
#include "Core/ProcessExitFuture.h"
#include "Core/HelpyEnvironment.h"
#include "Core/RuneSchemaDataRoot.h"
#include "Generator/HelpyMenu.h"
#include "Core/HelpyGameProcess.h"
#include <set>

namespace PS::SaveRecoveryPanel {
using HelpyGameProcess::GameStopped;
namespace fs=std::filesystem;using Json=nlohmann::json;
struct Row {SaveBackups::Entry Backup;bool World=false;std::string Slot;fs::path Target;std::string RequestState;};
struct Data {std::vector<Row> Rows;bool Stopped=false,Automatic=false,WorldRecovery=false;std::string Runtime,Status;};
inline Data Current;inline ProcessExitFuture<Data> Work;inline bool Initialized=false,Confirm=false;
inline std::string Platform,Selected,Status;inline char Query[257]{};inline int Filter=0,NarrowSection=0;
#if defined(HELPY_PREVIEW)
inline fs::path PreviewRoot;
#endif
inline fs::path Root(){
#if defined(HELPY_PREVIEW)
    if(!PreviewRoot.empty())return PreviewRoot;
#endif
    return RuneSchemaData::Directory();
}
inline std::string DefaultPlatform(){using K=HelpyEnvironment::Kind;switch(HelpyEnvironment::Current){case K::Steam:return "steam";case K::Epic:return "epic";case K::GamePass:return "gamepass";case K::Dedicated:return "server";default:return {};}}
inline fs::path WorldRoot(const std::string& runtime){
    if(runtime!="server")return RuneSchemaData::GameSavedDirectory()/"SaveGames";
    const auto binaries=HostServices::ModDirectory().parent_path().parent_path().parent_path();
    auto name=binaries.filename().wstring();for(auto& c:name)c=towlower(c);
    if(name!=L"win64"||binaries.parent_path().filename()!=L"Binaries")throw std::runtime_error("Open the dedicated server's own Helpy to recover its worlds");
    return binaries.parent_path().parent_path()/"Saved/SaveGames";
}
inline std::string State(const fs::path& directory,const std::string& guid,const std::string& hash){
    const auto pending=SaveBackups::Pending(directory,guid);CacheBundleIO::Safe(pending);
    if(fs::exists(pending))return "Queued for next entry";
    if(fs::exists(fs::path(pending.wstring()+L".processing")))return "Needs review; not retried";
    const auto receipt=fs::path(pending.wstring()+L".processing.loaded");
    if(fs::exists(receipt))try{const auto saved=Json::parse(CacheBundleIO::Read(receipt,4096));if(saved.value("SHA256",std::string{})==hash)return "This snapshot was loaded at an entry; native saving remains game-owned";}catch(...){}
    return {};
}
inline Data Scan(const fs::path& root,const std::string& runtime){
    Data data;data.Runtime=runtime;data.Stopped=HelpyGameProcess::GameStopped();const auto characters=SaveBackups::Directory(root,runtime),worlds=WorldBackups::Directory(root,runtime);
    data.Automatic=SaveBackups::Automatic(characters);
    data.WorldRecovery=WorldBackups::Enabled(worlds);
    std::map<std::string,fs::path> targets;std::set<std::string> duplicates;
    if(runtime=="steam"||runtime=="epic"){
        const auto saves=RuneSchemaData::GameSavedDirectory()/"SaveCharacters";CacheBundleIO::Safe(saves);size_t files=0,bytes=0;
        if(fs::is_directory(saves))for(const auto& file:fs::directory_iterator(saves)){if(++files>256)throw std::runtime_error("Character directory exceeds the recovery scan limit");if(file.path().extension()!=L".json"||!file.is_regular_file())continue;
            const auto size=file.file_size();if(size>SaveBackups::MaximumBytes||bytes+size>64*1024*1024)continue;bytes+=size;
            try{const auto guid=SaveBackups::Identity(SaveBackups::Decode(CacheBundleIO::Read(file.path(),SaveBackups::MaximumBytes)));if(guid.empty())continue;if(!targets.emplace(guid,file.path()).second)duplicates.insert(guid);}catch(...){}
        }
    }
    for(auto backup:SaveBackups::List(characters)){Row row;row.Backup=std::move(backup);if(targets.contains(row.Backup.Guid)&&!duplicates.contains(row.Backup.Guid))row.Target=targets.at(row.Backup.Guid);row.RequestState=State(characters,row.Backup.Guid,row.Backup.Hash);data.Rows.push_back(std::move(row));}
    if(data.WorldRecovery)for(auto backup:WorldBackups::List(worlds)){Row row;row.World=true;row.Slot=backup.Slot;row.Backup=static_cast<SaveBackups::Entry&>(backup);
        if(runtime!="gamepass")try{const auto target=WorldRoot(runtime)/(fs::u8path(row.Slot).wstring()+L".sav");CacheBundleIO::Safe(target);if(fs::is_regular_file(target))row.Target=target;}catch(...){}
        row.RequestState=State(worlds,row.Backup.Guid,row.Backup.Hash);data.Rows.push_back(std::move(row));
    }
    std::stable_sort(data.Rows.begin(),data.Rows.end(),[](const auto& a,const auto& b){return a.Backup.CreatedUtc>b.Backup.CreatedUtc;});data.Status="Found "+std::to_string(data.Rows.size())+" save snapshots.";return data;
}
inline bool Busy(){return Work.valid();}
template<class F> inline void Start(F action){if(Busy())return;Work=std::async(std::launch::async,std::move(action));}
inline void Refresh(){if(Platform.empty())return;const auto root=Root();const auto runtime=Platform;Initialized=true;Confirm=false;Status="Reading backup metadata...";Start([root,runtime]{return Scan(root,runtime);});}
inline void Poll(){if(!Work.valid()||Work.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;try{Current=Work.get();Status=Current.Status;}catch(const std::exception& error){Status=error.what();}}
inline std::string Key(const Row& row){return std::string(row.World?"world/":"character/")+row.Backup.Guid+"/"+row.Backup.Hash;}
inline const Row* Chosen(){for(const auto& row:Current.Rows)if(Key(row)==Selected)return &row;return nullptr;}
inline std::string Reasons(const SaveBackups::Entry& entry){std::string text;if(entry.Reasons.is_array())for(const auto& value:entry.Reasons)if(value.is_string()){if(!text.empty())text+=", ";text+=value.get<std::string>();}return text.empty()?"Legacy backup; reason not recorded":text;}
inline void Information(const Row& row){
    const auto& b=row.Backup;HelpyImGuiTheme::DetailsTextUnformatted(b.Name.c_str());
    HelpyImGuiTheme::DetailsText("%s · %s",row.World?"World":"Character",Current.Runtime.c_str());
    HelpyImGuiTheme::DetailsText("Snapshot: %s",b.CreatedUtc.empty()?"Unknown (older backup)":b.CreatedUtc.c_str());
    HelpyImGuiTheme::DetailsText("Last played: %s",b.LastPlayedUtc.empty()?"Unknown (not recorded)":b.LastPlayedUtc.c_str());
    HelpyImGuiTheme::DetailsText("Reason: %s",Reasons(b).c_str());
    HelpyImGuiTheme::DetailsText("%s: %s",row.World?"World slot":"Character GUID",row.World?row.Slot.c_str():b.Guid.c_str());
    if(!row.RequestState.empty())HelpyImGuiTheme::DetailsText("%s",row.RequestState.c_str());
    ImGui::Separator();HelpyImGuiTheme::DetailsTextUnformatted("Mods active when this snapshot was recorded");
    if(!b.Mods.is_array())HelpyImGuiTheme::DetailsTextDisabled("No reliable mod record is available for this snapshot.");
    else if(b.Mods.empty())HelpyImGuiTheme::DetailsTextDisabled("No RuneSchema mods were recorded.");
    else for(const auto& mod:b.Mods)if(mod.is_object()){
        const auto field=[&](const char* key,const std::string& fallback){const auto it=mod.find(key);return it!=mod.end()&&it->is_string()?it->get<std::string>().substr(0,1024):fallback;};
        const auto name=field("Name",field("Id","Unknown mod")),version=field("Version",{});
        HelpyImGuiTheme::DetailsText("%s%s%s",name.c_str(),version.empty()?"":" · ",version.c_str());
    }
    HelpyImGuiTheme::DetailsTextDisabled("Mod files are not backed up. Reacquire and enable the required mods before loading the restored save.");
}
inline void Restore(const Row& row){const auto root=Root();const auto runtime=Platform;Start([root,runtime,row]{
    if(row.World){const auto directory=WorldBackups::Directory(root,runtime);WorldBackups::Entry entry;static_cast<SaveBackups::Entry&>(entry)=row.Backup;entry.Slot=row.Slot;
        if(runtime=="gamepass")WorldBackups::QueueRestore(directory,entry,GameStopped);
        else{if(row.Target.empty())throw std::runtime_error("The matching current world file was not found; no target is guessed");const auto current=CacheBundleIO::Read(row.Target,WorldBackups::MaximumWorldBytes);WorldBackups::RestoreFile(directory,entry,WorldRoot(runtime),CacheBundleIO::Hash(current),GameStopped);}
    }else{
        const auto directory=SaveBackups::Directory(root,runtime);
        if(runtime=="server")throw std::runtime_error("Restore a player's character from that player's own Helpy installation");
        if(runtime=="gamepass")SaveBackups::QueueRestore(directory,row.Backup,GameStopped);
        else{if(row.Target.empty())throw std::runtime_error("A unique matching native character was not found; no target is guessed");const auto current=CacheBundleIO::Read(row.Target,SaveBackups::MaximumBytes);SaveBackups::RestoreFile(directory,row.Backup,row.Target.parent_path(),row.Target,CacheBundleIO::Hash(current),GameStopped);}
    }
    auto result=Scan(root,runtime);result.Status=runtime=="gamepass"?"Restore queued for this exact character/world's next native entry. The previous payload will be backed up first.":"Save restored and verified. The previous file was backed up before replacement.";return result;
});}
inline void Render(){
    Poll();if(Platform.empty())Platform=DefaultPlatform();if(!Initialized&&!Platform.empty())Refresh();const auto& theme=HelpyImGuiTheme::Current;
    ImGui::BeginDisabled(Busy());ImGui::SetNextItemWidth(std::min(theme.X(190),ImGui::GetContentRegionAvail().x));
    if(ImGui::BeginCombo("##recovery-runtime",Platform.empty()?"Choose runtime":Platform.c_str())){for(const auto* runtime:{"steam","gamepass","epic","server"})if(ImGui::Selectable(runtime,Platform==runtime)){Platform=runtime;Selected.clear();Refresh();}ImGui::EndCombo();}
    ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Refresh"))Refresh();
    bool automatic=Current.Automatic;if(HelpyImGuiTheme::Toggle(Current.WorldRecovery?"Automatic snapshots at character/world entry":"Automatic snapshots at character entry",&automatic)&&!Platform.empty()){const auto root=Root();const auto runtime=Platform;Start([root,runtime,automatic]{SaveBackups::SetAutomatic(SaveBackups::Directory(root,runtime),automatic);return Scan(root,runtime);});}
    ImGui::EndDisabled();ImGui::TextWrapped("Before-cleanup backups stay enabled. Entry snapshots are optional; no recurring save polling or mod-file copies are used.");
    ImGui::SetNextItemWidth(-1);HelpyTextInput::InputTextWithHint("##recovery-search","Search character, world or GUID...",Query,sizeof(Query));
    if(Current.WorldRecovery){constexpr std::array filters{"All saves","Characters","Worlds"};HelpyMenu::Tabs("recovery-kind",filters,Filter);}
    else{Filter=1;ImGui::TextWrapped("World recovery is experimental. Enable it in Settings > Helpy > Saves, then refresh this list.");}
    if(!Status.empty())ImGui::TextWrapped("%s%s",Busy()?"Working · ":"",Status.c_str());
    const bool wide=ImGui::GetContentRegionAvail().x>=theme.X(700);
    if(!wide){constexpr std::array tabs{"Backups","Details"};HelpyMenu::Tabs("recovery-layout",tabs,NarrowSection);}
    const bool table=wide&&ImGui::BeginTable("##recovery-panes",2,ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchProp);
    if(table){ImGui::TableSetupColumn("Backups",ImGuiTableColumnFlags_WidthStretch,.48f);ImGui::TableSetupColumn("Details",ImGuiTableColumnFlags_WidthStretch,.52f);ImGui::TableNextColumn();}
    if(wide||NarrowSection==0){if(HelpyImGuiTheme::BeginPanel("##recovery-list",{0,0},true,0,true)){
        for(const auto& row:Current.Rows){if((Filter==1&&row.World)||(Filter==2&&!row.World))continue;const auto haystack=row.Backup.Name+" "+row.Backup.Guid+" "+row.Slot;if(*Query&&LiveLog::Lower(haystack).find(LiveLog::Lower(Query))==std::string::npos)continue;
            const auto key=Key(row),label=row.Backup.Name+"\n"+(row.Backup.CreatedUtc.empty()?"Older backup":row.Backup.CreatedUtc)+" · "+Reasons(row.Backup);ImGui::PushID(key.c_str());
            if(ImGui::Selectable(label.c_str(),Selected==key,0,{0,ImGui::GetTextLineHeightWithSpacing()*3})){Selected=key;Confirm=false;if(!wide)NarrowSection=1;}
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::RecoveryRows[key]=HelpyPreviewHooks::LastRect();
#endif
            if(ImGui::IsItemHovered()){HelpyImGuiTheme::BeginDetailsTooltip(440);Information(row);HelpyImGuiTheme::EndDetailsTooltip();}ImGui::PopID();
        }if(Current.Rows.empty())ImGui::TextWrapped("No snapshots found for this runtime. New backups will appear here after cleanup or an entry with automatic snapshots enabled.");
    }PS::HelpyImGuiTheme::EndPanel();}
    if(table)ImGui::TableNextColumn();
    if(wide||NarrowSection==1){if(HelpyImGuiTheme::BeginPanel("##recovery-details",{0,0},true,0,true)){
        if(const auto* row=Chosen()){
            const bool review=row->RequestState.starts_with("Queued")||row->RequestState.starts_with("Needs review");
            const float footer=(review?2.f:1.f)*ImGui::GetFrameHeightWithSpacing()+ImGui::GetStyle().ItemSpacing.y;
            if(HelpyImGuiTheme::BeginPanel("##snapshot-information",{0,std::max(1.f,ImGui::GetContentRegionAvail().y-footer)},false)){
            Information(*row);ImGui::Separator();ImGui::TextWrapped("Close Dragonwilds before restoring. Existing saves are preserved first. Normal cleanup still applies when missing mods are not reinstalled.");
            if(!Current.Stopped)ImGui::TextWrapped("Dragonwilds was running during the last check. Close it, then refresh this list.");
            }PS::HelpyImGuiTheme::EndPanel();
            if(review){
                ImGui::BeginDisabled(!Current.Stopped||Busy());
                if(HelpyImGuiTheme::SoftButton("Cancel restore request")){
                    const auto root=Root();const auto runtime=Platform;const auto guid=row->Backup.Guid;const bool world=row->World;
                    Start([root,runtime,guid,world]{SaveBackups::CancelRequest(world?WorldBackups::Directory(root,runtime):SaveBackups::Directory(root,runtime),guid,GameStopped);auto result=Scan(root,runtime);result.Status="Restore request cancelled; backups and game saves were retained.";return result;});
                }ImGui::EndDisabled();
            }
            const bool unavailable=!Current.Stopped||Busy()||(!row->World&&Platform=="server")||(Platform!="gamepass"&&row->Target.empty());
            ImGui::BeginDisabled(unavailable);if(HelpyImGuiTheme::SoftButton(Platform=="gamepass"?"Queue restore for next entry":"Restore save")){Confirm=false;ImGui::OpenPopup("Confirm save recovery");}
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::RecoveryRestore=HelpyPreviewHooks::LastRect();
#endif
            ImGui::EndDisabled();
            if(HelpyImGuiTheme::BeginCenteredModal("Confirm save recovery",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
                ImGui::TextWrapped("Restore %s from %s? The current save is backed up before any replacement. Reacquire required mods yourself.",row->Backup.Name.c_str(),row->Backup.CreatedUtc.empty()?"this older snapshot":row->Backup.CreatedUtc.c_str());
                ImGui::Checkbox("I have reviewed the selected save and required mods",&Confirm);ImGui::BeginDisabled(!Confirm||Busy());
                if(ImGui::Button("Restore")){Restore(*row);ImGui::CloseCurrentPopup();}ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
            }
        }else ImGui::TextWrapped("Select a snapshot to review its save identity, backup reason and mod record.");
    }PS::HelpyImGuiTheme::EndPanel();}if(table)ImGui::EndTable();
}
}
