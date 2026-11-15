#pragma once
#include "Core/HelpyNotificationFile.h"
#include "Runtime/HelpyModMetadataWatch.h"
#include "Generator/ModsPanel.h"
#include "Generator/HelpyStartupProgress.h"
#include "Runtime/HelpyUpdates.h"
#include "Core/HelpyHotfixInfo.h"
#include <ctime>

namespace PS::NotificationsPanel {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline HelpyNotifications::Store Inbox;
inline HelpyNotifications::File Storage;
struct DiskResult {HelpyNotifications::File Storage;HelpyNotifications::Store Data;std::string Error;uint64_t Revision=0;bool Loading=false;Json Applied;};
inline ProcessExitFuture<DiskResult> Disk;
inline bool Initialized=false,Loaded=false,Refresh=false,VanillaPrompt=false;
inline uint64_t Saved=0,Observed=~uint64_t{},Generation=~uint64_t{};
inline std::string Status,LastJob,ImportJob,ImportSource,ImportVersion,ImportName,ObservedGame,ImportFingerprint;inline bool ImportVanilla=false;
inline HelpyModMetadataWatch Watch;
inline std::chrono::steady_clock::time_point RefreshAt{},SaveAt{};
inline bool HotfixPrompt=false;inline Json SelectedHotfix,AppliedHotfix;inline std::string SelectedHotfixId,ObservedHotfix;
inline fs::path HotfixReceipt(){return Storage.Path.parent_path()/"hotfix-applied.json";}
inline uint64_t Now(){return uint64_t(std::time(nullptr));}
inline std::string GameStamp(){
    if(HelpyEnvironment::GameVersion.empty()||HelpyEnvironment::GameVersion=="unknown"||HelpyEnvironment::Current==HelpyEnvironment::Kind::Unknown)return {};
    auto result=std::string(HelpyEnvironment::Label())+" "+HelpyEnvironment::GameVersion;
    const auto& steam=HelpyEnvironment::Steam;if(steam.contains("BuildId")&&steam["BuildId"].is_number_integer()&&steam["BuildId"]>0)result+=" ("+std::to_string(steam["BuildId"].get<int64_t>())+")";return result;
}
inline void DiskPoll(){
    if(!Initialized){Initialized=true;Storage.Path=HostServices::CacheDirectory().parent_path()/"notifications.json";const auto file=Storage;
        Disk=std::async(std::launch::async,[file]()mutable{DiskResult r;r.Storage=file;r.Loading=true;try{r.Data=r.Storage.Load();}catch(const std::exception& e){r.Error=e.what();}
            try{const auto receipt=file.Path.parent_path()/"hotfix-applied.json";RuneSchemaData::Safe(receipt);if(fs::is_regular_file(receipt))r.Applied=Json::parse(ConfigFiles::Read(receipt,4096));}catch(...){}return r;});}
    if(Disk.valid()&&Disk.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto r=Disk.get();Storage=std::move(r.Storage);Status=std::move(r.Error);if(!Status.empty())Storage.Ready=false;if(r.Loading){Inbox=std::move(r.Data);AppliedHotfix=std::move(r.Applied);Loaded=true;Observed=~uint64_t{};}Saved=r.Revision;}
    if(Loaded&&Storage.Ready&&!Disk.valid()&&Inbox.Revision!=Saved&&std::chrono::steady_clock::now()>=SaveAt){
        const auto file=Storage;const auto data=Inbox;SaveAt=std::chrono::steady_clock::now()+std::chrono::seconds(1);
        Disk=std::async(std::launch::async,[file,data]()mutable{DiskResult r;r.Storage=file;r.Revision=data.Revision;try{r.Storage.Save(data);}catch(const std::exception& e){r.Error=e.what();}return r;});
    }
}
inline void Observe(){
    ModsPanel::Poll();const auto generation=SpawnToolRequests::CurrentGeneration();
    if(ModMetadataPanel::Saved){ModMetadataPanel::Saved=false;Refresh=true;RefreshAt={};}
    if(Generation!=generation){Generation=generation;Refresh=true;RefreshAt={};}
    Watch.Watch(HostServices::ModDirectory());if(Watch.Changed()){Refresh=true;RefreshAt=std::chrono::steady_clock::now()+std::chrono::milliseconds(600);}
    if(Refresh&&!ModsPanel::Dirty&&!ModsPanel::Worker.valid()&&RuneSchemaSettingsPanel::Remote&&std::chrono::steady_clock::now()>=RefreshAt){Refresh=false;ModsPanel::Start({{"Action","ModsRead"}});}
    if(!Loaded||ModsPanel::SnapshotGeneration!=generation||!ModsPanel::Snapshot.contains("Rows"))return;
    const auto game=GameStamp();if(Observed==ModsPanel::SnapshotRevision&&ObservedGame==game)return;Observed=ModsPanel::SnapshotRevision;ObservedGame=game;
    HelpyStartupProgress::SetTips(ModsPanel::Snapshot["Rows"]);
    std::vector<HelpyNotifications::Mod> mods;
    for(const auto& row:ModsPanel::Snapshot["Rows"]){const auto& meta=row.at("Metadata");const auto folder=row.value("Name",std::string{});auto id=meta.value("modId",folder);if(id.empty())id=folder;
        id=row.value("TrackingId",id);mods.push_back({HelpyCacheSources::Lower(id),meta.value("name",folder),meta.value("version",std::string{}),row.value("Enabled",false),row.value("Present",false),row.value("LocalFingerprint",std::string{})});}
    Inbox.Observe(game,mods,Now());
}
inline bool Busy(){return HelpyModCacheBuilder::Current().Running||HelpyModCacheBuilder::CurrentBatch().Active||HelpyIconPacks::Busy()||HelpyIconPacks::Building()||!ImportJob.empty();}
inline void Hotfixes(){
    if(!Loaded)return;
    const auto& updates=HelpyUpdates::Current();
    const auto release=HelpyHotfixInfo::Select(Json::array({updates.Hotfix}),HelpyEnvironment::RuneSchemaVersion);
    const auto asset=HelpyHotfixInfo::ForPlatform(release,HelpyEnvironment::Current==HelpyEnvironment::Kind::GamePass?"WinGDK":"Win64");
    if(asset.is_null())return;const auto id=HelpyHotfixInfo::Identity(release,asset),stamp=id+":"+std::to_string(updates.CheckedAt);
    if(ObservedHotfix==stamp)return;ObservedHotfix=stamp;
    Inbox.Hotfix(id,release.at("tag_name").get<std::string>(),Now());
    const auto root=HostServices::ModDirectory().u8string();
    if(AppliedHotfix.is_object()&&AppliedHotfix.value("Identity",std::string{})==id&&AppliedHotfix.value("Root",std::string{})==std::string(root.begin(),root.end()))Inbox.AppliedHotfix(id);
}
inline bool BuildEnabled(){
    if(Busy()||ModsPanel::Dirty||ModsPanel::SnapshotGeneration!=SpawnToolRequests::CurrentGeneration()||!ModsPanel::Snapshot.contains("Root"))return false;
    const bool ok=HelpyModCacheBuilder::StartEnabled(fs::u8path(ModsPanel::Snapshot.at("Root").get<std::string>()),ModsPanel::Snapshot.at("Rows"),HelpyImGuiTheme::Current.autoInstallAuthoringDotNet);
    if(ok)CacheBuilderPanel::FocusInstalled(ModsPanel::Snapshot,Json::object());return ok;
}
inline void Act(const HelpyNotifications::Notice& notice){
    if(notice.Action=="hotfix"){
        const auto release=HelpyHotfixInfo::Select(Json::array({HelpyUpdates::Current().Hotfix}),HelpyEnvironment::RuneSchemaVersion);
        const auto asset=HelpyHotfixInfo::ForPlatform(release,HelpyEnvironment::Current==HelpyEnvironment::Kind::GamePass?"WinGDK":"Win64");
        if(!asset.is_null()&&HelpyHotfixInfo::Identity(release,asset)==notice.Id){SelectedHotfix=release;SelectedHotfixId=notice.Id;HotfixPrompt=true;}
        else Status="This hotfix was superseded. Check for updates again.";return;
    }
    if(notice.Action=="resync-vanilla"){VanillaPrompt=true;return;}
    if(Busy()||ModsPanel::Dirty||!ModsPanel::Snapshot.contains("Root"))return;
    for(const auto& row:ModsPanel::Snapshot["Rows"]){const auto& meta=row.at("Metadata");const auto folder=row.value("Name",std::string{});auto id=meta.value("modId",folder);if(id.empty())id=folder;
        id=row.value("TrackingId",id);if(HelpyCacheSources::Lower(id)!=notice.Source||!row.value("Enabled",false)||!row.value("Present",false))continue;
        if(HelpyModCacheBuilder::StartInstalled(fs::u8path(ModsPanel::Snapshot.at("Root").get<std::string>()),fs::u8path(row.at("Folder").get<std::string>()),folder,HelpyImGuiTheme::Current.autoInstallAuthoringDotNet))CacheBuilderPanel::FocusInstalled(ModsPanel::Snapshot,row);return;
    }Status="This mod is no longer enabled or present. Refresh Mods before rebuilding.";
}
inline void Jobs(){
    // Keep explicit jobs moving in the tray; no idle engine indexing is started here.
    HelpyIconPacks::Poll();
    if(!ImportJob.empty()&&!HelpyIconPacks::Busy()){
        const bool success=HelpyIconPacks::LastOperationOK;
        if(success){Inbox.Built(ImportSource,ImportVersion,ImportVanilla);if(!ImportFingerprint.empty())Inbox.Built(ImportSource,"files:"+ImportFingerprint,false);}
        Inbox.Result(ImportJob,ImportSource,ImportName,success,HelpyIconPacks::Status,Now());ImportJob.clear();
    }
    HelpyModCacheBuilder::Poll(!HelpyIconPacks::Busy()&&!HelpyIconPacks::Building()&&ImportJob.empty());
    const auto& job=HelpyModCacheBuilder::Current();const auto id=job.Job.filename().string();
    if(job.Hotfix&&!job.Running&&job.State=="Succeeded"&&id!=LastJob){Inbox.AppliedHotfix(SelectedHotfixId);LastJob=id;}
    if(!job.Running&&!id.empty()&&id!=LastJob&&(job.Vanilla||!job.SourceId.empty())&&(job.State=="Failed"||job.State=="Cancelled")){
        Inbox.Result(id,job.SourceId,job.Owner,false,job.Stage,Now());LastJob=id;
    }
    if(const auto bundle=HelpyModCacheBuilder::ReadyBundle();!bundle.empty()&&!HelpyIconPacks::Busy()&&!HelpyIconPacks::Building()){
        const auto selected=HelpyIconPacks::SelectedSource;HelpyIconPacks::SelectedSource=job.Vanilla?"core":"mods/"+HelpyCacheSources::Lower(job.Owner);
        const bool accepted=HelpyIconPacks::Start(bundle,true);HelpyIconPacks::SelectedSource=selected;HelpyModCacheBuilder::BundleApplied();LastJob=id;
        if(accepted){ImportJob=id;ImportSource=job.SourceId;ImportVersion=job.SourceVersion;ImportName=job.Owner;ImportVanilla=job.Vanilla;ImportFingerprint=job.SourceFingerprint;}
        else Inbox.Result(id,job.SourceId,job.Owner,false,"Cache built; AppData import needs attention. "+HelpyIconPacks::Status,Now());
    }
}
inline bool Poll(){
    DiskPoll();Observe();Hotfixes();
    bool release=false;
    if(Loaded){
        static std::string installed,latest;
        const auto& version=HelpyEnvironment::RuneSchemaVersion;
        const auto& candidate=HelpyUpdates::Current().ReleaseVersion;
        if(version!=installed||candidate!=latest){
            installed=version;latest=candidate;
            release=Inbox.ObserveRelease(installed,latest,Now());
        }
        Jobs();
    }
    DiskPoll();return release;
}
inline void Shutdown(){Watch.Close();if(Disk.valid()){auto r=Disk.get();Storage=std::move(r.Storage);if(r.Loading)Inbox=std::move(r.Data);if(!r.Error.empty())Storage.Ready=false;Saved=r.Revision;}if(Loaded&&Storage.Ready&&Inbox.Revision!=Saved)try{Storage.Save(Inbox);}catch(...){};}
inline void Prompt(){
    if(HotfixPrompt){HotfixPrompt=false;ImGui::OpenPopup("RuneSchema emergency hotfix");}
    HelpyImGuiTheme::CenterNextWindow();ImGui::SetNextWindowSize({std::min(HelpyImGuiTheme::Current.X(620),ImGui::GetIO().DisplaySize.x-24),0},ImGuiCond_Appearing);
    if(HelpyImGuiTheme::BeginCenteredModal("RuneSchema emergency hotfix",nullptr,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings)){
        ImGui::TextWrapped("Close Dragonwilds or its server before downloading. The hotfix replaces only the listed RuneSchema base files after verifying this installation. Review the file list below before applying; Helpy will restart.");
        ImGui::BeginDisabled(Busy()||!PluginRuntimeServices::ExternalDesktop);
        if(HelpyImGuiTheme::SoftButton("Download and review hotfix"))HelpyModCacheBuilder::StartHotfix(HostServices::ModDirectory(),HelpyEnvironment::RuneSchemaVersion,SelectedHotfix,SelectedHotfixId,HotfixReceipt());
        ImGui::EndDisabled();const auto& job=HelpyModCacheBuilder::Current();
        if(job.Hotfix){ImGui::TextWrapped("%s",job.Stage.c_str());ImGui::ProgressBar(job.Percent/100.f,{-1,0});
            if(ImGui::BeginChild("Hotfix file report",{0,HelpyImGuiTheme::Current.Y(210)},true,ImGuiWindowFlags_HorizontalScrollbar))ImGui::TextUnformatted(job.Console.c_str());ImGui::EndChild();
            ImGui::BeginDisabled(job.State!="AwaitingExit");if(HelpyImGuiTheme::SoftButton("Apply and restart Helpy")&&HelpyModCacheBuilder::ApplyHotfix())PostQuitMessage(0);ImGui::EndDisabled();
            if(job.Running){HelpyMenu::NextControl("Cancel hotfix");if(HelpyImGuiTheme::SoftButton("Cancel hotfix"))HelpyModCacheBuilder::Control("stop");}
        }
        HelpyMenu::NextControl("Close");if(HelpyImGuiTheme::SoftButton("Close"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    if(VanillaPrompt){VanillaPrompt=false;ImGui::OpenPopup("Resync vanilla cache?");}
    HelpyImGuiTheme::CenterNextWindow();ImGui::SetNextWindowSize({std::min(HelpyImGuiTheme::Current.X(480),ImGui::GetIO().DisplaySize.x-24),0},ImGuiCond_Appearing);
    if(HelpyImGuiTheme::BeginCenteredModal("Resync vanilla cache?",nullptr,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings)){
        ImGui::TextWrapped("Read this installation's PAKs in a separate worker and refresh vanilla items and pictures? Your current cache stays available. The game may remain open; a full build can use disk and CPU time.");
        ImGui::BeginDisabled(Busy());if(HelpyImGuiTheme::SoftButton("Resync vanilla files")){
            if(HelpyModCacheBuilder::StartVanilla(HostServices::ModDirectory(),HostServices::CacheDirectory(),GameStamp(),HelpyImGuiTheme::Current.autoInstallAuthoringDotNet)){CacheBuilderPanel::Requested=true;ImGui::CloseCurrentPopup();}
            else Status=HelpyModCacheBuilder::Current().Stage;
        }ImGui::EndDisabled();HelpyMenu::NextControl("Later");if(HelpyImGuiTheme::SoftButton("Later"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
}
inline void Render(){
    static int category=0;constexpr std::array labels{"Updates","Cache jobs","History"};HelpyMenu::Categories("notifications",labels,category);
    if(HelpyMenu::BeginBody("Notification details",category)){
        ImGui::TextWrapped("See stable RuneSchema releases, game changes, and updates for enabled mods.");
        const auto columns=std::clamp(int(ImGui::GetContentRegionAvail().x/HelpyImGuiTheme::Current.X(172)),1,3);
        if(ImGui::BeginTable("Notification actions",columns,ImGuiTableFlags_SizingStretchSame)){
            ImGui::TableNextColumn();ImGui::BeginDisabled(!Loaded||Inbox.Notices.empty());
            if(HelpyImGuiTheme::SoftButton("Mark all read",{-1,0}))Inbox.ReadAll();ImGui::EndDisabled();
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::AuthoringButtons["notifications/read-all"]=HelpyPreviewHooks::LastRect();
#endif
            ImGui::TableNextColumn();if(HelpyImGuiTheme::SoftButton("Refresh mod versions",{-1,0})){Refresh=true;RefreshAt={};}
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::AuthoringButtons["notifications/refresh"]=HelpyPreviewHooks::LastRect();
#endif
            ImGui::TableNextColumn();ImGui::BeginDisabled(!Loaded||Inbox.Notices.empty());
            if(HelpyImGuiTheme::SoftButton("Delete all",{-1,0}))Inbox.DeleteAll();ImGui::EndDisabled();
            if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Clear notifications in every section. Active cache jobs continue.");
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::AuthoringButtons["notifications/delete-all"]=HelpyPreviewHooks::LastRect();
#endif
            ImGui::EndTable();
        }
        if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());if(!Loaded)ImGui::TextDisabled("Loading notification history...");
        size_t shown=0;for(auto it=Inbox.Notices.rbegin();it!=Inbox.Notices.rend();++it){const auto& n=*it;const bool update=n.Kind=="mod-update"||n.Kind=="game-update"||n.Kind=="hotfix"||n.Kind=="schema-update";
            if(category!=2&&(n.Dismissed||n.Resolved||!n.Active||update!=(category==0)))continue;++shown;ImGui::PushID(n.Id.c_str());ImGui::Separator();
            ImGui::TextColored(n.Read?HelpyImGuiTheme::Current.text:HelpyImGuiTheme::Current.accent,"%s",n.Title.c_str());ImGui::TextWrapped("%s",n.Detail.c_str());
            if(n.Resolved)ImGui::TextDisabled("Updated");else if(!n.Active)ImGui::TextDisabled("Source disabled, missing, or superseded");
            const bool release=n.Kind=="schema-update";
            if(release&&n.Active&&!n.Resolved&&!n.Dismissed&&ImGui::BeginTable("Release downloads",columns,ImGuiTableFlags_SizingStretchSame)){
                const auto link=[&](const char* label,const wchar_t* url){
                    ImGui::TableNextColumn();
                    if(HelpyImGuiTheme::SoftButton(label,{-1,0}))ShellExecuteW(nullptr,L"open",url,nullptr,nullptr,SW_SHOWNORMAL);
                };
                link("Nexus Mods",RuneSchemaUpdateInfo::NexusFilesUrl);
                link("CurseForge",RuneSchemaUpdateInfo::CurseForgeFilesUrl);
                link("GitHub",RuneSchemaUpdateInfo::GitHubLatestUrl);
                ImGui::EndTable();
            }
            if(ImGui::BeginTable("Notification buttons",columns,ImGuiTableFlags_SizingStretchSame)){
                if(!release&&n.Active&&!n.Resolved&&!n.Action.empty()){ImGui::TableNextColumn();ImGui::BeginDisabled(Busy()||ModsPanel::Dirty);if(HelpyImGuiTheme::SoftButton(n.Action=="hotfix"?"Review hotfix":n.Action=="build-mod"?"Create / update cache":"Resync vanilla files",{-1,0}))Act(n);ImGui::EndDisabled();}
                ImGui::TableNextColumn();ImGui::BeginDisabled(n.Read);if(HelpyImGuiTheme::SoftButton("Mark read",{-1,0}))Inbox.Read(n.Id);ImGui::EndDisabled();
                if(!n.Dismissed){ImGui::TableNextColumn();if(HelpyImGuiTheme::SoftButton("Dismiss",{-1,0}))Inbox.Dismiss(n.Id);}ImGui::EndTable();
            }ImGui::PopID();
        }if(!shown)ImGui::TextDisabled(category==0?"No pending updates.":category==1?"No cache jobs yet.":"Your notification history is empty.");
    }HelpyMenu::EndBody();
}
}
