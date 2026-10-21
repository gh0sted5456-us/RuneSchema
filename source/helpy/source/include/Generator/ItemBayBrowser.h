#pragma once
#include "Generator/ItemGridPicker.h"
#include <functional>
namespace PS::ItemBayBrowser {
// One control strip for the save editor and live inventory workbench. Callers
// scope their IDs and pass the same viewport height as the inventory pane.
inline void Controls(ItemGridPicker::State& state,char* filter,size_t capacity,const std::function<void()>& refreshOverride={},bool categories=true){
    const auto refresh=[&]{state.CatalogRevision=0;state.Matches.clear();state.PageIndex=0;state.ResetScroll=true;if(refreshOverride)refreshOverride();else LazyCatalog::Search(0,ItemGridPicker::BackendQuery(state));};
    if(categories&&ImGui::BeginTabBar("##item-bay-tabs",ImGuiTabBarFlags_FittingPolicyScroll|ImGuiTabBarFlags_Reorderable)){
        for(const auto* category:{"All","Weapons","Armor","Consumables","Ammo","Resources","Tools","Quest"})if(ImGui::BeginTabItem(category)){
            if(state.ActiveCategory!=category){state.ActiveCategory=category;HelpyImGuiTheme::Current.itemCategory=category;refresh();}ImGui::EndTabItem();
        }ImGui::EndTabBar();
    }
    const auto& t=HelpyImGuiTheme::Current;
    ImGui::SetNextItemWidth(std::max(60.f,ImGui::GetContentRegionAvail().x-t.X(135)));
    const bool enter=ImGui::InputTextWithHint("##item-bay-search","Search items…",filter,capacity,ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();const bool search=HelpyImGuiTheme::SoftButton("Search");ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Refresh")){if(refreshOverride)refresh();else{LazyCatalog::Retry(0);state.CatalogRevision=0;}}
    if(search||enter){state.Query=filter;refresh();}
    ImGui::SetNextItemWidth(-1);if(ImGui::BeginCombo("##item-bay-source",ItemSource::Label(state.SourceFilter))){
        for(const auto kind:{ItemSource::Kind::All,ItemSource::Kind::Vanilla,ItemSource::Kind::Expansion,ItemSource::Kind::Modded,ItemSource::Kind::Unknown})if(ImGui::Selectable(ItemSource::Label(kind),state.SourceFilter==kind)){
            state.SourceFilter=kind;if(categories)HelpyImGuiTheme::Current.itemSource=ItemSource::Token(kind);refresh();
        }ImGui::EndCombo();
    }
    if(PersonalizationPanel::Filters(state.FavoritesOnly,state.PersonalTag)){state.CatalogRevision=0;state.Matches.clear();state.PageIndex=0;state.ResetScroll=true;}
    ImGui::TextDisabled("Item library");if(!state.FamilyKey.empty()){ImGui::SameLine();if(ImGui::SmallButton("Clear family")){state.FamilyKey.clear();refresh();}}
}
}
