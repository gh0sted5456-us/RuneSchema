#pragma once
#include "Generator/HelpyTextInput.h"
#include "Generator/ItemGridPicker.h"
#include <functional>
namespace PS::ItemBayBrowser {
// One control strip for the save editor and live inventory workbench. Callers
// scope their IDs and pass the same viewport height as the inventory pane.
inline void Controls(ItemGridPicker::State& state,const nlohmann::json& catalog,uint64_t revision,char* filter,size_t capacity,const std::function<void()>& refreshOverride={},bool categories=true){
    // Each surface owns its selection. The embedded browsers use the same Item Bay query as the main pane.
    auto& shared=ItemGridPicker::Bay;
    const auto syncQuery=[](ItemGridPicker::State& to,const ItemGridPicker::State& from){
        to.Query=from.Query;to.SourceFilter=from.SourceFilter;to.SourceProvider=from.SourceProvider;
        to.ActiveCategory=from.ActiveCategory;to.FamilyKey=from.FamilyKey;
        to.CatalogRevision=0;to.Matches.clear();to.PageIndex=0;to.ResetScroll=true;
    };
    const bool sync=!refreshOverride&&(state.Query!=shared.Picker.Query||state.SourceFilter!=shared.Picker.SourceFilter
        ||state.SourceProvider!=shared.Picker.SourceProvider||state.ActiveCategory!=shared.Picker.ActiveCategory||state.FamilyKey!=shared.Picker.FamilyKey);
    if(sync){
        // The shared text keeps any typed source: prefix; the query itself is the rest.
        syncQuery(state,shared.Picker);std::snprintf(filter,capacity,"%s",ItemSourcePrefix::Parse(shared.Filter).Text==state.Query?shared.Filter:state.Query.c_str());
    }
    const auto refresh=[&]{
        state.CatalogRevision=0;state.Matches.clear();state.PageIndex=0;state.ResetScroll=true;
        if(refreshOverride){refreshOverride();return;}
        syncQuery(shared.Picker,state);shared.Picker.SearchInput=state.Query;shared.Picker.SearchDue=0;
        std::snprintf(shared.Filter,sizeof(shared.Filter),"%s",ItemSourcePrefix::Parse(filter).Text==state.Query?filter:state.Query.c_str());
        LazyCatalog::Search(0,ItemGridPicker::BackendQuery(state));
    };
    const bool familyChanged=!refreshOverride&&ItemGridPicker::ApplyPendingFamily(state,filter,true);
    const bool preferencesChanged=!refreshOverride&&ItemGridPicker::ApplySearchPreferences(state);
    if(familyChanged||preferencesChanged)refresh();
    const bool syncTabs=sync||familyChanged||preferencesChanged;
    if(categories&&ImGui::BeginTabBar("##item-bay-tabs",ImGuiTabBarFlags_FittingPolicyScroll|ImGuiTabBarFlags_Reorderable)){
        for(const auto* category:ItemCategory::Categories)if(ImGui::BeginTabItem(category,nullptr,syncTabs&&state.ActiveCategory==category?ImGuiTabItemFlags_SetSelected:0)){
            if(!syncTabs&&state.ActiveCategory!=category){state.ActiveCategory=category;HelpyImGuiTheme::Current.itemCategory=category;refresh();}ImGui::EndTabItem();
        }ImGui::EndTabBar();
    }
    const bool canStop=!refreshOverride&&HelpyCachedCatalog::SearchCanStop();
    const char* refreshLabel=canStop?"Stop search":"Refresh";
    const auto& style=ImGui::GetStyle();
    const float actions=ImGui::CalcTextSize("Search").x+ImGui::CalcTextSize(refreshLabel).x+4*style.FramePadding.x+2*style.ItemSpacing.x;
    ImGui::SetNextItemWidth(std::max(1.f,ImGui::GetContentRegionAvail().x-actions));
    const bool enter=HelpyTextInput::InputTextWithHint("##item-bay-search","Search items…",filter,capacity,ImGuiInputTextFlags_EnterReturnsTrue);ItemGridPicker::SearchSyntaxTooltip();
    ImGui::SameLine();const bool search=HelpyImGuiTheme::SoftButton("Search");ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton(refreshLabel)){
        if(canStop){shared.Picker.SearchDue=0;HelpyCachedCatalog::StopSearch();}
        else if(refreshOverride)refresh();
        else{LazyCatalog::Retry(0);state.CatalogRevision=0;}
    }
    if(search||enter){state.Query=ItemGridPicker::ApplyTypedSource(state,catalog,revision,filter,categories);refresh();}
    if(!state.SourceNotice.empty())ImGui::TextDisabled("%s",state.SourceNotice.c_str());
    ImGui::SetNextItemWidth(-1);ItemGridPicker::SourceControl(state,catalog,revision,refresh,categories,filter,capacity);
    if(PersonalizationPanel::Filters(state.FavoritesOnly,state.PersonalTag)){state.CatalogRevision=0;state.Matches.clear();state.PageIndex=0;state.ResetScroll=true;}
    ImGui::TextDisabled("Item library");if(!state.FamilyKey.empty()){ImGui::SameLine();if(ImGui::SmallButton("Clear family")){state.FamilyKey.clear();refresh();}}
}
}
