#pragma once
#include "Runtime/HelpyWikiIcons.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Core/MapFilters.h"
#include "Generator/NativeJournalPanel.h"
namespace PS::JournalPanel {
inline char Search[129]{};inline std::string Selected;
inline void Wiki(){
    const auto& t=HelpyImGuiTheme::Current;ImGui::TextWrapped("Optional Wiki articles, not native journal entries or your character's discovered/unlocked state.");
    if(!HelpyWikiIcons::Status().empty())ImGui::TextDisabled("%s",HelpyWikiIcons::Status().c_str());
    const bool wide=ImGui::GetContentRegionAvail().x>=t.X(650);const bool table=wide&&ImGui::BeginTable("##journal-layout",2,ImGuiTableFlags_SizingStretchProp);
    if(table){ImGui::TableSetupColumn("Entries",ImGuiTableColumnFlags_WidthStretch,.65f);ImGui::TableSetupColumn("Journal",ImGuiTableColumnFlags_WidthStretch,1.35f);ImGui::TableNextColumn();}
    ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##journal-search","Search cached entries...",Search,sizeof(Search));
    auto names=HelpyWikiIcons::Names();std::erase_if(names,[&](const auto& name){return !MapFilters::Matches(MapFilters::Fold(name),Search);});
    ImGui::BeginChild("##journal-list",{0,wide?t.Y(450):t.Y(160)},true);ImGuiListClipper clip;clip.Begin(int(names.size()));
    while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i)if(ImGui::Selectable(names[i].c_str(),Selected==names[i]))Selected=names[i];
    if(names.empty())ImGui::TextWrapped("No matching cached entries. Use Download Initial Assets or Map filters > Download wiki pictures & journals.");ImGui::EndChild();
    if(table)ImGui::TableNextColumn();ImGui::BeginChild("##journal-entry",{0,t.Y(450)},true);
    if(Selected.empty())ImGui::TextWrapped("Choose an entry to view its artwork and cached text.");else{
        ImGui::TextUnformatted(Selected.c_str());const auto a=ImGui::GetCursorScreenPos();if(HelpyWikiIcons::Draw(Selected,a,{a.x+t.X(150),a.y+t.Y(150)}))ImGui::Dummy({t.X(150),t.Y(150)});
        const auto text=HelpyWikiIcons::Journal(Selected);ImGui::TextWrapped("%s",text.empty()?"No text was returned for this entry; no journal content has been invented.":text.c_str());
        const auto page=HelpyWikiIcons::Page(Selected);if(!page.empty())ImGui::TextLinkOpenURL("Open original wiki entry",page.c_str());
    }ImGui::EndChild();if(table)ImGui::EndTable();
    ImGui::TextWrapped("Artwork and game journal media: copyright Jagex Ltd, via the Dragonwilds Wiki. Wiki text: CC BY-NC-SA 3.0. Third-party media is not RuneSchema MIT.");
}
inline void Shutdown(){NativeJournalCache::Shutdown();JournalAuthorPanel::Shutdown();}
inline void Render(){if(ImGui::BeginTabBar("##journal-tabs",ImGuiTabBarFlags_Reorderable)){if(ImGui::BeginTabItem("Journal / Lore")){NativeJournalPanel::Render();ImGui::EndTabItem();}const auto flags=JournalAuthorPanel::OpenRequested?ImGuiTabItemFlags_SetSelected:ImGuiTabItemFlags_None;JournalAuthorPanel::OpenRequested=false;if(ImGui::BeginTabItem("Author",nullptr,flags)){JournalAuthorPanel::Render();ImGui::EndTabItem();}if(ImGui::BeginTabItem("Wiki reference")){Wiki();ImGui::EndTabItem();}ImGui::EndTabBar();}}
}
