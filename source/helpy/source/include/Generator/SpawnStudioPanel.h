#pragma once
#include "Core/SpawnPresentation.h"
#include "Core/SpawnNames.h"
#include "Generator/LazyCatalog.h"
#include "Generator/HelpyNodes.h"
#include "Generator/MapLayerIcons.h"
#include "Generator/PlayerPortraitPicker.h"
namespace PS::SpawnStudioPanel {
struct State {std::string Selected,Query,Player;char Search[257]{},Name[129]{};float Scale=1,Radius=500,Yaw=0,Height=0;int Count=1,Mode=0,Power=1;bool Permanent=false,Confirmed=false,OverridePower=false;uint64_t Revision=0,Generation=0;std::vector<size_t> Matches;};
inline State Resources,Npcs,Enemies;
inline uint64_t JournalRevision=0;
inline std::map<std::string,std::string> JournalArt;
inline std::string Icon(const nlohmann::json& row){
    const auto direct=SpawnCatalogSearch::Text(row,"Icon");if(!direct.empty())return direct;
    if(JournalRevision!=NativeJournalCache::Revision){JournalRevision=NativeJournalCache::Revision;JournalArt.clear();for(const auto& entry:NativeJournalCache::Entries){
        const auto key=SpawnNames::Key(ItemPresentation::Text(entry,"Name")),icon=ItemPresentation::Text(entry,"Icon");if(key.empty()||icon.empty())continue;
        if(const auto it=JournalArt.find(key);it!=JournalArt.end()&&it->second!=icon)it->second.clear();else JournalArt.emplace(key,icon);
    }}
    const auto label=SpawnNames::Display(row);for(const auto& name:{label.Name,label.Art}){const auto it=JournalArt.find(SpawnNames::Key(name));if(it!=JournalArt.end()&&!it->second.empty())return it->second;}return {};
}
inline void Silhouette(ImDrawList*,ImVec2 foot,float height,int kind,ImU32 color){PortraitArt::Draw(kind,{foot.x-height*.5f,foot.y-height},{foot.x+height*.5f,foot.y},color);}
inline void ScaleReference(float scale,int kind,const std::string& name,const std::string& icon,const std::string& playerIcon){const auto& t=HelpyImGuiTheme::Current;const float width=std::max(t.X(160),ImGui::GetContentRegionAvail().x),height=t.Y(126);const auto a=ImGui::GetCursorScreenPos();ImGui::Dummy({width,height});auto* draw=ImGui::GetWindowDrawList();
    const float visual=std::clamp(scale,.25f,4.f),unit=t.Y(88)/std::max(1.f,visual);const float floor=a.y+t.Y(108);draw->AddLine({a.x+t.X(12),floor},{a.x+width-t.X(12),floor},ImGui::GetColorU32(t.border));
    if(!PlayerPortraitPicker::DrawCached(playerIcon,{a.x+width*.28f-unit*.5f,floor-unit},{a.x+width*.28f+unit*.5f,floor}))Silhouette(draw,{a.x+width*.28f,floor},unit,0,ImGui::GetColorU32(t.textDisabled));
    const ImVec2 lo{a.x+width*.72f-unit*visual*.5f,floor-unit*visual},hi{a.x+width*.72f+unit*visual*.5f,floor};
    if(!ItemIconCache::Draw(icon,lo,hi,true)&&!HelpyWikiIcons::Draw(name,lo,hi))Silhouette(draw,{a.x+width*.72f,floor},unit*visual,kind,ImGui::GetColorU32(t.accentHover));
    draw->AddText({a.x+width*.28f-t.X(22),floor+t.Y(3)},ImGui::GetColorU32(t.textDisabled),"Player 1x");draw->AddText({a.x+width*.72f-t.X(20),floor+t.Y(3)},ImGui::GetColorU32(t.text),"Selected");
    ImGui::TextDisabled("Relative portrait preview (visual range 0.25-4x); asset heights vary.");}
inline void Render(const nlohmann::json& snapshot,uint64_t revision,bool resource,int actorMode,const std::string& player){
    NativeJournalCache::Poll();
    auto& s=resource?Resources:actorMode==0?Npcs:Enemies;const bool npc=!resource&&actorMode==0;const int category=resource?2:1;const auto& t=HelpyImGuiTheme::Current;
    const auto generation=SpawnToolRequests::CurrentGeneration();if(s.Generation!=generation||s.Player!=player){s.Generation=generation;s.Player=player;s.Confirmed=false;}
    ImGui::TextDisabled("1  Choose a spawn     2  Set placement     3  Review and spawn");
    auto& lazy=LazyCatalog::For(category);if(!lazy.Started&&!lazy.Pending)LazyCatalog::Search(category,s.Query);
    const bool wide=ImGui::GetContentRegionAvail().x>=t.X(740);const bool table=wide&&ImGui::BeginTable("##spawn-studio",2,ImGuiTableFlags_SizingStretchProp);
    if(table){ImGui::TableSetupColumn("Browse",ImGuiTableColumnFlags_WidthStretch,1.15f);ImGui::TableSetupColumn("Placement",ImGuiTableColumnFlags_WidthStretch,1);ImGui::TableNextColumn();}
    ImGui::SeparatorText("Choose spawn");
    if(resource){if(ImGui::RadioButton("Trees",s.Mode==0)){s.Mode=0;s.Selected.clear();s.Confirmed=false;s.Revision=0;}ImGui::SameLine();if(ImGui::RadioButton("Ore / stone / other",s.Mode==1)){s.Mode=1;s.Selected.clear();s.Confirmed=false;s.Revision=0;}}
    ImGui::SetNextItemWidth(std::max(t.X(120),ImGui::GetContentRegionAvail().x-t.X(80)));const bool search=ImGui::InputTextWithHint("##spawn-search","Search name or source...",s.Search,sizeof(s.Search),ImGuiInputTextFlags_EnterReturnsTrue);ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Search")||search){s.Query=s.Search;s.Confirmed=false;s.Revision=0;LazyCatalog::Search(category,s.Query);}
    const auto at=snapshot.find("Definitions");const nlohmann::json empty=nlohmann::json::array();const auto& rows=at!=snapshot.end()&&at->is_array()?*at:empty;
    if(s.Revision!=revision){s.Revision=revision;s.Matches.clear();for(size_t i=0;i<rows.size();++i){const auto& row=rows[i];const auto type=SpawnCatalogSearch::Text(row,"Type");
        if(resource){if(type!="Resource")continue;const auto kind=HelpyNodes::ResourceKind(SpawnCatalogSearch::Text(row,"Class"),SpawnCatalogSearch::Text(row,"ResourceFamily"));if((s.Mode==0)!=(kind==HelpyNodes::Kind::Tree))continue;}
        else if(npc?type!="NPC":!SpawnPresentation::Enemy(row))continue;
        if(SpawnCatalogSearch::Matches(row,s.Query))s.Matches.push_back(i);}}
    ImGui::BeginChild("##spawn-browser",{0,t.Y(wide?350:220)},true);ImGuiListClipper clip;clip.Begin(int(s.Matches.size()));
    while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i){const auto& row=rows[s.Matches[i]];const auto key=SpawnCatalogSearch::Text(row,"Key");const auto label=SpawnNames::Display(row);ImGui::PushID(key.c_str());
        const auto a=ImGui::GetCursorScreenPos();const float side=t.Y(24);ImGui::Dummy({side,side});
        if(!ItemIconCache::Draw(Icon(row),a,{a.x+side,a.y+side},true)&&!HelpyWikiIcons::Draw(label.Art,a,{a.x+side,a.y+side}))MapLayerIcons::Draw(resource?(s.Mode==0?"trees":"ore"):npc?"npcs":"creatures",{a.x+side*.5f,a.y+side*.5f},side);
        ImGui::SameLine();if(ImGui::Selectable(label.Name.empty()?key.c_str():label.Name.c_str(),s.Selected==key,0,{0,side})){s.Selected=key;s.Confirmed=false;}
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",SpawnCatalogSearch::Text(row,"Class").c_str());ImGui::PopID();}
    if(s.Matches.empty())ImGui::TextWrapped("%s",lazy.Pending?"Reading runtime actors...":resource?"No matching resources in this slice.":npc?"No matching NPCs.":"No verified enemy character actors in this slice. Attack/effect candidates are excluded.");
    ImGui::EndChild();
    if(lazy.HasMore&&!lazy.Pending&&s.Matches.size()<24)LazyCatalog::More(category);
    if(lazy.HasMore&&!lazy.Pending&&HelpyImGuiTheme::SoftButton("More results"))LazyCatalog::More(category);
    ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Refresh")){LazyCatalog::Retry(category);s.Revision=0;s.Confirmed=false;}
    if(table)ImGui::TableNextColumn();
    ImGui::SeparatorText("Placement");std::string playerName="Choose a player above",playerIcon;if(snapshot.contains("Players"))for(const auto& row:snapshot["Players"])if(row.value("Path",std::string{})==player){playerName=row.value("Name",playerName);playerIcon=PlayerPortraitPicker::Icon(row);}ImGui::Text("Near: %s",playerName.c_str());
    const nlohmann::json* chosen=nullptr;for(const auto& row:rows)if(row.value("Key",std::string{})==s.Selected){chosen=&row;break;}
    ImGui::TextWrapped("Spawn: %s",chosen?SpawnNames::Display(*chosen).Name.c_str():"Select an actor on the left");
    if(ImGui::SliderFloat("Scale",&s.Scale,.1f,10.f,"%.2fx"))s.Confirmed=false;ScaleReference(s.Scale,resource?(s.Mode==0?1:2):npc?0:3,chosen?SpawnNames::Display(*chosen).Art:"",chosen?Icon(*chosen):"",playerIcon);
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
