#pragma once
#include "Generator/NativeJournalCache.h"
#include "Generator/JournalAuthorPanel.h"
#include "Generator/PlayerPortraitPicker.h"
#include "Core/MapFilters.h"
#include "Core/JournalSections.h"
namespace PS::NativeJournalPanel {
using Json=nlohmann::json;
inline char Search[129]{};inline std::string Selected;inline int Source=0,Page=0,Section=0;
inline void Render(){
    NativeJournalCache::Poll();const auto& t=HelpyImGuiTheme::Current;
    const float gap=t.X(8),buttonWidth=std::max(1.f,(ImGui::GetContentRegionAvail().x-gap*2)/3);
    for(int i=1;i<=3;++i){if(i>1)ImGui::SameLine(0,gap);ImGui::PushID(i);const bool active=Section==i;
        if(active)ImGui::PushStyleColor(ImGuiCol_Button,t.selection);
        if(HelpyImGuiTheme::SoftButton(JournalSections::Names[i],{buttonWidth,t.Y(38)})){Section=i;Selected.clear();Page=0;}
        if(active)ImGui::PopStyleColor();ImGui::PopID();}
    if(ImGui::SmallButton("All entries")){Section=0;Selected.clear();Page=0;}ImGui::SameLine();ImGui::TextDisabled("%s",JournalSections::Names[Section]);
    ImGui::TextWrapped("Game-native vanilla and modded journal/lore. This reference browser does not discover entries or mark books read.");
    ImGui::TextWrapped("%s",NativeJournalCache::Status.c_str());
    ImGui::BeginDisabled(NativeJournalCache::Pending!=0);
    if(ImGui::Button("Refresh from game")){NativeJournalCache::Refresh=true;NativeJournalCache::Scanning=false;}
    ImGui::EndDisabled();
    ImGui::SetNextItemWidth(-1);ImGui::Combo("##journal-source",&Source,"All sources\0Vanilla\0Modded\0Other / unverified\0");
    ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##journal-search","Search title, type, ID or source...",Search,sizeof(Search));
    const bool wide=ImGui::GetContentRegionAvail().x>=t.X(650);const bool table=wide&&ImGui::BeginTable("##native-journal",2,ImGuiTableFlags_SizingStretchProp);
    if(table){ImGui::TableSetupColumn("Entries",ImGuiTableColumnFlags_WidthStretch,.65f);ImGui::TableSetupColumn("Journal",ImGuiTableColumnFlags_WidthStretch,1.35f);ImGui::TableNextColumn();}
    ImGui::BeginChild("##native-journal-list",{0,wide?t.Y(430):t.Y(150)},true);
    for(const auto& row:NativeJournalCache::Entries){
        if(Section&&JournalSections::For(row)!=JournalSections::Names[Section])continue;
        const auto group=row.value("SourceKind",std::string{});
        if((Source==1&&group!="vanilla")||(Source==2&&group!="modded")||(Source==3&&(group=="vanilla"||group=="modded")))continue;
        if(Search[0]&&!MapFilters::Matches(MapFilters::Fold(row.value("Name",std::string{})+" "+row.value("Type",std::string{})+" "+row.value("PersistenceID",std::string{})+" "+row.value("InternalName",std::string{})+" "+row.value("SourceName",std::string{})),Search))continue;
        const auto path=row.value("Path",std::string{});ImGui::PushID(path.c_str());if(ImGui::Selectable(row.value("Name",path).c_str(),Selected==path)){Selected=path;Page=0;}ImGui::PopID();
    }
    if(NativeJournalCache::Entries.empty())ImGui::TextWrapped("No native journal entries cached yet. Load a world to read its loaded journal data.");ImGui::EndChild();
    if(table)ImGui::TableNextColumn();ImGui::BeginChild("##native-journal-entry",{0,t.Y(430)},true);
    const Json* chosen=nullptr;for(const auto& row:NativeJournalCache::Entries)if(row.value("Path",std::string{})==Selected){chosen=&row;break;}
    if(chosen){const auto& row=*chosen;
        ImGui::TextWrapped("%s",row.value("Name",Selected).c_str());ImGui::TextWrapped("Type: %s | Source: %s",row.value("Type",std::string{}).c_str(),row.value("SourceName",std::string("Unknown")).c_str());
        ImGui::TextWrapped("Persistence ID: %s",row.value("PersistenceID",std::string{}).c_str());
        ImGui::TextWrapped("%s",row.value("ObservedCurrent",false)?"Observed in the current runtime scan. This does not establish discovery/read state.":"Cached reference only; presence in the current build/mod set has not been confirmed.");
        if(row.value("ModifiedByRuneSchema",false))ImGui::TextWrapped("Modified by: %s",row.value("ModifiedBy",std::string{}).c_str());
        const auto icon=row.value("Icon",std::string{});const auto a=ImGui::GetCursorScreenPos();
        if(ItemIconCache::Draw(icon,a,{a.x+t.X(170),a.y+t.Y(170)},true))ImGui::Dummy({t.X(170),t.Y(170)});
        const auto pages=row.value("Pages",Json::array());if(!pages.empty()){Page=std::clamp(Page,0,int(pages.size())-1);ImGui::Text("Page %d / %d",Page+1,int(pages.size()));if(ImGui::Button("Previous")&&Page>0)--Page;ImGui::SameLine();if(ImGui::Button("Next")&&Page+1<int(pages.size()))++Page;ImGui::TextWrapped("%s",pages[Page].get<std::string>().c_str());}
        else ImGui::TextWrapped("%s",row.value("Description",std::string("No readable page text exposed by this native entry.")).c_str());
        if(ImGui::Button("Use as new draft"))JournalAuthorPanel::Template(row);
        if(ImGui::CollapsingHeader("Asset details"))ImGui::TextWrapped("%s",row.value("Path",std::string{}).c_str());
    }else ImGui::TextWrapped("Choose an entry to view native pages and artwork.");
    ImGui::EndChild();if(table)ImGui::EndTable();
}
}
