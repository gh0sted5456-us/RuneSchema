#pragma once
#include "Core/ModHealth.h"
#include "Generator/LiveLogPanel.h"
#include "Generator/HelpyIconPacks.h"
#include "Generator/RuneSchemaSettingsPanel.h"
#include "Generator/PersonalizationPanel.h"
#include "Generator/DragFeedback.h"
#include <future>
#include <map>

namespace PS::ModsPanel {
using Json=nlohmann::json;
inline Json Snapshot,Rows=Json::array();
inline std::future<Json> Worker;
inline std::string Status;
inline bool Dirty=false,Restart=false;
inline bool WorkerMutates=false;
inline char Search[128]{};
inline std::map<std::string,ModHealth::State> Health;
inline uint64_t HealthRevision=~uint64_t{};
inline double NextRefresh=0;
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
    if(Worker.valid()){Status="An operation is already in progress.";return;}
    const auto remote=RuneSchemaSettingsPanel::Remote;if(!remote){Status="Mod management requires a matching RuneSchema core.";return;}
    WorkerMutates=request.value("Action",std::string{})!="ModsRead";
    Worker=std::async(std::launch::async,[remote,request=std::move(request)]{try{Json reply;if(remote(request,reply)&&reply.is_object())return reply;return Json{{"Ok",false},{"Status","Mod service unavailable. Nothing was retried."}};}catch(const std::exception& e){return Json{{"Ok",false},{"Status",e.what()}};}});
}
inline void Import(const std::filesystem::path& path){if(Dirty){Status="Apply or discard your order changes before importing.";return;}if(Snapshot.is_null()||!Snapshot.contains("Root")){Status="Refresh Mods to confirm the connected runtime directory before importing.";return;}Start({{"Action","ModsImport"},{"Archive",path.string()},{"ExpectedRoot",Snapshot.at("Root")}});}
inline void Poll(){
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=Worker.get();Status=result.value("Status",std::string{});
        if(result.value("Ok",false)&&(!Dirty||WorkerMutates)){Restart|=!Status.empty();Snapshot=result;Rows=result.at("Rows");Dirty=false;HealthRevision=~uint64_t{};}
    }
}
inline void Shutdown(){if(Worker.valid())try{(void)Worker.get();}catch(...){};}
inline void Metadata(const Json& row){
    const auto& metadata=row.at("Metadata");
    ImGui::TextUnformatted(row.at("Name").get_ref<const std::string&>().c_str());
    for(const char* key:{"modId","name","version","build","author","description","nexusModId","nexusFileId"})if(metadata.contains(key)&&metadata[key].is_string()&&!metadata[key].get_ref<const std::string&>().empty())ImGui::TextWrapped("%s: %s",key,metadata[key].get_ref<const std::string&>().c_str());
    if(metadata.empty())ImGui::TextDisabled("No author metadata");
    if(row.contains("Issue"))ImGui::TextWrapped("%s",row.at("Issue").get_ref<const std::string&>().c_str());
    ImGui::TextWrapped("Loaders: %s",row.at("Loaders").dump().c_str());
    ImGui::TextDisabled("Cache: %s",row.value("HasCache",false)?"included":"not supplied");
    if(metadata.contains("history")&&metadata["history"].is_array()){
        ImGui::SeparatorText("Version history (author supplied)");size_t count=0;
        for(const auto& change:metadata["history"]){if(++count>12)break;ImGui::TextWrapped("%s",change.dump().c_str());}
    }
}
inline void HealthDetails(const Json& row,const ModHealth::State& state,bool checked){
    HelpyImGuiTheme::BeginDetailsTooltip(560);
    ImGui::TextUnformatted(row.at("Name").get_ref<const std::string&>().c_str());ImGui::Separator();
    ImGui::TextWrapped("%s · observed issues, not a compatibility check.",ModHealth::Label(state,checked));
    if(row.contains("Health")){const auto& lifetime=row["Health"].value("Lifetime",Json::object());ImGui::Text("This session: %zu errors, %zu warnings",state.Errors,state.Warnings);ImGui::Text("All sessions: %llu errors, %llu warnings",lifetime.value("Errors",uint64_t{}),lifetime.value("Warnings",uint64_t{}));}
    if(row.contains("RegistryDeclarations")){
        ImGui::Text("Declarations: %zu accepted, %zu rejected",state.AcceptedDeclarations,state.RejectedDeclarations);
        ImGui::TextDisabled("Enrollment is not a full native-reference check.");
        for(const auto& declaration:row["RegistryDeclarations"])ImGui::TextWrapped("%s: %s%s%s",declaration.value("Accepted",false)?"Accepted":"Rejected",declaration.value("Source",std::string{}).c_str(),declaration.value("Accepted",false)?"":" — ",declaration.value("Reason",std::string{}).c_str());
    }else ImGui::TextDisabled("Declaration audit unavailable; refresh with the updated core.");
    if(state.OptionalProbes)ImGui::TextWrapped("%zu optional conventional-path lookups found no asset. Other declaration paths remain valid; original messages are in Live Log.",state.OptionalProbes);
    for(const auto& detail:state.Details)ImGui::TextWrapped("%s",detail.c_str());
    if(state.Details.empty()&&row.contains("Health"))for(const auto& detail:row["Health"].value("Lifetime",Json::object()).value("LastIssues",std::vector<std::string>{}))ImGui::TextWrapped("Previous session: %s",detail.c_str());
    const auto storage=Snapshot.value("HealthLedger",Json::object()).value("StorageError",std::string{});if(!storage.empty())ImGui::TextWrapped("History not saved: %s",storage.c_str());
    if(!checked)ImGui::TextDisabled("Logs not checked yet");HelpyImGuiTheme::EndDetailsTooltip();
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
inline void Render(){
    Poll();const auto& theme=HelpyImGuiTheme::Current;
    if(Snapshot.is_null()&&!Worker.valid()&&Status.empty())Start({{"Action","ModsRead"}});
    if(!Snapshot.is_null()&&!Worker.valid()&&!Dirty&&ImGui::GetTime()>=NextRefresh){NextRefresh=ImGui::GetTime()+2;Start({{"Action","ModsRead"}});}
    if(HelpyImGuiTheme::SoftButton("Import ZIP..." )&&!Worker.valid()&&!Dirty){
        wchar_t file[32768]{};OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrFilter=L"Mod ZIP (*.zip)\0*.zip\0\0";dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST;
        if(GetOpenFileNameW(&dialog))Import(file);
    }ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Refresh")&&!Worker.valid()&&!Dirty)Start({{"Action","ModsRead"}});
    ImGui::SameLine();if(Restart)ImGui::TextColored(theme.accent,"Restart required");else ImGui::TextDisabled("Changes apply next launch");
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
    if(Worker.valid()&&Snapshot.is_null())ImGui::TextDisabled("Reading mods…");
    if(Snapshot.is_null())return;
    ImGui::TextDisabled("%s",Snapshot.value("Runtime",std::string("Connected runtime")).c_str());
    if(Snapshot.contains("ModsDirectory")){ImGui::SameLine();ImGui::TextDisabled("Mod directory");if(ImGui::IsItemHovered()){HelpyImGuiTheme::BeginDetailsTooltip();ImGui::TextWrapped("%s",Snapshot.at("ModsDirectory").get_ref<const std::string&>().c_str());HelpyImGuiTheme::EndDetailsTooltip();}}
    if(!Snapshot.value("OrderEnabled",false))ImGui::TextWrapped("Load order is disabled in RuneSchema Settings. Enable it before applying changes or importing.");
    ImGui::SetNextItemWidth(std::min(theme.X(420),ImGui::GetContentRegionAvail().x));ImGui::InputTextWithHint("##mods-search","Search mods...",Search,sizeof(Search));
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
    int moveFrom=-1,moveTo=-1;
#if defined(HELPY_PREVIEW)
    PreviewPlacards=0;PreviewNativeBadges=0;PreviewArtCards=0;PreviewBadgeOrders.clear();PreviewBadgeTints.clear();
#endif
    ImGui::BeginDisabled(Worker.valid()&&WorkerMutates);
    if(ImGui::BeginChild("mods-list",{0,std::max(theme.Y(100),ImGui::GetContentRegionAvail().y-theme.Y(70))},ImGuiChildFlags_Borders)){
        size_t activeOrder=0;
        for(size_t i=0;i<Rows.size();++i){auto& row=Rows[i];const auto owner=row.at("Name").get<std::string>();
            const size_t order=row.value("Enabled",false)&&row.value("Present",false)?++activeOrder:0;
            const auto nickname=Nickname(row);
            if(Search[0]&&LiveLog::Lower(owner+" "+nickname).find(LiveLog::Lower(Search))==std::string::npos)continue;
            ImGui::PushID(owner.c_str());const auto& health=Health[owner];
            if(ImGui::BeginChild("placard",{0,theme.Y(84)},ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)){
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
                ImGui::SetCursorScreenPos(titleAt);ImGui::InvisibleButton("##mod-title",{titleWidth,theme.Y(60)});
                const bool hovered=ImGui::IsItemHovered();
                if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip)){const auto index=static_cast<int>(i);ImGui::SetDragDropPayload("RUNESCHEMA_MOD_ORDER",&index,sizeof(index));DragFeedback::Held("Mod",nickname.c_str());ImGui::EndDragDropSource();}
                if(ImGui::BeginDragDropTarget()){if(const auto* payload=ImGui::AcceptDragDropPayload("RUNESCHEMA_MOD_ORDER")){const int from=*static_cast<const int*>(payload->Data);if(from>=0&&from<static_cast<int>(Rows.size())&&from!=static_cast<int>(i)){moveFrom=from;moveTo=static_cast<int>(i);}}ImGui::EndDragDropTarget();}
                if(ImGui::BeginPopupContextWindow("mod-actions",ImGuiPopupFlags_MouseButtonRight)){
                    const auto url=row.at("Metadata").value("website",std::string{});
                    if(ImGui::MenuItem("Open mod page",nullptr,false,!url.empty()&&(url.starts_with("https://")||url.starts_with("http://"))&&url.find_first_of("\r\n\t")==url.npos))ShellExecuteA(nullptr,"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
                    const auto folder=row.value("Folder",std::string{});
                    if(ImGui::MenuItem("Open folder",nullptr,false,row.value("Present",false)&&!folder.empty()))ShellExecuteW(nullptr,L"open",std::filesystem::u8path(folder).c_str(),nullptr,nullptr,SW_SHOWNORMAL);
                    ImGui::Separator();
                    if(ImGui::MenuItem("Set nickname..."))PersonalizationPanel::Edit(HelpyPersonalization::ModKey(owner),owner,false);
                    if(ImGui::MenuItem("Cache & banner...",nullptr,false,!source.empty())){HelpyIconPacks::SelectedSource=source;HelpyIconPacks::Open=true;}
                    if(ImGui::MenuItem("Export source cache…",nullptr,false,!source.empty()&&!HelpyIconPacks::Busy()&&!HelpyIconPacks::Building())){HelpyIconPacks::SelectedSource=source;HelpyIconPacks::Open=true;HelpyIconPacks::Choose(false);}
                    if(ImGui::MenuItem("Change banner...",nullptr,false,!source.empty()))SourceBanners::Choose(source);
                    ImGui::Separator();if(ImGui::BeginMenu("Mod details")){Metadata(row);ImGui::EndMenu();}ImGui::EndPopup();}
                if(hovered&&!ImGui::IsPopupOpen("mod-actions")){HelpyImGuiTheme::BeginDetailsTooltip(480);Metadata(row);HelpyImGuiTheme::EndDetailsTooltip();}
                auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(titleAt,{titleAt.x+titleWidth,b.y-theme.Y(6)},true);
                draw->AddText(ImGui::GetFont(),theme.Y(17),titleAt,ImGui::GetColorU32(theme.text),nickname.c_str());
                PersonalizationPanel::Italic(draw,ImGui::GetFont(),theme.Y(12),{titleAt.x,titleAt.y+theme.Y(25)},ImGui::GetColorU32(theme.textDisabled),"{"+owner+"}");
                const auto& metadata=row.at("Metadata");const auto version=metadata.value("version",std::string{});
                const std::string subtitle=!row.value("Present",false)?"Folder missing":!row.value("Enabled",false)?"Disabled for next launch":version.empty()?"Enabled for next launch":"v"+version+"  |  Enabled next launch";
                draw->AddText({titleAt.x,titleAt.y+theme.Y(46)},ImGui::GetColorU32(theme.textDisabled),subtitle.c_str());draw->PopClipRect();
                ImGui::SetCursorScreenPos({b.x-theme.X(116),a.y+theme.Y(12)});bool enabled=row.at("Enabled").get<bool>();if(ImGui::Checkbox("Enabled",&enabled)){row["Enabled"]=enabled;Dirty=true;}
                ImGui::SetCursorScreenPos({b.x-theme.X(116),a.y+theme.Y(46)});if(ImGui::SmallButton("Up")&&i){moveFrom=static_cast<int>(i);moveTo=moveFrom-1;}ImGui::SameLine();if(ImGui::SmallButton("Down")&&i+1<Rows.size()){moveFrom=static_cast<int>(i);moveTo=moveFrom+1;}
            }ImGui::EndChild();ImGui::PopID();
        }
    }ImGui::EndChild();
    if(moveFrom>=0){auto moved=Rows[moveFrom];Rows.erase(Rows.begin()+moveFrom);Rows.insert(Rows.begin()+moveTo,std::move(moved));Dirty=true;}
    ImGui::BeginDisabled(!Dirty);if(HelpyImGuiTheme::SoftButton("Apply for next launch")){Start({{"Action","ModsSave"},{"ExpectedRoot",Snapshot.value("Root",std::string{})},{"ExpectedExists",Snapshot.at("ExpectedExists")},{"ExpectedContents",Snapshot.at("ExpectedContents")},{"Rows",Rows}});}
    ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Discard changes")){Rows=Snapshot.at("Rows");Dirty=false;}ImGui::EndDisabled();ImGui::EndDisabled();
    ImGui::TextDisabled("Drop a ZIP · Right-click for options");
}
}
