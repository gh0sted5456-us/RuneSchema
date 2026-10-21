#pragma once
#include "Generator/HelpyImGuiTheme.h"
#include "Core/SpawnPresentation.h"
#include "Core/SpawnNames.h"
#include "Core/SpawnScale.h"
#include "Generator/HelpyBackgrounds.h"
#include "Generator/LazyCatalog.h"
#include "Generator/HelpyNodes.h"
#include "Generator/MapLayerIcons.h"
#include "Generator/PlayerPortraitPicker.h"
#include "Generator/SpawnBrowserSettings.h"
namespace PS::SpawnStudioPanel {
struct State {std::string Selected,Query,Player;char Search[257]{},Name[129]{};float Scale=1,Radius=500,Yaw=0,Height=0;int Count=1,Mode=0,Power=1;bool Permanent=false,Confirmed=false,OverridePower=false,ShowHidden=false,Rebuild=true;uint64_t Revision=0,Generation=0,PreferencesRevision=0,RowsRevision=0;size_t RowCount=0;std::vector<size_t> Matches;};
inline State Resources,Npcs,Enemies;
inline void RefreshMatches(State& s,const nlohmann::json& rows,uint64_t revision,uint64_t rowsRevision,bool resource,bool npc){
    if(!s.Rebuild&&s.Revision==revision&&s.RowsRevision==rowsRevision&&s.RowCount==rows.size()&&s.PreferencesRevision==SpawnBrowserSettings::Revision)return;
    s.Rebuild=false;s.Revision=revision;s.RowsRevision=rowsRevision;s.RowCount=rows.size();s.PreferencesRevision=SpawnBrowserSettings::Revision;s.Matches.clear();
    for(size_t i=0;i<rows.size();++i){const auto& row=rows[i];if(!row.is_object())continue;const auto type=SpawnCatalogSearch::Text(row,"Type");
        if(resource){if(SpawnPresentation::ResourceTab(row)!=s.Mode)continue;}else if(npc?type!="NPC":!SpawnPresentation::Enemy(row))continue;
        if(SpawnCatalogSearch::Matches(row,s.Query)&&SpawnBrowserSettings::Choices.Visible(row,s.ShowHidden))s.Matches.push_back(i);
    }
}
inline uint64_t JournalRevision=0;
inline std::map<std::string,std::string> JournalArt;
inline std::string Icon(const nlohmann::json& row){
    const auto direct=SpawnCatalogSearch::Text(row,"Icon");if(!direct.empty())return direct;
    if(JournalRevision!=NativeJournalCache::Revision){JournalRevision=NativeJournalCache::Revision;JournalArt.clear();const auto add=[&](const nlohmann::json& entries){for(const auto& entry:entries){
        const auto key=SpawnNames::Key(ItemPresentation::Text(entry,"Name")),icon=ItemPresentation::Text(entry,"Icon");if(key.empty()||icon.empty())continue;
        if(const auto it=JournalArt.find(key);it!=JournalArt.end()&&it->second!=icon)it->second.clear();else JournalArt.emplace(key,icon);
    }};add(NativeJournalCache::Entries);add(NativeJournalCache::BundleArt);}
    const auto label=SpawnNames::Display(row);for(const auto& name:{label.Name,label.Art}){const auto it=JournalArt.find(SpawnNames::Key(name));if(it!=JournalArt.end()&&!it->second.empty())return it->second;}
    // This native copper icon path is taken from the game's ItemData, not a
    // guessed wiki URL. Other materials prefer their cached journal artwork.
    if(label.Art=="Copper ore"&&SpawnCatalogSearch::Fold(SpawnCatalogSearch::Text(row,"Class")).starts_with("/game/"))return "/Game/Art/UI/Icons/Resources_ConceptArt/T_Icon_Resource_Ore_Copper.T_Icon_Resource_Ore_Copper";
    return {};
}
inline void Details(const nlohmann::json& row,bool compact=false){
    const auto& t=HelpyImGuiTheme::Current;const auto label=SpawnNames::Display(row);const auto a=ImGui::GetCursorScreenPos();const float side=t.Y(compact?40:48);
    ImGui::Dummy({side,side});if(!ItemIconCache::Draw(Icon(row),a,{a.x+side,a.y+side},true)&&!HelpyWikiIcons::Draw(label.Art,a,{a.x+side,a.y+side})){
        const auto type=SpawnCatalogSearch::Text(row,"Type");MapLayerIcons::Draw(type=="NPC"?"npcs":type=="AI"?"creatures":SpawnPresentation::ResourceTab(row)==0?"trees":"ore",{a.x+side*.5f,a.y+side*.5f},side);
    }
    ImGui::SameLine();ImGui::BeginGroup();ImGui::TextWrapped("%s",label.Name.c_str());
    const auto type=SpawnCatalogSearch::Text(row,"Type"),family=SpawnCatalogSearch::Text(row,"ResourceFamily");
    ImGui::TextDisabled("%s%s%s",type=="AI"?"Enemy / AI":type.c_str(),family.empty()?"":" · ",family.c_str());ImGui::EndGroup();
    auto source=SpawnCatalogSearch::Text(row,"SourceName");if(source.empty())source=SpawnCatalogSearch::Text(row,"ModId");
    if(!source.empty())ImGui::TextWrapped("Source: %s",source.c_str());
    if(SpawnBrowserSettings::Choices.Contains(row))ImGui::TextColored(t.accentHover,"Hidden from normal results");
    const auto description=SpawnCatalogSearch::Text(row,"Description");if(!description.empty())ImGui::TextWrapped("%s",description.c_str());
    ImGui::TextWrapped("%s",row.value("NeedsInspection",false)?"Mounted candidate · inspection required before spawning.":row.value("Available",false)?"Native definition available. Placement safety checks still apply.":"Not currently available for spawning.");
    const auto reason=SpawnCatalogSearch::Text(row,"Reason");if(!reason.empty())ImGui::TextWrapped("%s",reason.c_str());
    if(!compact&&ImGui::CollapsingHeader("Technical details")){
        for(const auto* key:{"Class","Key","SourceRoot","Origin","TemporaryReason","PermanentReason"}){const auto value=SpawnCatalogSearch::Text(row,key);if(!value.empty())ImGui::TextWrapped("%s: %s",key,value.c_str());}
    }
}
inline void SetHidden(State& s,const nlohmann::json& row,bool hidden){
    if(SpawnBrowserSettings::Set(row,hidden)){s.Rebuild=true;s.Confirmed=false;if(hidden&&!s.ShowHidden)s.Selected.clear();}
}
inline void Silhouette(ImDrawList*,ImVec2 foot,float height,int kind,ImU32 color){PortraitArt::Draw(kind,{foot.x-height*.5f,foot.y-height},{foot.x+height*.5f,foot.y},color);}
inline void ScaleReference(float scale,int kind,const std::string& name,const std::string& icon,const std::string& playerIcon,const nlohmann::json& row){auto& t=HelpyImGuiTheme::Current;
    if(ImGui::Checkbox("Journal character reference",&t.scaleUsePortrait))HelpyImGuiTheme::SaveCurrent();
    const std::array<std::pair<const char*,const char*>,5> choices{{{"NuLL","scale-null.png"},{"Lonestar","scale-lonestar.png"},{"iGhrazi","scale-ighrazi.png"},{"Hauler","scale-hauler.png"},{"Selected player portrait","@player"}}};
    if(t.scaleUsePortrait){const auto selected=t.scaleReferenceIcon.empty()?std::string("scale-null.png"):t.scaleReferenceIcon;const char* title="NuLL";for(const auto& choice:choices)if(selected==choice.second)title=choice.first;
        ImGui::SetNextItemWidth(-1);if(ImGui::BeginCombo("##scale-reference-art",title)){for(const auto& choice:choices)if(ImGui::Selectable(choice.first,selected==choice.second)){t.scaleReferenceIcon=choice.second;HelpyImGuiTheme::SaveCurrent();}ImGui::EndCombo();}}
    const float width=std::max(t.X(160),ImGui::GetContentRegionAvail().x),height=t.Y(156);const auto a=ImGui::GetCursorScreenPos();ImGui::Dummy({width,height});auto* draw=ImGui::GetWindowDrawList();
    const float visual=float(std::clamp(SpawnScale::Ratio(row,scale),.05,100.)),unit=t.Y(118)/std::max(1.f,visual);const float floor=a.y+t.Y(136);draw->AddLine({a.x+t.X(12),floor},{a.x+width-t.X(12),floor},ImGui::GetColorU32(t.border));
    bool reference=false;const auto art=t.scaleReferenceIcon.empty()?std::string("scale-null.png"):t.scaleReferenceIcon;
    if(t.scaleUsePortrait&&art!="@player"){
        HelpyBackgrounds::RequestImage(art);if(const auto* image=HelpyBackgrounds::Find(art);image&&image->Texture){const float aspect=image->Width*(image->UV1.x-image->UV0.x)/(image->Height*(image->UV1.y-image->UV0.y)),w=std::min(width*.42f,unit*aspect);draw->AddImage(ImTextureRef(image->Texture),{a.x+width*.28f-w*.5f,floor-unit},{a.x+width*.28f+w*.5f,floor},image->UV0,image->UV1);reference=true;}
    }else if(t.scaleUsePortrait)reference=PlayerPortraitPicker::DrawCached(playerIcon,{a.x+width*.28f-unit*.5f,floor-unit},{a.x+width*.28f+unit*.5f,floor});
    if(!reference)Silhouette(draw,{a.x+width*.28f,floor},unit,0,ImGui::GetColorU32(t.textDisabled));
    const ImVec2 lo{a.x+width*.72f-unit*visual*.5f,floor-unit*visual},hi{a.x+width*.72f+unit*visual*.5f,floor};
    if(!ItemIconCache::Draw(icon,lo,hi,true)&&!HelpyWikiIcons::Draw(name,lo,hi))Silhouette(draw,{a.x+width*.72f,floor},unit*visual,kind,ImGui::GetColorU32(t.accentHover));
    draw->AddText({a.x+width*.28f-t.X(35),floor+t.Y(3)},ImGui::GetColorU32(t.textDisabled),"1.80 m reference");draw->AddText({a.x+width*.72f-t.X(20),floor+t.Y(3)},ImGui::GetColorU32(t.text),"Selected");
    if(const auto measured=SpawnScale::Height(row)){ImGui::Text("Scaled height: %.2f m",*measured*scale/100.);ImGui::TextDisabled("%s",row.value("PreviewHeightBasis",std::string("Native measurement")).c_str());}
    else ImGui::TextDisabled("Height unavailable · illustration only, not measured size");}
inline void Render(const nlohmann::json& snapshot,uint64_t revision,bool resource,int actorMode,const std::string& player,std::function<void(const nlohmann::json&)> inspect={}){
    NativeJournalCache::Poll();SpawnBrowserSettings::Pump();
    auto& s=resource?Resources:actorMode==0?Npcs:Enemies;const bool npc=!resource&&actorMode==0;const int category=resource?2:1;const auto& t=HelpyImGuiTheme::Current;
    const auto generation=SpawnToolRequests::CurrentGeneration();if(s.Generation!=generation||s.Player!=player){s.Generation=generation;s.Player=player;s.Confirmed=false;s.Rebuild=true;}
    ImGui::TextDisabled("1  Choose a spawn     2  Set placement     3  Review and spawn");
    auto& lazy=LazyCatalog::For(category);if(!lazy.Started&&!lazy.Pending)LazyCatalog::Search(category,s.Query);
    const bool wide=ImGui::GetContentRegionAvail().x>=t.X(740);const bool table=wide&&ImGui::BeginTable("##spawn-studio",2,ImGuiTableFlags_SizingStretchProp);
    if(table){ImGui::TableSetupColumn("Browse",ImGuiTableColumnFlags_WidthStretch,1.15f);ImGui::TableSetupColumn("Placement",ImGuiTableColumnFlags_WidthStretch,1);ImGui::TableNextColumn();}
    ImGui::SeparatorText("Choose spawn");
    if(resource){int mode=0;for(const auto* label:{"Trees","Ore / stone","Other resources"}){if(mode&&ImGui::GetContentRegionAvail().x>t.X(125))ImGui::SameLine();if(ImGui::RadioButton(label,s.Mode==mode)){s.Mode=mode;s.Selected.clear();s.Confirmed=false;s.Rebuild=true;}++mode;}}
    ImGui::SetNextItemWidth(std::max(t.X(120),ImGui::GetContentRegionAvail().x-t.X(80)));const bool search=ImGui::InputTextWithHint("##spawn-search","Search name or source...",s.Search,sizeof(s.Search),ImGuiInputTextFlags_EnterReturnsTrue);ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Search")||search){s.Query=s.Search;s.Confirmed=false;s.Rebuild=true;LazyCatalog::Search(category,s.Query);}
    if(ImGui::Checkbox("Show hidden",&s.ShowHidden)){s.Rebuild=true;s.Confirmed=false;}
    if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("Include hidden results. Right-click an entry or select it to restore it.\nOnly your local Helpy browser preferences change; no cache entry is deleted.");
    if(!SpawnBrowserSettings::Error.empty()){ImGui::TextWrapped("%s",SpawnBrowserSettings::Error.c_str());if(HelpyImGuiTheme::SoftButton("Retry preferences"))SpawnBrowserSettings::Retry();}
    const auto at=snapshot.find("Definitions");const nlohmann::json empty=nlohmann::json::array();const auto& rows=at!=snapshot.end()&&at->is_array()?*at:empty;
    RefreshMatches(s,rows,revision,snapshot.value("_CatalogRowsRevision",uint64_t{}),resource,npc);
    ImGui::BeginChild("##spawn-browser",{0,t.Y(wide?350:220)},true);ImGuiListClipper clip;clip.Begin(int(s.Matches.size()));
    while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i){if(i<0||size_t(i)>=s.Matches.size()||s.Matches[i]>=rows.size()){s.Rebuild=true;continue;}const auto& row=rows[s.Matches[i]];if(!row.is_object()){s.Rebuild=true;continue;}const auto key=SpawnCatalogSearch::Text(row,"Key");const auto label=SpawnNames::Display(row);ImGui::PushID(key.c_str());
        const auto a=ImGui::GetCursorScreenPos();const float side=t.Y(24);ImGui::Dummy({side,side});
        if(!ItemIconCache::Draw(Icon(row),a,{a.x+side,a.y+side},true)&&!HelpyWikiIcons::Draw(label.Art,a,{a.x+side,a.y+side}))MapLayerIcons::Draw(resource?(s.Mode==0?"trees":"ore"):npc?"npcs":"creatures",{a.x+side*.5f,a.y+side*.5f},side);
        const auto title=(label.Name.empty()?key:label.Name)+(SpawnBrowserSettings::Choices.Contains(row)?" · Hidden":"");
        ImGui::SameLine();if(ImGui::Selectable(title.c_str(),s.Selected==key,0,{0,side})){s.Selected=key;s.Confirmed=false;}
#ifdef HELPY_PREVIEW
        HelpyPreviewHooks::SpawnRows[key]=HelpyPreviewHooks::LastRect();
#endif
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)){ImGui::SetNextWindowSizeConstraints({t.X(240),0},{t.X(360),FLT_MAX});PS::HelpyImGuiTheme::BeginDetailsTooltip();Details(row,true);PS::HelpyImGuiTheme::EndDetailsTooltip();}
        if(ImGui::BeginPopupContextItem("##spawn-entry-actions")){
            if(ImGui::MenuItem("View details")){s.Selected=key;s.Confirmed=false;}
            const bool hidden=SpawnBrowserSettings::Choices.Contains(row);
            if(ImGui::MenuItem(hidden?"Restore result":"Hide result",nullptr,false,SpawnBrowserSettings::Loaded&&!SpawnBrowserPreferences::Identity(row).empty()))SetHidden(s,row,!hidden);
            ImGui::EndPopup();
        }
        ImGui::PopID();}
    if(s.Matches.empty())ImGui::TextWrapped("%s",lazy.Pending||lazy.HasMore?"Searching mounted and loaded definitions…":resource?"No matching resource candidates.":npc?"No matching NPCs.":"No matching enemy candidates. Attack/effect actors are excluded.");
    ImGui::EndChild();
    if(lazy.HasMore&&!lazy.Pending&&s.Matches.size()<24)LazyCatalog::More(category);
    if(lazy.HasMore&&!lazy.Pending&&HelpyImGuiTheme::SoftButton("More results"))LazyCatalog::More(category);
    ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Refresh")){LazyCatalog::Retry(category);s.Rebuild=true;s.Confirmed=false;}
    if(table)ImGui::TableNextColumn();
    const nlohmann::json* chosen=nullptr;for(const auto& row:rows)if(row.value("Key",std::string{})==s.Selected){chosen=&row;break;}
    ImGui::SeparatorText("Details");
    if(chosen){Details(*chosen);const bool hidden=SpawnBrowserSettings::Choices.Contains(*chosen);ImGui::BeginDisabled(!SpawnBrowserSettings::Loaded||SpawnBrowserPreferences::Identity(*chosen).empty());
        if(HelpyImGuiTheme::SoftButton(hidden?"Restore result":"Hide result"))SetHidden(s,*chosen,!hidden);
#ifdef HELPY_PREVIEW
        HelpyPreviewHooks::SpawnHide=HelpyPreviewHooks::LastRect();HelpyPreviewHooks::SpawnDetails=chosen->value("Key",std::string{});
#endif
        ImGui::EndDisabled();
    }else ImGui::TextDisabled("Select a result to see its details.");
    ImGui::SeparatorText("Placement");std::string playerName="Choose a player above",playerIcon;if(snapshot.contains("Players"))for(const auto& row:snapshot["Players"])if(row.value("Path",std::string{})==player){playerName=row.value("Name",playerName);playerIcon=PlayerPortraitPicker::Icon(row);}ImGui::Text("Near: %s",playerName.c_str());
    ImGui::TextWrapped("Spawn: %s",chosen?SpawnNames::Display(*chosen).Name.c_str():"Select an actor on the left");
    if(chosen&&chosen->value("NeedsInspection",false)){
        ImGui::TextWrapped("Mounted candidate · verify its native class before spawning. No actor is created by inspection.");
        ImGui::BeginDisabled(!inspect||SpawnToolRequests::IsWaiting());
        if(HelpyImGuiTheme::SoftButton("Inspect selected definition")){inspect(*chosen);s.Revision=0;s.Confirmed=false;}
        ImGui::EndDisabled();
    }
    if(ImGui::SliderFloat("Scale",&s.Scale,.1f,10.f,"%.2fx"))s.Confirmed=false;ScaleReference(s.Scale,resource?(s.Mode==0?1:2):npc?0:3,chosen?SpawnNames::Display(*chosen).Art:"",chosen?Icon(*chosen):"",playerIcon,chosen?*chosen:nlohmann::json::object());
    if(!npc&&ImGui::SliderInt("Count",&s.Count,1,20))s.Confirmed=false;
    float metres=s.Radius/100;if(ImGui::SliderFloat("Radius",&metres,0,50,"%.1f m")){s.Radius=metres*100;s.Confirmed=false;}
    if(ImGui::Checkbox("Permanent placement",&s.Permanent))s.Confirmed=false;ImGui::TextWrapped("%s",s.Permanent?"Installed RuneSchema placement, restored on load.":"Temporary actors; native save-exclusion checks still apply.");
    if(ImGui::CollapsingHeader("Advanced placement")){
        if(ImGui::InputText("Name override",s.Name,sizeof(s.Name)))s.Confirmed=false;
        if(ImGui::InputFloat("Yaw (degrees)",&s.Yaw))s.Confirmed=false;if(ImGui::InputFloat("Height offset (cm)",&s.Height))s.Confirmed=false;
        if(!resource&&!npc){if(ImGui::Checkbox("Override power level",&s.OverridePower))s.Confirmed=false;if(s.OverridePower&&ImGui::SliderInt("Power level",&s.Power,1,100))s.Confirmed=false;}
    }
    const bool allowed=chosen&&chosen->value("Available",true)&&(!npc||(s.Permanent?chosen->value("PermanentAllowed",false):chosen->value("TemporaryAllowed",false)));
    if(chosen&&!allowed)ImGui::TextWrapped("%s",chosen->value(s.Permanent?"PermanentReason":"TemporaryReason",chosen->value("Reason",std::string("Not currently supported"))).c_str());
    ImGui::Separator();ImGui::Checkbox("Confirm this placement",&s.Confirmed);ImGui::BeginDisabled(!s.Confirmed||!allowed||player.empty()||SpawnToolRequests::IsWaiting());
    if(HelpyImGuiTheme::SoftButton(s.Permanent?"Spawn & install":"Spawn now",{-1,0})){
        nlohmann::json request={{"Action","Spawn"},{"_Source","HelpyStandalone"},{"Player",player},{"Definition",s.Selected},{"Class",chosen->value("Class",std::string{})},{"Name",s.Name},{"Scale",s.Scale},{"Permanent",s.Permanent},{"NPC",npc},{"Resource",resource},{"Count",npc?1:s.Count},{"Distance",5.0},{"PlacementMode","Radius"},{"PlacementRadius",s.Radius},{"Yaw",s.Yaw},{"Height",s.Height}};
        if(npc)request["DurationSeconds"]=s.Permanent?0:HelpyNodes::DefaultDuration;if(!resource&&!npc&&s.OverridePower)request["PowerLevel"]=s.Power;SpawnToolRequests::Submit(std::move(request));s.Confirmed=false;
    }ImGui::EndDisabled();if(table)ImGui::EndTable();
    ImGui::Separator();if(HelpyImGuiTheme::SoftButton("Export placements"))SpawnToolRequests::Submit({{"Action","Export"}});ImGui::SameLine();ImGui::TextDisabled("Export does not install or change actors.");
}
}
