#pragma once
#include "Generator/HelpyTextInput.h"
#include "Core/ProcessExitFuture.h"
#include "Core/ModHealth.h"
#include "Generator/LiveLogPanel.h"
#include "Generator/HelpyIconPacks.h"
#include "Generator/RuneSchemaSettingsPanel.h"
#include "Generator/PersonalizationPanel.h"
#include "Generator/DragFeedback.h"
#include "Generator/CacheBuilderPanel.h"
#include "Generator/HelpyMenu.h"
#include "ModDownloadLink.h"
#include "Core/ServerModVersions.h"
#include "Core/UE4SSInventory.h"
#include "Core/UE4SSManagement.h"
#include "Runtime/HelpyRuntimeMonitor.h"
#include "Core/HelpyModMetadata.h"
#include "Core/ModDeployment.h"
#include "Generator/ModMetadataPanel.h"
#include "Generator/ModLicensePanel.h"
#include "Generator/ModSaveFilesPanel.h"
#include <future>
#include <map>

namespace PS::ModsPanel {
using Json=nlohmann::json;
inline Json Snapshot,Rows=Json::array();
inline PS::ProcessExitFuture<Json> Worker;
inline std::string Status;
inline bool Dirty=false,Restart=false;
inline bool WorkerMutates=false;
inline int Tab=0;
inline uint64_t SnapshotRevision=0,SnapshotGeneration=~uint64_t{},RequestGeneration=0;
inline Json UeSnapshot;
inline PS::ProcessExitFuture<Json> UeWorker;
inline std::string UeStatus;
inline uint64_t UeGeneration=~uint64_t{},UeRequestGeneration=0,UeRequestRevision=0,UeRevision=0;
#if defined(HELPY_PREVIEW)
inline size_t ServerRowsDrawn=0;
inline size_t UeRowsDrawn=0;
inline std::string PreviewOpenedModLink;
inline std::string ServerTableWindow;
inline ImVec4 ServerLinkRect{};
#endif
inline void OpenModPage(const std::string& url){
    if(!ModDownloadLink::Valid(url))return;
#if defined(HELPY_PREVIEW)
    PreviewOpenedModLink=url;
#else
    ShellExecuteA(nullptr,"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
#endif
}
inline size_t RemoveMissing(const std::string& name={}){
    const auto before=Rows.size();auto& entries=Rows.get_ref<Json::array_t&>();
    std::erase_if(entries,[&](const auto& row){return !row.value("Present",true)&&(name.empty()||row.value("Name",std::string{})==name);});
    const auto removed=before-Rows.size();if(removed){Dirty=true;Status="Removed "+std::to_string(removed)+" missing list entries from the draft. Apply for next launch to save.";}return removed;
}
inline char Search[128]{};
inline std::map<std::string,ModHealth::State> Health;
inline uint64_t HealthRevision=~uint64_t{};
inline std::string Nickname(const Json& row){const auto owner=row.at("Name").get<std::string>();auto name=HelpyPersonalization::Nickname(HelpyPersonalization::ModKey(owner));
    const auto& metadata=row.at("Metadata");if(name.empty())name=metadata.value("nickname",std::string{});if(name.empty())name=metadata.value("name",std::string{});return name.empty()?owner:name;}
#if defined(HELPY_PREVIEW)
inline bool PreviewChecked=false;
inline size_t PreviewPlacards=0;
inline size_t PreviewNativeBadges=0,PreviewArtCards=0;
inline std::vector<size_t> PreviewBadgeOrders;
inline std::vector<ImU32> PreviewBadgeTints;
#endif
inline void Start(Json request){
    if((HelpyModCacheBuilder::Current().Running||HelpyModCacheBuilder::CurrentBatch().Active)&&request.value("Action",std::string{})!="ModsRead"){Status="Wait for the cache builder to finish before changing mod files/order.";return;}
    if(Worker.valid()){Status="An operation is already in progress.";return;}
    const auto remote=RuneSchemaSettingsPanel::Remote;if(!remote){Status="Mod management requires a matching RuneSchema core.";return;}
    WorkerMutates=request.value("Action",std::string{})!="ModsRead";RequestGeneration=SpawnToolRequests::CurrentGeneration();
    const auto metadataData=HostServices::CacheDirectory().parent_path(),assignedRoot=HostServices::ModDirectory(),cache=HelpySourceCache::Directory(false);
    Worker=std::async(std::launch::async,[remote,metadataData,assignedRoot,cache,request=std::move(request)]{try{Json reply;if(remote(request,reply)&&reply.is_object()){
        if(reply.value("Ok",false)&&reply.contains("Root")&&reply.contains("Rows")){ModDeployment::Annotate(reply);if(std::filesystem::u8path(reply.at("Root").get<std::string>()).lexically_normal()==assignedRoot.lexically_normal())HelpyModMetadata::UpdateTracking(reply,metadataData);try{const auto tips=cache/"tips.json";if(std::filesystem::is_regular_file(tips))CacheTips::Annotate(reply["Rows"],CacheTips::Read(tips));}catch(const std::exception& error){reply["TipsIssue"]=error.what();}}return reply;
    }return Json{{"Ok",false},{"Status","Mod service unavailable. Nothing was retried."}};}catch(const std::exception& e){return Json{{"Ok",false},{"Status",e.what()}};}});
}
inline void Import(const std::filesystem::path& path){if(Dirty){Status="Apply or discard your order changes before importing.";return;}if(Snapshot.is_null()||!Snapshot.contains("Root")){Status="Refresh Mods to confirm the connected runtime directory before importing.";return;}Start({{"Action","ModsImport"},{"Archive",path.string()},{"ExpectedRoot",Snapshot.at("Root")}});}
inline void Poll(){
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=Worker.get();Status=result.value("Status",std::string{});
        if(RequestGeneration!=SpawnToolRequests::CurrentGeneration())return;
        if(result.value("Ok",false)&&(!Dirty||WorkerMutates)){Restart|=!Status.empty();Snapshot=result;Rows=result.at("Rows");Dirty=false;HealthRevision=~uint64_t{};SnapshotGeneration=RequestGeneration;++SnapshotRevision;UeGeneration=~uint64_t{};}
    }
}
inline Json ServerSnapshot;
inline ServerModVersions::Report ServerComparison;
inline uint64_t ServerRevision=~uint64_t{},ServerComparisonRevision=0,ComparedRevision=~uint64_t{},ComparedGeneration=~uint64_t{},RequestedComparison=~uint64_t{};
inline std::string ServerError;
inline std::chrono::steady_clock::time_point ServerRetry{};
inline bool UpdateServerComparison(){
    const auto now=std::chrono::steady_clock::now();if(now<ServerRetry)return false;
    try{
    Poll();const bool changed=SpawnToolRequests::ReadQuickIfChanged(ServerRevision,-2,ServerSnapshot);
    const auto generation=SpawnToolRequests::CurrentGeneration();
    const bool hostReady=ServerSnapshot.contains("ServerMods")&&ServerSnapshot["ServerMods"].is_array();
    const bool localReady=SnapshotGeneration==generation&&Snapshot.contains("Rows")&&Snapshot["Rows"].is_array();
    if(hostReady&&!localReady&&RequestedComparison!=generation&&!Worker.valid()&&!Dirty){Start({{"Action","ModsRead"}});RequestedComparison=generation;}
    if(changed||ComparedRevision!=SnapshotRevision||ComparedGeneration!=generation){
        const Json empty=Json::array();ServerComparison=hostReady?ServerModVersions::Build(ServerSnapshot.at("ServerMods"),localReady?Snapshot.at("Rows"):empty,localReady):ServerModVersions::Report{};
        ComparedRevision=SnapshotRevision;ComparedGeneration=generation;++ServerComparisonRevision;
    }
    ServerError.clear();return true;
    }catch(const std::exception& error){ServerError=error.what();}catch(...){ServerError="The server status could not be refreshed.";}
    ServerRetry=now+std::chrono::seconds(5);ServerRevision=~uint64_t{};ServerSnapshot=Json::object();ServerComparison={};++ServerComparisonRevision;return false;
}
inline void StartUeInventory(){
    if(UeWorker.valid())return;const auto root=HostServices::ModDirectory();UeRequestGeneration=SpawnToolRequests::CurrentGeneration();UeRequestRevision=SnapshotRevision;
    UeWorker=std::async(std::launch::async,[root]{try{auto result=UE4SSManagement::Snapshot(root);result["Ok"]=true;return result;}catch(const std::exception& error){return Json{{"Ok",false},{"Status",error.what()}};}});
}
inline void SetUeEnabled(const Json& row,bool enabled){
    if(UeWorker.valid())return;const auto root=HostServices::ModDirectory(),data=HostServices::CacheDirectory().parent_path();const auto expected=UeSnapshot;const bool allowed=HelpyImGuiTheme::Current.ue4ssManageExternal;
    UeRequestGeneration=SpawnToolRequests::CurrentGeneration();UeRequestRevision=SnapshotRevision;
    UeWorker=std::async(std::launch::async,[root,data,expected,row,enabled,allowed]{try{return UE4SSManagement::Save(root,expected,row,enabled,allowed,data);}catch(const std::exception& error){auto retained=expected;retained["Status"]=error.what();return retained;}});
}
inline void UeToggle(const Json& row){
    const bool managed=row.value("Managed",false);bool enabled=row.value(managed?"LoaderEnabled":"ConfiguredEnabled",false);
    const bool allowed=managed||HelpyImGuiTheme::Current.ue4ssManageExternal;
    ImGui::BeginDisabled(!allowed||UeWorker.valid()||row.value("NativeUnsupported",false)||!row.value("ConfiguredKnown",true)||!row.contains("ControlFile")||!UeSnapshot.contains("ModsTxt"));
    if(HelpyImGuiTheme::Toggle("Enabled",&enabled))SetUeEnabled(row,enabled);ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))HelpyImGuiTheme::Tooltip(managed?"Enables or disables all UE4SS loaders in this RuneSchema mod through ue4ss/status.txt. Its PAK/JSON content and parent enable state stay separate. Applies next launch.":allowed?"Updates UE4SS mods.txt for the next launch. An enabled.txt marker is backed up into AppData before disabling.":"Enable Manage external UE4SS mods to edit this runtime's mods.txt. Applies next launch.");
}
inline void UeCardBadge(bool enabled,ImVec2 a,float size){
    auto* draw=ImGui::GetWindowDrawList();const ImVec2 b{a.x+size,a.y+size};const auto tint=enabled?IM_COL32(170,170,170,255):IM_COL32(0,0,0,255);
    HelpyBackgrounds::RequestImage("power-neutralzone.png");if(const auto* art=HelpyBackgrounds::Find("power-neutralzone.png");art&&art->Texture)draw->AddImage(ImTextureRef(art->Texture),a,b,{0,0},{1,1},tint);
    else{const ImVec2 diamond[]{{a.x+size*.5f,a.y},{b.x,a.y+size*.5f},{a.x+size*.5f,b.y},{a.x,a.y+size*.5f}};draw->AddConvexPolyFilled(diamond,4,tint);draw->AddPolyline(diamond,4,IM_COL32(110,110,110,255),ImDrawFlags_Closed,1);}
}
inline void RenderUeInventory(){
#if defined(HELPY_PREVIEW)
    UeRowsDrawn=0;
#endif
    auto& theme=HelpyImGuiTheme::Current;const auto generation=SpawnToolRequests::CurrentGeneration();
    if(UeWorker.valid()&&UeWorker.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=UeWorker.get();if(UeRequestGeneration==generation&&UeRequestRevision==SnapshotRevision){UeGeneration=generation;UeStatus=result.value("Status",std::string{});UeSnapshot=std::move(result);++UeRevision;}}
    if(UeGeneration!=generation&&!UeWorker.valid()){UeSnapshot={};StartUeInventory();}
    static int group=0;constexpr std::array groups{"External","RuneSchema loaders"};HelpyMenu::Tabs("ue4ss-mod-groups",groups,group,false);
    ImGui::TextWrapped("Manage next-launch UE4SS configuration. Versions are optional author metadata and do not enter the host alignment report.");
    if(group)ImGui::TextWrapped("Each switch controls that RuneSchema mod's UE4SS loader only. Its other content stays enabled. Native/hybrid packages still require stock UE4SS installation.");
    ImGui::BeginDisabled(UeWorker.valid());if(!group&&HelpyImGuiTheme::Toggle("Manage external UE4SS mods",&theme.ue4ssManageExternal))HelpyImGuiTheme::SaveCurrent();ImGui::EndDisabled();
    if(HelpyImGuiTheme::Toggle("Compact view",&theme.modsCompact))HelpyImGuiTheme::SaveCurrent();
    ImGui::BeginDisabled(UeWorker.valid());if(HelpyImGuiTheme::SoftButton("Refresh UE4SS inventory"))StartUeInventory();ImGui::EndDisabled();
    if(UeWorker.valid())ImGui::TextDisabled("Reading this runtime's UE4SS mods...");
    if(!UeStatus.empty())ImGui::TextWrapped("%s",UeStatus.c_str());
    if(!UeSnapshot.is_object()||!UeSnapshot.value("Ok",false))return;
    static char search[128]{};static std::vector<size_t> filtered;static uint64_t revision=~uint64_t{};static int filteredGroup=-1;
    ImGui::SetNextItemWidth(std::min(theme.X(420),ImGui::GetContentRegionAvail().x));const bool searched=HelpyTextInput::InputTextWithHint("##ue4ss-search","Search UE4SS mods...",search,sizeof(search));
    const auto& rows=UeSnapshot.at("Rows");if(revision!=UeRevision||filteredGroup!=group||searched){filtered.clear();const auto query=LiveLog::Lower(search);
        for(size_t i=0;i<rows.size();++i){const auto& row=rows[i];if(row.at("Managed").get<bool>()!=(group==1))continue;
            if(query.empty()||LiveLog::Lower(row.at("Name").get<std::string>()+" "+row.at("Owner").get<std::string>()+" "+row.at("Version").get<std::string>()).find(query)!=std::string::npos)filtered.push_back(i);}
        revision=UeRevision;filteredGroup=group;}
    ImGui::TextWrapped("%zu entries. Configured status does not prove that a mod loaded. Unversioned entries are informational.",filtered.size());
    if(filtered.empty()){ImGui::TextDisabled("No matching UE4SS entries in this group.");return;}
    if(!theme.modsCompact){
        if(HelpyImGuiTheme::BeginPanel("ue4ss-mod-cards",{0,0},false)){
            const float rowHeight=theme.Y(88);ImGuiListClipper clipper;clipper.Begin(static_cast<int>(filtered.size()),rowHeight+ImGui::GetStyle().ItemSpacing.y);
            while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){const auto& row=rows.at(filtered[i]);const auto& metadata=row.at("Metadata");const auto name=row.at("Name").get<std::string>();auto title=metadata.value("name",name);if(title.empty())title=name;const bool enabled=row.value("ConfiguredEnabled",false)&&!row.value("NativeUnsupported",false);
                ImGui::PushID(static_cast<int>(filtered[i]));if(HelpyImGuiTheme::BeginPanel("ue4ss-mod",{0,rowHeight},true,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)){
#if defined(HELPY_PREVIEW)
                    ++UeRowsDrawn;
#endif
                    const auto a=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();const float badge=theme.Y(44);UeCardBadge(enabled,{a.x+theme.X(10),a.y+theme.Y(18)},badge);
                    ImGui::SetCursorScreenPos({a.x+theme.X(68),a.y+theme.Y(10)});const float titleWidth=std::max(1.f,size.x-theme.X(195));const auto titleAt=ImGui::GetCursorScreenPos();
                    ModLicensePanel::Title(row,title,{titleWidth,theme.Y(64)});
#if defined(HELPY_PREVIEW)
                    HelpyPreviewHooks::ModTitles[row.at("Folder").get<std::string>()]=HelpyPreviewHooks::LastRect();
#endif
                    ImGui::GetWindowDrawList()->AddText(titleAt,ImGui::GetColorU32(theme.text),ItemGridPicker::Ellipsize(title,titleWidth).c_str());
                    if(ImGui::IsItemHovered()){HelpyImGuiTheme::BeginDetailsTooltip();HelpyImGuiTheme::DetailsText("%s",name.c_str());if(group)HelpyImGuiTheme::DetailsText("RuneSchema owner: %s",row.at("Owner").get_ref<const std::string&>().c_str());for(const auto* key:{"author","description","source","issue"})if(metadata.contains(key)&&metadata[key].is_string())HelpyImGuiTheme::DetailsText("%s: %s",key,metadata[key].get_ref<const std::string&>().c_str());HelpyImGuiTheme::EndDetailsTooltip();}
                    ImGui::SetCursorScreenPos({a.x+theme.X(68),a.y+theme.Y(34)});ImGui::TextDisabled("%s · %s",row.at("Kind").get_ref<const std::string&>().c_str(),row.at("Version").get_ref<const std::string&>().empty()?"Unversioned":row.at("Version").get_ref<const std::string&>().c_str());
                    ImGui::SetCursorScreenPos({a.x+theme.X(68),a.y+theme.Y(57)});ImGui::TextDisabled("%s",row.value("NativeUnsupported",false)?"Native loader unsupported here":!row.value("ParentEnabled",true)?"Parent mod is disabled":enabled?"Enabled for next launch":"Disabled for next launch");
                    ImGui::SetCursorScreenPos({a.x+size.x-theme.X(116),a.y+theme.Y(12)});UeToggle(row);
                    ImGui::SetCursorScreenPos({a.x+size.x-theme.X(116),a.y+theme.Y(48)});if(HelpyImGuiTheme::SoftButton("Folder"))ShellExecuteW(nullptr,L"open",std::filesystem::u8path(row.at("Folder").get<std::string>()).c_str(),nullptr,nullptr,SW_SHOWNORMAL);
                    if(ImGui::IsWindowHovered()&&(ImGui::IsMouseClicked(0)||ImGui::IsMouseClicked(1)))ModLicensePanel::Select(row,title);
                    if(ModLicensePanel::Selected(row))ModLicensePanel::Outline(a,{a.x+size.x,a.y+size.y});
                }PS::HelpyImGuiTheme::EndPanel();ImGui::PopID();
            }
        }PS::HelpyImGuiTheme::EndPanel();return;
    }
    if(ImGui::BeginTable("ue4ss-inventory",5,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_ScrollY|ImGuiTableFlags_ScrollX|ImGuiTableFlags_Resizable,ImGui::GetContentRegionAvail(),std::max(ImGui::GetContentRegionAvail().x,theme.X(700)))){
        ImGui::TableSetupColumn("Mod",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("Version",ImGuiTableColumnFlags_WidthFixed,theme.X(76));ImGui::TableSetupColumn("Type",ImGuiTableColumnFlags_WidthFixed,theme.X(70));ImGui::TableSetupColumn("Configured",ImGuiTableColumnFlags_WidthFixed,theme.X(90));ImGui::TableSetupColumn("Observed health",ImGuiTableColumnFlags_WidthFixed,theme.X(140));ImGui::TableHeadersRow();
        ImGuiListClipper clipper;clipper.Begin(static_cast<int>(filtered.size()));while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){const auto& row=rows.at(filtered[i]);const auto& metadata=row.at("Metadata");const auto name=row.at("Name").get<std::string>(),version=row.at("Version").get<std::string>();
#if defined(HELPY_PREVIEW)
            ++UeRowsDrawn;
#endif
            ImGui::PushID(static_cast<int>(filtered[i]));ImGui::TableNextRow();ImGui::TableNextColumn();auto title=metadata.value("name",name);if(title.empty())title=name;if(ImGui::Selectable(title.c_str(),ModLicensePanel::Selected(row),ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowOverlap))ModLicensePanel::Select(row,title);
            if(ImGui::IsItemHovered()){HelpyImGuiTheme::BeginDetailsTooltip();HelpyImGuiTheme::DetailsText("%s",name.c_str());if(group)HelpyImGuiTheme::DetailsText("RuneSchema owner: %s",row.at("Owner").get_ref<const std::string&>().c_str());
                for(const auto* key:{"author","description","source","issue"})if(metadata.contains(key)&&metadata[key].is_string())HelpyImGuiTheme::DetailsText("%s: %s",key,metadata[key].get_ref<const std::string&>().c_str());HelpyImGuiTheme::DetailsText("%s",row.at("Folder").get_ref<const std::string&>().c_str());HelpyImGuiTheme::EndDetailsTooltip();}
            if(HelpyMenu::BeginContextItem("ue4ss-entry")){ModLicensePanel::Select(row,title);if(ImGui::MenuItem("Open folder"))ShellExecuteW(nullptr,L"open",std::filesystem::u8path(row.at("Folder").get<std::string>()).c_str(),nullptr,nullptr,SW_SHOWNORMAL);ImGui::TextDisabled(group?"Controlled through its owner's ue4ss/status.txt":"Optional next-launch UE4SS mods.txt management");ImGui::EndPopup();}
            ImGui::TableNextColumn();ImGui::TextUnformatted(version.empty()?"Unversioned":version.c_str());ImGui::TableNextColumn();ImGui::TextUnformatted(row.at("Kind").get_ref<const std::string&>().c_str());
            ImGui::TableNextColumn();UeToggle(row);
            ImGui::TableNextColumn();const auto health=theme.ue4ssDiagnostics?HelpyRuntimeMonitor::Lua.Status(name):std::string("Monitoring disabled");ImGui::TextUnformatted(health.c_str());if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Observed stock UE4SS log evidence only. Errors may precede this observation; current execution and every mod feature are not certified. Resource thresholds are in Settings > Diagnostics > Performance.");ImGui::PopID();
        }ImGui::EndTable();
    }
}
inline void Shutdown(){ModLicensePanel::Shutdown();if(Worker.valid())try{(void)Worker.get();}catch(...){};if(UeWorker.valid())try{(void)UeWorker.get();}catch(...){};}
inline void Metadata(const Json& row){
    const auto& metadata=row.at("Metadata");
    PS::HelpyImGuiTheme::DetailsTextUnformatted(row.at("Name").get_ref<const std::string&>().c_str());
    for(const char* key:{"modId","name","version","build","author","description","nexusModId","nexusFileId"})if(metadata.contains(key)&&metadata[key].is_string()&&!metadata[key].get_ref<const std::string&>().empty())PS::HelpyImGuiTheme::DetailsText("%s: %s",key,metadata[key].get_ref<const std::string&>().c_str());
    if(metadata.empty())PS::HelpyImGuiTheme::DetailsTextDisabled("No author metadata");
    if(row.contains("Issue"))PS::HelpyImGuiTheme::DetailsText("%s",row.at("Issue").get_ref<const std::string&>().c_str());
    if(row.contains("Origin"))PS::HelpyImGuiTheme::DetailsText("Installed through: %s",row.at("Origin").get_ref<const std::string&>().c_str());
    if(row.value("ModOrganizerVirtualView",false))PS::HelpyImGuiTheme::DetailsTextUnformatted("Mod Organizer 2 virtual filesystem is active for this view.");
    if(row.value("LocalIdentity",false))PS::HelpyImGuiTheme::DetailsTextUnformatted("Local ID.txt; file-change tracking enabled.");
    if(row.contains("MetadataTrackingIssue"))PS::HelpyImGuiTheme::DetailsText("Local tracking: %s",row["MetadataTrackingIssue"].get_ref<const std::string&>().c_str());
    PS::HelpyImGuiTheme::DetailsText("Loaders: %s",row.at("Loaders").dump().c_str());
    PS::HelpyImGuiTheme::DetailsTextDisabled("Cache: %s",row.value("HasCache",false)?"included":"not supplied");
    if(metadata.contains("history")&&metadata["history"].is_array()){
        ImGui::SeparatorText("Version history (author supplied)");size_t count=0;
        for(const auto& change:metadata["history"]){if(++count>12)break;PS::HelpyImGuiTheme::DetailsText("%s",change.dump().c_str());}
    }
}
inline void HealthDetails(const Json& row,const ModHealth::State& state,bool checked){
    HelpyImGuiTheme::BeginDetailsTooltip(560);
    PS::HelpyImGuiTheme::DetailsTextUnformatted(row.at("Name").get_ref<const std::string&>().c_str());ImGui::Separator();
    PS::HelpyImGuiTheme::DetailsText("%s · observed issues, not a compatibility check.",ModHealth::Label(state,checked));
    if(row.contains("Health")){const auto& lifetime=row["Health"].value("Lifetime",Json::object());PS::HelpyImGuiTheme::DetailsText("This session: %zu errors, %zu warnings",state.Errors,state.Warnings);PS::HelpyImGuiTheme::DetailsText("All sessions: %llu errors, %llu warnings",lifetime.value("Errors",uint64_t{}),lifetime.value("Warnings",uint64_t{}));}
    if(row.contains("RegistryDeclarations")){
        PS::HelpyImGuiTheme::DetailsText("Declarations: %zu accepted, %zu rejected",state.AcceptedDeclarations,state.RejectedDeclarations);
        PS::HelpyImGuiTheme::DetailsTextDisabled("Enrollment is not a full native-reference check.");
        for(const auto& declaration:row["RegistryDeclarations"])PS::HelpyImGuiTheme::DetailsText("%s: %s%s%s",declaration.value("Accepted",false)?"Accepted":"Rejected",declaration.value("Source",std::string{}).c_str(),declaration.value("Accepted",false)?"":" — ",declaration.value("Reason",std::string{}).c_str());
    }else PS::HelpyImGuiTheme::DetailsTextDisabled("Declaration audit unavailable; refresh with the updated core.");
    if(state.OptionalProbes)PS::HelpyImGuiTheme::DetailsText("%zu optional conventional-path lookups found no asset. Other declaration paths remain valid; original messages are in Live Log.",state.OptionalProbes);
    for(const auto& detail:state.Details)PS::HelpyImGuiTheme::DetailsText("%s",detail.c_str());
    if(state.Details.empty()&&row.contains("Health"))for(const auto& detail:row["Health"].value("Lifetime",Json::object()).value("LastIssues",std::vector<std::string>{}))PS::HelpyImGuiTheme::DetailsText("Previous session: %s",detail.c_str());
    const auto storage=Snapshot.value("HealthLedger",Json::object()).value("StorageError",std::string{});if(!storage.empty())PS::HelpyImGuiTheme::DetailsText("History not saved: %s",storage.c_str());
    if(!checked)PS::HelpyImGuiTheme::DetailsTextDisabled("Logs not checked yet");HelpyImGuiTheme::EndDetailsTooltip();
}
inline void OrderBadge(size_t order,const ModHealth::State& state,bool checked,ImVec2 a,float size){
    const ImVec4 color=!order?ImVec4(0,0,0,1):state.Errors||state.RejectedDeclarations?ImVec4(.95f,.28f,.25f,1):state.Warnings||!checked?ImVec4(.95f,.75f,.25f,1):ImVec4(.25f,.8f,.45f,1);
    auto* draw=ImGui::GetWindowDrawList();const ImVec2 b{a.x+size,a.y+size};
    const char* art=!order?"power-neutralzone.png":state.Errors||state.RejectedDeclarations?"power-severelybelowzone.png":state.Warnings||!checked?"power-atzone.png":"power-abovezone.png";
    const ImU32 tint=order?IM_COL32_WHITE:IM_COL32(0,0,0,255);
#if defined(HELPY_PREVIEW)
    PreviewBadgeOrders.push_back(order);PreviewBadgeTints.push_back(tint);
#endif
    HelpyBackgrounds::RequestImage(art);
    if(const auto* image=HelpyBackgrounds::Find(art);image&&image->Texture){draw->AddImage(ImTextureRef(image->Texture),a,b,{0,0},{1,1},tint);
#if defined(HELPY_PREVIEW)
        ++PreviewNativeBadges;
#endif
    }
    else {ItemIconCache::Queue(QuickDecorations::PowerLevelBadge,true);const ImVec2 diamond[]{{a.x+size*.5f,a.y},{b.x,a.y+size*.5f},{a.x+size*.5f,b.y},{a.x,a.y+size*.5f}};draw->AddConvexPolyFilled(diamond,4,ImGui::GetColorU32(color));draw->AddPolyline(diamond,4,IM_COL32(255,255,255,100),ImDrawFlags_Closed,1);}
    if(order){const auto label=std::to_string(order);const auto text=ImGui::CalcTextSize(label.c_str());const ImVec2 at{a.x+(size-text.x)*.5f,a.y+(size-text.y)*.5f};
        draw->AddText({at.x+1,at.y+1},IM_COL32(0,0,0,255),label.c_str());draw->AddText(at,IM_COL32(255,255,255,255),label.c_str());}
}
inline void ModActions(const Json& row,const std::string& owner,const std::string& nickname,std::string& removeMissing){
    const auto& theme=HelpyImGuiTheme::Current;
    const auto source=ContentSource::Mod(owner);
    ModLicensePanel::Select(row,nickname);
    if(ImGui::MenuItem("License"))Tab=3;
    if(!row.value("Present",true)&&ImGui::MenuItem("Remove missing entry"))removeMissing=owner;
    const auto url=row.at("Metadata").value("website",std::string{});
    if(ImGui::MenuItem("Open mod page",nullptr,false,ModDownloadLink::Valid(url)))OpenModPage(url);
    const auto folder=row.value("Folder",std::string{});
    if(ImGui::MenuItem("Open folder",nullptr,false,row.value("Present",false)&&!folder.empty()))ShellExecuteW(nullptr,L"open",std::filesystem::u8path(folder).c_str(),nullptr,nullptr,SW_SHOWNORMAL);
    ImGui::Separator();
    if(ImGui::MenuItem("Create/Update Cache",nullptr,false,row.value("Present",false)&&!Worker.valid()))CacheBuilderPanel::FocusInstalled(Snapshot,row);
    if(ImGui::MenuItem("Create/Edit ID.txt...",nullptr,false,row.value("Present",false)&&!Worker.valid()&&!Dirty&&!HelpyModCacheBuilder::Current().Running&&!HelpyModCacheBuilder::CurrentBatch().Active))ModMetadataPanel::Open(Snapshot,row);
    if(ImGui::MenuItem("Mod saves...",nullptr,false,row.value("Present",false)&&!ModSaveFilesPanel::Worker.valid()))ModSaveFilesPanel::Open(Snapshot,row);
    if(ImGui::MenuItem("Set nickname..."))PersonalizationPanel::Edit(HelpyPersonalization::ModKey(owner),owner,false);
    if(ImGui::MenuItem("Cache & banner...",nullptr,false,!source.empty())){HelpyIconPacks::SelectedSource=source;HelpyIconPacks::Open=true;}
    if(ImGui::MenuItem("Export source cache…",nullptr,false,!source.empty()&&!HelpyIconPacks::Busy()&&!HelpyIconPacks::Building())){HelpyIconPacks::SelectedSource=source;HelpyIconPacks::Open=true;HelpyIconPacks::Choose(false);}
    if(ImGui::MenuItem("Change banner...",nullptr,false,!source.empty()))SourceBanners::Choose(source);
    ImGui::Separator();
    const auto* viewport=ImGui::GetMainViewport();const float detailsWidth=std::min(theme.X(460),std::max(theme.X(180),viewport->WorkSize.x-theme.X(32)));
    ImGui::SetNextWindowSizeConstraints({detailsWidth,0},{detailsWidth,viewport->WorkSize.y*.8f});
    ImGui::SetNextWindowSize({detailsWidth,0});
    const bool detailsOpen=ImGui::BeginMenu("Mod details");
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::ModDetailsMenu=HelpyPreviewHooks::LastRect();
#endif
    if(detailsOpen){
#if defined(HELPY_PREVIEW)
        const auto position=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();HelpyPreviewHooks::ModDetailsWindow={position.x,position.y,position.x+size.x,position.y+size.y};
#endif
        Metadata(row);ImGui::EndMenu();}
}
inline void ModOrderDrag(size_t index,int& moveFrom,int& moveTo,const std::string& nickname){
    const auto to=static_cast<int>(index);
    if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip)){
        ImGui::SetDragDropPayload("RUNESCHEMA_MOD_ORDER",&to,sizeof(to));
        DragFeedback::Held("Mod",nickname.c_str());
        ImGui::EndDragDropSource();
    }
    if(ImGui::BeginDragDropTarget()){
        if(const auto* payload=ImGui::AcceptDragDropPayload("RUNESCHEMA_MOD_ORDER");payload&&payload->DataSize==sizeof(int)){
            const auto from=*static_cast<const int*>(payload->Data);
            if(from>=0&&from<static_cast<int>(Rows.size())&&from!=to){moveFrom=from;moveTo=to;}
        }
        ImGui::EndDragDropTarget();
    }
}
inline void ModToggle(Json& row){
    bool enabled=row.at("Enabled").get<bool>();
    if(HelpyImGuiTheme::Toggle("Enabled",&enabled)){row["Enabled"]=enabled;Dirty=true;}
}
inline void ModMoveButtons(size_t index,int& moveFrom,int& moveTo){
    if(ImGui::SmallButton("Up")&&index){moveFrom=static_cast<int>(index);moveTo=moveFrom-1;}
    ImGui::SameLine();
    if(ImGui::SmallButton("Down")&&index+1<Rows.size()){moveFrom=static_cast<int>(index);moveTo=moveFrom+1;}
}
inline void RenderBody(){
    Poll();auto& theme=HelpyImGuiTheme::Current;
    if(Tab==3){ModLicensePanel::Render(OpenModPage);return;}
    if(Tab==2){RenderUeInventory();return;}
    if(Tab==1){
#if defined(HELPY_PREVIEW)
        ServerRowsDrawn=0;
#endif
        if(!UpdateServerComparison()){ImGui::TextWrapped("Server status unavailable: %s",ServerError.c_str());return;}static char search[128]{};static std::vector<size_t> filtered;static int reportTab=0,filteredTab=-1;static uint64_t filteredRevision=~uint64_t{};
        ImGui::TextWrapped("Host versions compared with installed client metadata. Version labels do not establish registry compatibility; saved order changes apply next launch.");
        if(!ServerSnapshot.contains("ServerMods")||!ServerSnapshot["ServerMods"].is_array()){ImGui::TextDisabled("No server list received. Join a world with a supported RuneSchema host.");return;}
        const auto& rows=ServerSnapshot["ServerMods"];const auto& comparison=ServerComparison;const auto& counts=comparison.Counts;const Json empty=Json::array();const auto& installed=counts.Ready?Snapshot.at("Rows"):empty;
        ImGui::TextDisabled("%zu enabled mods",rows.size());
        HelpyMenu::NextControl("Refresh installed");ImGui::BeginDisabled(Worker.valid()||Dirty);if(HelpyImGuiTheme::SoftButton("Refresh installed"))Start({{"Action","ModsRead"}});ImGui::EndDisabled();
        HelpyMenu::NextControl("Copy comparison report");ImGui::BeginDisabled(!counts.Ready);if(HelpyImGuiTheme::SoftButton("Copy comparison report"))ImGui::SetClipboardText(ServerModVersions::Text(rows,installed,comparison).c_str());ImGui::EndDisabled();
        if(counts.Ready){ImGui::TextWrapped("%zu match · %zu different · %zu missing · %zu disabled · %zu ambiguous",counts.Matched,counts.Different,counts.Missing,counts.Disabled,counts.Ambiguous);ImGui::TextWrapped("%zu unversioned host/shared entries excluded · %zu client-only entries excluded",counts.Unversioned,counts.ClientOnly);}
        else ImGui::TextDisabled("Checking installed versions...");
        if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
        constexpr std::array reportTabs{"Host comparison","Client only"};HelpyMenu::Tabs("server-mod-report",reportTabs,reportTab,false);
        ImGui::SetNextItemWidth(std::min(theme.X(420),ImGui::GetContentRegionAvail().x));const bool searched=HelpyTextInput::InputTextWithHint("##server-mod-search","Search comparison...",search,sizeof(search));
        if(filteredRevision!=ServerComparisonRevision||filteredTab!=reportTab||searched){const auto query=LiveLog::Lower(search);filtered.clear();for(size_t i=0;i<(reportTab?comparison.ClientOnly.size():rows.size());++i){const auto index=reportTab?comparison.ClientOnly[i]:i;const auto& row=reportTab?installed.at(index):rows[index];
            if(query.empty()||LiveLog::Lower(row.value("Name",std::string{})+" "+row.value("Id",std::string{})+" "+row.value("Version",std::string{})+" "+(reportTab?row.at("Metadata").dump():comparison.Host.at(index).Version)).find(query)!=std::string::npos)filtered.push_back(index);}filteredRevision=ServerComparisonRevision;filteredTab=reportTab;}
        if(ImGui::BeginTable("server-mod-list",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_ScrollY|ImGuiTableFlags_ScrollX|ImGuiTableFlags_Resizable,ImGui::GetContentRegionAvail(),std::max(ImGui::GetContentRegionAvail().x,theme.X(650)))){
#if defined(HELPY_PREVIEW)
            ServerTableWindow=ImGui::GetCurrentWindow()->Name;
#endif
            ImGui::TableSetupColumn("Mod",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("Host",ImGuiTableColumnFlags_WidthFixed,theme.X(75));ImGui::TableSetupColumn("Client",ImGuiTableColumnFlags_WidthFixed,theme.X(75));ImGui::TableSetupColumn("Mod page",ImGuiTableColumnFlags_WidthFixed,theme.X(75));ImGui::TableHeadersRow();
            ImGuiListClipper clipper;clipper.Begin(static_cast<int>(filtered.size()));while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){
#if defined(HELPY_PREVIEW)
                ++ServerRowsDrawn;
#endif
                const auto& row=reportTab?installed.at(filtered[i]):rows[filtered[i]];const auto metadata=reportTab?row.at("Metadata"):Json::object();
                const auto name=reportTab?Nickname(row):row.value("Name",std::string{}),id=reportTab?row.value("Name",std::string{}):row.value("Id",std::string{}),version=reportTab?std::string{}:ServerModVersions::Version(row.value("Version",std::string{}));
                ImGui::TableNextRow();ImGui::TableNextColumn();
                const auto match=reportTab?ServerModVersions::Result{ServerModVersions::State::Unknown,ServerModVersions::Version(metadata.value("version",std::string{}))}:comparison.Host.at(filtered[i]);
                const bool excluded=reportTab?match.Version.empty():match.Excluded;const auto* label=excluded?"Unversioned":reportTab?"Client only":ServerModVersions::Label(match.Status);
                const ImVec4 color=excluded||reportTab?theme.textDisabled:match.Status==ServerModVersions::State::Match?ImVec4(.35f,.8f,.48f,1):match.Status!=ServerModVersions::State::Unknown?ImVec4(.95f,.75f,.25f,1):theme.textDisabled;
                const auto a=ImGui::GetCursorScreenPos(),size=ImGui::CalcTextSize(label);ImGui::Dummy({size.x+theme.X(8),size.y+theme.Y(2)});
                auto fill=color;fill.w=.16f;auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(a,ImGui::GetItemRectMax(),ImGui::GetColorU32(fill),theme.Y(3));draw->AddText({a.x+theme.X(4),a.y+theme.Y(1)},ImGui::GetColorU32(color),label);
                if(ImGui::IsItemHovered())ImGui::SetTooltip("Client: %s\nHost: %s\n%s\n%s",match.Version.empty()?"Not available":match.Version.c_str(),version.empty()?"Not supplied":version.c_str(),reportTab?"Client-only mod; excluded from alignment":excluded?"Unversioned - excluded from alignment":match.Status==ServerModVersions::State::Ambiguous?"Multiple installed copies share this identity":label,"Author version labels only; registry compatibility is checked separately.");
                ImGui::SameLine();ImGui::TextUnformatted(name.c_str());
                if(ImGui::IsItemHovered()){HelpyImGuiTheme::BeginDetailsTooltip();HelpyImGuiTheme::DetailsText("%s\nID: %s",name.c_str(),id.c_str());HelpyImGuiTheme::EndDetailsTooltip();}
                ImGui::TableNextColumn();ImGui::TextUnformatted(reportTab?"—":version.empty()?"Unversioned":version.c_str());
                ImGui::TableNextColumn();ImGui::TextUnformatted(reportTab?(match.Version.empty()?"Unversioned":match.Version.c_str()):match.Status==ServerModVersions::State::Missing?"Missing":match.Status==ServerModVersions::State::Ambiguous?"Copies":match.Version.empty()?counts.Ready?"Unversioned":"Checking...":match.Version.c_str());
                ImGui::TableNextColumn();const auto url=reportTab?metadata.value("website",std::string{}):row.value("Website",std::string{});
                if(ModDownloadLink::Valid(url)){
                    ImGui::PushID(i);if(ImGui::TextLink("Download"))OpenModPage(url);ImGui::PopID();
#if defined(HELPY_PREVIEW)
                    if(i==0){const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();ServerLinkRect={a.x,a.y,b.x,b.y};}
#endif
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Open the author's mod page in your browser:\n%s",url.c_str());
                }else ImGui::TextDisabled("—");
            }
            ImGui::EndTable();
        }
        return;
    }
    if(!Worker.valid()&&!Dirty&&((Snapshot.is_null()&&Status.empty())||(SnapshotGeneration!=SpawnToolRequests::CurrentGeneration()&&RequestGeneration!=SpawnToolRequests::CurrentGeneration())))Start({{"Action","ModsRead"}});
    if(HelpyImGuiTheme::SoftButton("Import ZIP..." )&&!Worker.valid()&&!Dirty){
        wchar_t file[32768]{};OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrFilter=L"Mod ZIP (*.zip)\0*.zip\0\0";dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST;
        if(GetOpenFileNameW(&dialog))Import(file);
    }HelpyMenu::NextControl("Refresh");
    if(HelpyImGuiTheme::SoftButton("Refresh")&&!Worker.valid()&&!Dirty)Start({{"Action","ModsRead"}});
    HelpyMenu::NextControl("Cache enabled mods");ImGui::BeginDisabled(Snapshot.is_null()||Dirty||Worker.valid()||HelpyModCacheBuilder::Current().Running||HelpyModCacheBuilder::CurrentBatch().Active);
    if(HelpyImGuiTheme::SoftButton("Cache enabled mods")&&HelpyModCacheBuilder::StartEnabled(std::filesystem::u8path(Snapshot.value("Root",std::string{})),Rows,theme.autoInstallAuthoringDotNet))CacheBuilderPanel::FocusInstalled(Snapshot,Json::object());ImGui::EndDisabled();
    HelpyMenu::NextControl("More");
    if(HelpyMenu::BeginMore("mods-actions")){
const bool missing=std::any_of(Rows.begin(),Rows.end(),[](const auto& row){return !row.value("Present",true);});
    ImGui::BeginDisabled(!missing||Worker.valid()||!Snapshot.is_object()||!Snapshot.value("OrderEnabled",false));
    if(HelpyImGuiTheme::SoftButton("Remove all missing entries"))RemoveMissing();ImGui::EndDisabled();
        ImGui::EndPopup();
    }
    HelpyMenu::NextControl("Changes apply next launch");if(Restart)ImGui::TextColored(theme.accent,"Restart required");else ImGui::TextDisabled("Changes apply next launch");
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    if(Worker.valid()&&Snapshot.is_null())ImGui::TextDisabled("Reading mods…");
    if(Snapshot.is_null())return;
    ImGui::TextDisabled("%s",Snapshot.value("Runtime",std::string("Connected runtime")).c_str());
    if(Snapshot.contains("ModsDirectory")){ImGui::SameLine();ImGui::TextDisabled("Mod directory");if(ImGui::IsItemHovered()){HelpyImGuiTheme::BeginDetailsTooltip();PS::HelpyImGuiTheme::DetailsText("%s",Snapshot.at("ModsDirectory").get_ref<const std::string&>().c_str());HelpyImGuiTheme::EndDetailsTooltip();}}
    if(!Snapshot.value("OrderEnabled",false))ImGui::TextWrapped("Load order is disabled in RuneSchema Settings. Enable it before applying changes or importing.");
    ImGui::SetNextItemWidth(std::min(theme.X(420),ImGui::GetContentRegionAvail().x));HelpyTextInput::InputTextWithHint("##mods-search","Search mods...",Search,sizeof(Search));
    HelpyMenu::NextControl("Compact view");if(HelpyImGuiTheme::Toggle("Compact view",&theme.modsCompact))HelpyImGuiTheme::SaveCurrent();
    const auto filter=LiveLog::Lower(Search);
    // Reuse the bounded asynchronous log readers, including when Live Log is
    // hidden. A missing game log never produces an all-clear claim.
    const bool ledgerReady=Snapshot.value("HealthLedger",Json::object()).value("Ready",false);
    if(!ledgerReady){LiveLogPanel::Poll(0);LiveLogPanel::Poll(2);}
    const auto revision=LiveLogPanel::Streams[0].History.Revision+LiveLogPanel::Streams[2].History.Revision;
    const bool checked=ledgerReady||(LiveLogPanel::Streams[0].Cursor.Started&&LiveLogPanel::Streams[2].Cursor.Started&&LiveLogPanel::Streams[0].ReadError.empty()&&LiveLogPanel::Streams[2].ReadError.empty())
#if defined(HELPY_PREVIEW)
        ||PreviewChecked
#endif
        ;
    if(revision!=HealthRevision){Health.clear();for(const auto& row:Rows){const auto owner=row.at("Name").get<std::string>();auto& state=Health[owner];state.AcceptedDeclarations=row.value("AcceptedDeclarations",size_t{});state.RejectedDeclarations=row.value("RejectedDeclarations",size_t{});
        if(ledgerReady&&row.contains("Health")){const auto& current=row["Health"];state.Errors=current.value("Errors",size_t{});state.Warnings=current.value("Warnings",size_t{});state.OptionalProbes=current.value("OptionalProbes",size_t{});state.Details=current.value("Details",std::vector<std::string>{});}
        else for(size_t stream:{size_t(0),size_t(2)})for(const auto& entry:LiveLogPanel::Streams[stream].History.Rows)ModHealth::Observe(state,owner,entry,stream?"Game":"RuneSchema");}HealthRevision=revision;}
    int moveFrom=-1,moveTo=-1;std::string removeMissing;
#if defined(HELPY_PREVIEW)
    PreviewPlacards=0;PreviewNativeBadges=0;PreviewArtCards=0;PreviewBadgeOrders.clear();PreviewBadgeTints.clear();
#endif
    ImGui::BeginDisabled(Worker.valid()&&WorkerMutates);
    const float actionsHeight=ImGui::GetFrameHeightWithSpacing()+ImGui::GetTextLineHeightWithSpacing()+theme.Y(8);
    if(HelpyImGuiTheme::BeginPanel("mods-list",{0,std::max(theme.Y(100),ImGui::GetContentRegionAvail().y-actionsHeight)},ImGuiChildFlags_Borders)){
        const bool table=theme.modsCompact&&ImGui::BeginTable("mods-compact",5,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_ScrollY|ImGuiTableFlags_ScrollX|ImGuiTableFlags_Resizable,
            ImGui::GetContentRegionAvail(),std::max(ImGui::GetContentRegionAvail().x,theme.X(560)));
        if(table){
            ImGui::TableSetupColumn("Order",ImGuiTableColumnFlags_WidthFixed,theme.X(52));
            ImGui::TableSetupColumn("Mod",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Version",ImGuiTableColumnFlags_WidthFixed,theme.X(100));
            ImGui::TableSetupColumn("Next launch",ImGuiTableColumnFlags_WidthFixed,theme.X(112));
            ImGui::TableSetupColumn("Move",ImGuiTableColumnFlags_WidthFixed,theme.X(90));
            ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        }
        size_t activeOrder=0;
        if(!theme.modsCompact||table)for(size_t i=0;i<Rows.size();++i){auto& row=Rows[i];const auto owner=row.at("Name").get<std::string>();
            const size_t order=row.value("Enabled",false)&&row.value("Present",false)?++activeOrder:0;
            const auto nickname=Nickname(row);
            if(!filter.empty()&&LiveLog::Lower(owner+" "+nickname).find(filter)==std::string::npos)continue;
            ImGui::PushID(owner.c_str());const auto& health=Health[owner];
            if(table){
                ImGui::TableNextRow();ImGui::TableNextColumn();
                const auto at=ImGui::GetCursorScreenPos();const float size=ImGui::GetFrameHeight();
                ImGui::InvisibleButton("##health-order",{size,size});OrderBadge(order,health,checked,at,size);
                if(ImGui::IsItemHovered())HealthDetails(row,health,checked);
                ImGui::TableNextColumn();ImGui::SetNextItemAllowOverlap();
                auto highlight=theme.accent;highlight.w=.24f;ImGui::PushStyleColor(ImGuiCol_Header,highlight);
                if(ImGui::Selectable((nickname+"##mod-title").c_str(),ModLicensePanel::Selected(row),ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowOverlap))
                    ModLicensePanel::Select(row,nickname);
                ImGui::PopStyleColor();
#if defined(HELPY_PREVIEW)
                HelpyPreviewHooks::ModTitles[owner]=HelpyPreviewHooks::LastRect();
#endif
                ModOrderDrag(i,moveFrom,moveTo,nickname);
                if(HelpyMenu::BeginContextItem("mod-actions")){ModActions(row,owner,nickname,removeMissing);ImGui::EndPopup();}
                else if(ImGui::IsItemHovered()&&!row.value("Present",false))ImGui::SetTooltip("Folder missing");
                ImGui::TableNextColumn();const auto version=row.at("Metadata").value("version",std::string{});
                ImGui::TextUnformatted(version.empty()?"—":version.c_str());
                ImGui::TableNextColumn();ModToggle(row);
                ImGui::TableNextColumn();ModMoveButtons(i,moveFrom,moveTo);
                ImGui::PopID();continue;
            }
            if(HelpyImGuiTheme::BeginPanel("placard",{0,theme.Y(84)},ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)){
#if defined(HELPY_PREVIEW)
                ++PreviewPlacards;
#endif
                const auto a=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();const ImVec2 b{a.x+size.x,a.y+size.y};
                const auto source=ContentSource::Mod(owner);
                if(row.contains("BannerRelative")&&Snapshot.contains("ModsDirectory"))SourceBanners::QueueModCard(owner,std::filesystem::u8path(Snapshot.at("ModsDirectory").get<std::string>())/owner/std::filesystem::u8path(row.at("BannerRelative").get<std::string>()));
                if(SourceBanners::Images.contains("mod-card:"+owner))SourceBanners::DrawKey("mod-card:"+owner,a,b,78);else SourceBanners::DrawKey(source,a,b,78);
#if defined(HELPY_PREVIEW)
                if(SourceBanners::Images.contains(source)||SourceBanners::Images.contains("mod-card:"+owner))++PreviewArtCards;
#endif
                const ImVec2 badgeAt{a.x+theme.X(10),a.y+theme.Y(17)};const float badgeSize=theme.Y(44);
                ImGui::SetCursorScreenPos(badgeAt);ImGui::InvisibleButton("##health-order",{badgeSize,badgeSize});OrderBadge(order,health,checked,badgeAt,badgeSize);
                if(ImGui::IsItemHovered())HealthDetails(row,health,checked);
                const ImVec2 titleAt{a.x+theme.X(68),a.y+theme.Y(12)};const float titleWidth=std::max(theme.X(40),size.x-theme.X(190));
                ImGui::SetCursorScreenPos(titleAt);ModLicensePanel::Title(row,nickname,{titleWidth,theme.Y(60)});
#if defined(HELPY_PREVIEW)
                HelpyPreviewHooks::ModTitles[owner]=HelpyPreviewHooks::LastRect();
#endif
                const bool hovered=ImGui::IsItemHovered()||ImGui::IsItemFocused();
                ModOrderDrag(i,moveFrom,moveTo,nickname);
                if(ImGui::IsWindowFocused()&&ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft,false))ImGui::OpenPopup("mod-actions");
                if(ImGui::BeginPopupContextWindow("mod-actions",ImGuiPopupFlags_MouseButtonRight)){
                    ModActions(row,owner,nickname,removeMissing);ImGui::EndPopup();
                }
                if(hovered&&!ImGui::IsPopupOpen("mod-actions")){HelpyImGuiTheme::BeginDetailsTooltip(titleAt,{titleAt.x+titleWidth,titleAt.y+theme.Y(60)},480);Metadata(row);HelpyImGuiTheme::EndDetailsTooltip();}
                auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(titleAt,{titleAt.x+titleWidth,b.y-theme.Y(6)},true);
                draw->AddText(ImGui::GetFont(),theme.Y(17),titleAt,ImGui::GetColorU32(theme.text),nickname.c_str());
                PersonalizationPanel::Italic(draw,ImGui::GetFont(),theme.Y(12),{titleAt.x,titleAt.y+theme.Y(25)},ImGui::GetColorU32(theme.textDisabled),"{"+owner+"}");
                const auto& metadata=row.at("Metadata");const auto version=metadata.value("version",std::string{});
                const std::string subtitle=!row.value("Present",false)?"Folder missing":!row.value("Enabled",false)?"Disabled for next launch":version.empty()?"Enabled for next launch":"v"+version+"  |  Enabled next launch";
                draw->AddText({titleAt.x,titleAt.y+theme.Y(46)},ImGui::GetColorU32(theme.textDisabled),subtitle.c_str());draw->PopClipRect();
                ImGui::SetCursorScreenPos({b.x-theme.X(116),a.y+theme.Y(12)});ModToggle(row);
                ImGui::SetCursorScreenPos({b.x-theme.X(116),a.y+theme.Y(46)});ModMoveButtons(i,moveFrom,moveTo);
                if(ImGui::IsWindowHovered()&&(ImGui::IsMouseClicked(0)||ImGui::IsMouseClicked(1)))ModLicensePanel::Select(row,nickname);
                if(ModLicensePanel::Selected(row))ModLicensePanel::Outline(a,b);
            }PS::HelpyImGuiTheme::EndPanel();ImGui::PopID();
        }
        if(table)ImGui::EndTable();
    }PS::HelpyImGuiTheme::EndPanel();
    if(!removeMissing.empty())RemoveMissing(removeMissing);
    else if(moveFrom>=0){auto moved=Rows[moveFrom];Rows.erase(Rows.begin()+moveFrom);Rows.insert(Rows.begin()+moveTo,std::move(moved));Dirty=true;}
    ImGui::BeginDisabled(!Dirty);if(HelpyImGuiTheme::SoftButton("Apply for next launch")){Start({{"Action","ModsSave"},{"ExpectedRoot",Snapshot.value("Root",std::string{})},{"ExpectedExists",Snapshot.at("ExpectedExists")},{"ExpectedContents",Snapshot.at("ExpectedContents")},{"Rows",Rows}});}
    ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Discard changes")){Rows=Snapshot.at("Rows");Dirty=false;}ImGui::EndDisabled();ImGui::EndDisabled();
    ImGui::TextDisabled("Drop a ZIP · Right-click for options");
}
inline void Render(){
    ModLicensePanel::Context();if(Tab==3&&ModLicensePanel::Selection.empty())Tab=0;
    constexpr std::array sections{"RuneSchema mods","Server comparison","UE4SS mods"};HelpyMenu::Categories("mod-manager-sections",sections,Tab,ModLicensePanel::Selection.empty()?nullptr:"License");
    if(HelpyMenu::BeginBody("mod-manager-body",Tab))RenderBody();HelpyMenu::EndBody();
}
}
