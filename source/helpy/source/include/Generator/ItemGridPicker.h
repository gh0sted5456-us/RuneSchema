#include "RuntimePaths.h"
#pragma once
#include "Generator/HelpyTextInput.h"
#include "Generator/SourceBanners.h"
#include "Generator/PersonalizationPanel.h"
#include "Core/ItemNameOrder.h"
#include "Core/ItemCatalogFilter.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/HelpyMenu.h"
#include "Generator/DragFeedback.h"
#include "Generator/ItemIconCache.h"
#include "Generator/HelpyIconPacks.h"
#include "Generator/ToolRequest.h"
#include "Generator/LazyCatalog.h"
#include "Generator/HelpyCachedCatalog.h"
#include "Generator/ItemDetails.h"
#include "Core/ConfigFiles.h"
#include "Core/ItemCart.h"
#include "Core/ItemTransfer.h"
#include "Core/ItemBadges.h"
#include "Core/ItemSource.h"
#include "Core/ItemFamily.h"
#include "Core/ItemPopulation.h"
#include "Core/LoadoutEligibility.h"
#include "Generator/HelpyLoadingIndicator.h"
#include "Core/HelpyPreviewHooks.h"
#include "Core/ItemPreviewRequest.h"
#include "Generator/QuickMenuDecorations.h"
#include "Runtime/HostServices.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include <atomic>
#include <optional>
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <imgui_internal.h>
namespace PS::ItemGridPicker {
using json=nlohmann::json;
inline json CachedSlotRows(const std::string& slot,const std::string& query,uint64_t generation){
    json result=json::array();const auto& cache=LazyCatalog::For(0);if(cache.RuntimeGeneration!=generation)return result;
    std::unordered_set<std::string> seen;const ItemCatalogFilter::Query filter(query);
    const auto append=[&](const json& rows){for(const auto& row:rows){const auto path=ItemPresentation::Text(row,"Path");if(result.size()<2048&&!path.empty()&&LoadoutEligibility::Matches(row,slot)&&filter(row)&&seen.insert(path).second)result.push_back(row);}};
    append(cache.Rows);for(const auto& [key,window]:cache.Cache)if(window.Generation==generation)append(window.Rows);return result;
}
inline void DrawItemBadges(const json& row,const ImVec2& min,const ImVec2& max,const HelpyImGuiTheme::Theme& theme,float badgeSize=28.0f,bool masterwork=true);
struct State {
    std::string Query,SelectedPath,ActiveCategory{"All"};
    ItemSource::Kind SourceFilter=ItemSource::Kind::All;
    std::string SourceProvider;
    char SourceSearch[128]{};
    uint64_t SourceChoicesRevision=~uint64_t{},SourceChoicesSources=~uint64_t{},SourceChoicesLabels=~uint64_t{};
    std::vector<std::pair<std::string,std::string>> SourceChoices;
    std::string FamilyKey,PendingFamilyKey,PendingFamilyProvider;
    bool FavoritesOnly=false;std::string PersonalTag;
    uint64_t PersonalRevision=~uint64_t{};
    std::string SearchInput;
    double SearchDue=0;
    int PageIndex=0,PageSize=ItemPaging::DefaultSize;
    bool ResetScroll=false;
    std::vector<size_t> Matches;
    std::vector<std::string> SelectedPaths;
    json SelectedRows=json::object();bool MultiSelect=false;
    uint64_t CatalogRevision=0,SelectedRevision=0;
    size_t SelectedIndex=static_cast<size_t>(-1);
    bool GridView=false,ViewInitialized=false,ShowPlaceholders=false;
    bool InternalNames=false;
    float ResultPaneHeightLogical=-1.0f;
    uint64_t ViewPreferenceRevision=0;
    std::set<std::string> VisibleProgressIcons;
    int ProgressPage=-1;
};
inline ItemCart::Model SharedCart;
inline ImTextureRef ModLogo{},ModLogoAccent{};
inline bool SearchAsYouType=false;
inline bool ResizeDivider(const char* id,bool horizontal,float& value,float minimum,float maximum,float unitsPerPixel=1.0f){
    const auto& theme=HelpyImGuiTheme::Current;ImGui::PushID(id);
    const float thickness=std::max(theme.Y(8),1.0f);
    ImGui::InvisibleButton("##resize",horizontal?ImVec2(ImGui::GetContentRegionAvail().x,thickness):ImVec2(thickness,ImGui::GetContentRegionAvail().y));
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::ResizeHandles[id]=HelpyPreviewHooks::LastRect();
#endif
    const bool active=ImGui::IsItemActive(),hover=ImGui::IsItemHovered();
    if(active||hover)ImGui::SetMouseCursor(horizontal?ImGuiMouseCursor_ResizeNS:ImGuiMouseCursor_ResizeEW);
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();const auto c=ImVec2((a.x+b.x)*.5f,(a.y+b.y)*.5f);
    const float extent=theme.X(active||hover?42:28);
    ImGui::GetWindowDrawList()->AddLine(horizontal?ImVec2(c.x-extent*.5f,c.y):ImVec2(c.x,c.y-extent*.5f),horizontal?ImVec2(c.x+extent*.5f,c.y):ImVec2(c.x,c.y+extent*.5f),ImGui::GetColorU32(active||hover?theme.accentHover:theme.border),theme.X(2));
    if(hover&&!active)PS::HelpyImGuiTheme::Tooltip("Drag to resize");
    auto* storage=ImGui::GetStateStorage();const auto key=ImGui::GetID("dirty");
    if(active){const float delta=horizontal?ImGui::GetIO().MouseDelta.y:ImGui::GetIO().MouseDelta.x;
        if(delta!=0){value=std::clamp(value+delta*unitsPerPixel,minimum,std::max(minimum,maximum));storage->SetBool(key,true);}}
    const bool save=storage->GetBool(key)&&!ImGui::IsMouseDown(0);if(save){storage->SetBool(key,false);HelpyImGuiTheme::SaveCurrent();}
    ImGui::PopID();return active;
}
inline void CardScaleControl(){
    auto& theme=HelpyImGuiTheme::Current;int percent=static_cast<int>(std::lround(theme.gridCardHeight/116.0f*100.0f));
    ImGui::SetNextItemWidth(theme.X(120));
    if(ImGui::SliderInt("Card size",&percent,65,175,"%d%%",ImGuiSliderFlags_NoInput)){
        const float scale=percent/100.0f;
        theme.gridCardHeight=std::clamp(116.0f*scale,72.0f,260.0f);theme.gridMinCardWidth=std::clamp(132.0f*scale,92.0f,320.0f);theme.gridIconSize=std::clamp(60.0f*scale,28.0f,160.0f);
    }
    if(ImGui::IsItemDeactivatedAfterEdit())HelpyImGuiTheme::SaveCurrent();
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::CardScale=HelpyPreviewHooks::LastRect();
#endif
}
inline void IconCacheControl(bool catalogueBusy){
    if(ImGui::SmallButton("Cache tools..."))HelpyIconPacks::Open=true;
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::IconCache=HelpyPreviewHooks::LastRect();
#endif
    if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("Import a shared pack, pull game icons, export or update differences");
}
inline std::string CartStatus;
inline ImVec2 DetailsSurfaceMin{},DetailsSurfaceSize{};
inline void PinDetails(const json& row);
inline std::string IconForRow(const json& row){
    const auto marker=row.find("_HelpyIconPreview");
    if(marker!=row.end()&&marker->is_boolean()&&marker->get<bool>())return ItemPresentation::Icon(row);
    const auto path=ItemPresentation::Text(row,"Path");
    if(const auto confirmed=LazyCatalog::NativeIcons.find(path);confirmed!=LazyCatalog::NativeIcons.end())return confirmed->second;
    const auto* details=ItemDetails::Find(path);return ItemPresentation::Icon(row,details?&details->Native:nullptr);
}
inline bool AddToCart(const json& row){if(!row.is_object())return false;auto current=row;current["Name"]=ItemPresentation::Name(row);current["Icon"]=IconForRow(row);const bool added=SharedCart.Add(current);if(added)CartStatus.clear();else CartStatus=SharedCart.Delivery()?"Wait for delivery to complete before changing the cart.":"Cart accepts up to 64 distinct items with canonical asset paths.";return added;}
inline void RemoveFromCart(const std::string& path){SharedCart.Remove(path);}
inline void ClearSharedCart(){SharedCart.Clear();CartStatus.clear();}
inline void HeldItem(const json& row,const char* kind="Item"){
    auto* texture=ItemIconCache::Get(IconForRow(row));DragFeedback::Held(kind,ItemPresentation::Name(row).c_str(),texture&&texture->Status==ImTextureStatus_OK?texture->GetTexRef():ImTextureRef{});
}
inline void DragItem(const json& row,const std::vector<std::string>* selection=nullptr){
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::ItemRects[row.value("Path",std::string{})]=HelpyPreviewHooks::LastRect();
#endif
    if(!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID|ImGuiDragDropFlags_SourceNoPreviewTooltip))return;
    const auto path=row.value("Path",std::string{});
    const bool group=selection&&selection->size()>1&&std::find(selection->begin(),selection->end(),path)!=selection->end();
    const auto* current=ImGui::GetDragDropPayload();
    if(current&&current->DataFrameCount>=0)
        ImGui::SetDragDropPayload(current->DataType,current->Data,current->DataSize,ImGuiCond_Once);
    else try {
        if(group) {
            const auto data=json(*selection).dump();
            ImGui::SetDragDropPayload("RS_CART_SELECTION",data.c_str(),data.size()+1,ImGuiCond_Once);
        }else{
            auto compact=ItemTransfer::Compact(row);compact["Name"]=ItemPresentation::Name(row);compact["Icon"]=IconForRow(row);
            const auto data=compact.dump();
            ImGui::SetDragDropPayload("RS_ITEM_ROW",data.c_str(),data.size()+1,ImGuiCond_Once);
        }
    }catch(const std::exception&){}
    if(group){const auto title=std::to_string(selection->size())+" selected";DragFeedback::Held("Items",title.c_str());}
    else HeldItem(row);
    ImGui::EndDragDropSource();
}
// Quick actions can be hidden in an editor without disabling item transfers.
inline std::optional<json> AcceptItemDrop(std::string& error) {
    if(const auto* payload=ImGui::AcceptDragDropPayload("RS_ITEM_ROW"))try {
        auto item=ItemTransfer::Decode(payload->Data,static_cast<std::size_t>(payload->DataSize));error.clear();return item;
    }catch(const std::exception& e){error=e.what();}return {};
}
enum class ContextAction { None, GiveOne, GiveStack, DropOne, DropStack, SendToPlayer };
struct Result {
    bool Changed=false;
    std::string SelectedTitle;
    std::vector<std::string> SelectedPaths;
    const json* SelectedRow=nullptr;
    size_t MatchCount=0,SelectedCount=0;
    ContextAction RequestedAction=ContextAction::None;
    std::string ActionPath;
    int ActionCount=1;
    json ActionItems=json::array();
    json ActionRow;
};
inline std::string Lower(std::string value){for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;}
inline std::string BackendQuery(const State& state){const auto query=HelpyPersonalization::QueryUsesLabels(state.Query)?std::string{}:state.Query;return ItemSource::Query(state.SourceFilter,state.ActiveCategory,
    state.FamilyKey.empty()?query:"__helpyfamily:"+state.FamilyKey+(query.empty()?std::string{}:" "+query),state.SourceProvider);}
inline std::string ProviderLabel(const std::string& provider){
    const auto kind=ItemSource::Decode(provider);const auto name=provider.substr(provider.find('/')+1);
    if(kind==ItemSource::Kind::Modded){
        const auto nickname=HelpyPersonalization::Nickname(HelpyPersonalization::ModKey(name));if(!nickname.empty())return nickname;
        const auto found=HelpySourceCache::Current.DisplayNames.find(name);if(found!=HelpySourceCache::Current.DisplayNames.end())return found->second;
    }
    return ItemSource::DisplayName({kind,name});
}
inline std::vector<std::pair<std::string,std::string>> ProviderChoices(const json& catalog){
    std::set<std::string> providers;
    const auto add=[&](const std::string& value){if(providers.size()<1024&&value!="core"&&ContentSource::Valid(value))providers.insert(HelpyCacheSources::Lower(value));};
    const auto& snapshot=HelpySourceCache::Current;
    for(const auto& [owner,label]:snapshot.DisplayNames)add(ContentSource::Mod(owner));
    for(const auto& plugin:snapshot.NativePlugins)add("plugins/"+plugin);
    for(const auto& [file,container]:snapshot.Containers.items())if(container.value("Status",std::string{})=="Indexed")add(container.value("Owner",std::string{}));
    if(const auto rows=catalog.find("Items");rows!=catalog.end()&&rows->is_array())for(const auto& row:*rows)add(ItemSource::Provider(ItemSource::Row(row)));
    std::vector<std::pair<std::string,std::string>> choices;for(const auto& provider:providers)choices.emplace_back(provider,ProviderLabel(provider));
    std::sort(choices.begin(),choices.end(),[](const auto& a,const auto& b){return ItemNameOrder::Key(a.second,a.first)<ItemNameOrder::Key(b.second,b.first);});return choices;
}
inline bool SourceControl(State& state,const json& catalog,uint64_t revision,const std::function<void()>& refresh,bool sharedPreference=true){
    const auto label=state.SourceProvider.empty()?std::string(ItemSource::Label(state.SourceFilter)):ProviderLabel(state.SourceProvider);
    bool changed=false;const bool opened=ImGui::BeginCombo("##item-source",label.c_str(),ImGuiComboFlags_HeightLarge);
    if(!opened&&(ImGui::IsItemHovered()||ImGui::IsItemFocused()))HelpyImGuiTheme::Tooltip("%s",label.c_str());
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::SourceFilter=HelpyPreviewHooks::LastRect();
#endif
    if(opened){
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::SourceOptions.clear();
#endif
        if(ImGui::IsWindowAppearing())state.SourceSearch[0]=0;
        ImGui::SetNextItemWidth(-1);HelpyTextInput::InputTextWithHint("##source-search","Find a mod or source...",state.SourceSearch,sizeof(state.SourceSearch));
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::SourceSearch=HelpyPreviewHooks::LastRect();
#endif
        const auto choose=[&](ItemSource::Kind kind,const std::string& provider){state.SourceFilter=kind;state.SourceProvider=provider;changed=true;};
        for(const auto kind:{ItemSource::Kind::All,ItemSource::Kind::Vanilla,ItemSource::Kind::Expansion,ItemSource::Kind::Modded,ItemSource::Kind::Unknown}){
            const bool selected=state.SourceProvider.empty()&&state.SourceFilter==kind;
            if(ImGui::Selectable(ItemSource::Label(kind),selected))choose(kind,{});
            if(selected)ImGui::SetItemDefaultFocus();
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::SourceOptions[ItemSource::Token(kind)]=HelpyPreviewHooks::LastRect();
#endif
        }
        const auto labels=HelpyPersonalization::Revision.load();
        if(state.SourceChoicesRevision!=revision||state.SourceChoicesSources!=HelpySourceCache::Revision||state.SourceChoicesLabels!=labels){
            state.SourceChoices=ProviderChoices(catalog);state.SourceChoicesRevision=revision;state.SourceChoicesSources=HelpySourceCache::Revision;state.SourceChoicesLabels=labels;
        }
        const auto query=Lower(state.SourceSearch);size_t matches=0;
        for(const bool mods:{true,false}){
            bool heading=false;
            for(const auto& [provider,name]:state.SourceChoices){
                if(provider.starts_with("mods/")!=mods||(!query.empty()&&Lower(name+" "+provider).find(query)==std::string::npos))continue;
                if(!heading){ImGui::SeparatorText(mods?"Mods":"Other sources");heading=true;}
                ++matches;ImGui::PushID(provider.c_str());const bool selected=state.SourceProvider==provider;
                if(ImGui::Selectable(name.c_str(),selected))choose(ItemSource::Decode(provider),provider);
                if(selected)ImGui::SetItemDefaultFocus();
                if(ImGui::IsItemHovered()||ImGui::IsItemFocused())HelpyImGuiTheme::Tooltip("%s",provider.c_str());
#if defined(HELPY_PREVIEW)
                HelpyPreviewHooks::SourceOptions[provider]=HelpyPreviewHooks::LastRect();
#endif
                ImGui::PopID();
            }
        }
        if(!query.empty()&&!matches)ImGui::TextDisabled("No matching mod or source.");
        if(sharedPreference){
            ImGui::Separator();ImGui::BeginDisabled(!HelpyCachedCatalog::Connected()||!state.SourceProvider.starts_with("mods/")||HelpyCachedCatalog::Check.has_value());
            if(ImGui::SmallButton(HelpyCachedCatalog::Check?"Checking source...":"Refresh selected mod")){
                if(!HelpyCachedCatalog::RefreshSource(state.SourceProvider))HelpyCachedCatalog::CheckStatus="Select an enabled mod source and open its game to refresh. Existing cache is retained.";
            }ImGui::EndDisabled();
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::RefreshSource=HelpyPreviewHooks::LastRect();
#endif
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))HelpyImGuiTheme::Tooltip("Refresh this mod's mounted content while keeping its current cache available.");
        }
        ImGui::EndCombo();
    }
    if(changed){
        if(sharedPreference){HelpyImGuiTheme::Current.itemSource=state.SourceProvider.empty()?ItemSource::Token(state.SourceFilter):state.SourceProvider;HelpyImGuiTheme::SaveCurrent();}
        refresh();
    }
    return changed;
}
inline int MaxStack(const json& row){try{const auto it=row.find("MaxStackSize");if(it!=row.end()){if(it->is_number_integer())return std::clamp(it->get<int>(),1,10000);if(it->is_number_unsigned())return std::clamp(static_cast<int>(it->get<unsigned>()),1,10000);if(it->is_number_float())return std::clamp(static_cast<int>(it->get<double>()),1,10000);}}catch(...){}return 1;}
inline const json* Find(const json& catalog,const char* selected) {
    if(!selected||!*selected||!catalog.contains("Items")||!catalog["Items"].is_array())return nullptr;
    for(const auto& row:catalog["Items"])if(row.value("Path",std::string{})==selected)return &row;return nullptr;
}
inline std::string Title(const json& catalog,const std::string& path){const auto* row=Find(catalog,path.c_str());return row?ItemPresentation::Name(*row):path;}
inline json BuildActionItems(const json& catalog,const State& state,const std::string& clickedPath,bool fullStack) {
    json items=json::array();
    const bool useSelection=state.SelectedPaths.size()>1&&std::find(state.SelectedPaths.begin(),state.SelectedPaths.end(),clickedPath)!=state.SelectedPaths.end();
    const auto append=[&](const std::string& target){
        int count=1;if(fullStack){const auto* targetRow=Find(catalog,target.c_str());count=targetRow?MaxStack(*targetRow):1;}
        items.push_back({{"Item",target},{"Count",count}});
    };
    if(useSelection)for(const auto& target:state.SelectedPaths)append(target);else append(clickedPath);
    return items;
}
inline void Rebuild(const json& catalog,State& state,uint64_t revision) {
    state.Matches.clear();state.CatalogRevision=revision;
    if(!catalog.contains("Items")||!catalog["Items"].is_array())return;
    const ItemCatalogFilter::Query query(state.Query);const auto& items=catalog["Items"];state.Matches.reserve(items.size());
    for(size_t i=0;i<items.size();++i) {
        const auto& row=items[i];
        if(!row.is_object())continue;
        if(!HelpyImGuiTheme::Current.itemShowPlaceholders&&ItemPresentation::Placeholder(row))continue;
        if(!ItemSource::Matches(state.SourceFilter,ItemSource::Row(row),state.SourceProvider)||!ItemFamily::MatchesRow(state.FamilyKey,row)||!PersonalizationPanel::Matches(row,state.FavoritesOnly,state.PersonalTag))continue;
        if(state.ActiveCategory!="All"&&ItemCategory::Resolve(row)!=state.ActiveCategory)continue;
        const auto labels=HelpyPersonalization::SearchText(row);if(labels.empty()){if(query(row))state.Matches.push_back(i);}else{auto searchable=row;searchable["Tags"]=ItemPresentation::Text(row,"Tags")+" "+labels;if(query(searchable))state.Matches.push_back(i);}
    }
    // Legacy/full snapshots and imported UI fixtures use the same A-Z order
    // as native paged browsing. Cache each key once rather than constructing
    // display strings on every comparison or every rendered frame.
    using Key=std::pair<std::string,std::string>;
    std::vector<std::pair<Key,size_t>> ordered;ordered.reserve(state.Matches.size());
    for(const auto index:state.Matches)ordered.push_back({ItemNameOrder::Key(ItemPresentation::Name(items[index]),ItemPresentation::Text(items[index],"Path")),index});
    std::stable_sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    for(size_t i=0;i<ordered.size();++i)state.Matches[i]=ordered[i].second;
}
inline bool IsSelected(const State& state,const std::string& path){return std::find(state.SelectedPaths.begin(),state.SelectedPaths.end(),path)!=state.SelectedPaths.end();}
inline void SyncLegacySelection(State& state,const char* selected) {
    if(state.SelectedPaths.empty()&&selected&&*selected)state.SelectedPaths.push_back(selected);
}
inline bool ControllerConfirm(){const auto& context=*ImGui::GetCurrentContext();return context.NavInputSource==ImGuiInputSource_Gamepad&&context.NavActivateId!=0&&context.NavActivateId==ImGui::GetItemID();}
inline bool ApplySelection(State& state,const std::string& path,char* selected,size_t selectedCapacity,bool additive,bool allowMulti,const json* row=nullptr,bool updatePrimary=true) {
    if(path.empty()||!selected||path.size()>=selectedCapacity)return false;
    if(!allowMulti)additive=false;
    if(additive) {
        const auto found=std::find(state.SelectedPaths.begin(),state.SelectedPaths.end(),path);
        if(found==state.SelectedPaths.end()){if(state.SelectedPaths.size()>=ItemCart::Model::Limit)return false;state.SelectedPaths.push_back(path);}else state.SelectedPaths.erase(found);
    } else {
        state.SelectedPaths.clear();state.SelectedPaths.push_back(path);
    }
    for(auto it=state.SelectedRows.begin();it!=state.SelectedRows.end();)if(!IsSelected(state,it.key()))it=state.SelectedRows.erase(it);else ++it;
    if(row&&IsSelected(state,path))state.SelectedRows[path]=*row;
    if(!updatePrimary){if(state.SelectedPaths.empty()){selected[0]=0;state.SelectedPath.clear();state.SelectedRevision=0;}return true;}
    const std::string primary=IsSelected(state,path)?path:(state.SelectedPaths.empty()?std::string{}:state.SelectedPaths.back());
    std::snprintf(selected,selectedCapacity,"%s",primary.c_str());state.SelectedPath=primary;state.SelectedRevision=0;return true;
}
inline size_t SendSelectionToCart(const json& catalog,State& state,char* selected,size_t selectedCapacity){
    const auto total=state.SelectedPaths.size();size_t added=0;
    for(auto it=state.SelectedPaths.begin();it!=state.SelectedPaths.end();){const auto& path=*it;const auto* row=Find(catalog,path.c_str());
        if(!row){const auto cached=state.SelectedRows.find(path);if(cached!=state.SelectedRows.end())row=&*cached;}
        if(row&&AddToCart(*row)){++added;state.SelectedRows.erase(path);it=state.SelectedPaths.erase(it);}else ++it;
    }
    state.SelectedPath=state.SelectedPaths.empty()?std::string{}:state.SelectedPaths.back();state.SelectedRevision=0;
    if(selected&&selectedCapacity)std::snprintf(selected,selectedCapacity,"%s",state.SelectedPath.c_str());
    CartStatus="Added "+std::to_string(added)+" of "+std::to_string(total)+" selected items to the cart.";
    if(added<total)CartStatus+=" Unavailable items or the cart limit prevented the rest; those items remain selected.";
    return added;
}
inline bool MatchesDraggedSelection(const ImGuiPayload& payload,const State& state){
    const auto data=json(state.SelectedPaths).dump();
    return state.SelectedPaths.size()>1&&payload.Data&&payload.DataSize==static_cast<int>(data.size()+1)
        &&std::memcmp(payload.Data,data.c_str(),data.size()+1)==0;
}
inline bool CartPayload(const ImGuiPayload* payload){
    return payload&&(payload->IsDataType("RS_CART_SELECTION")||payload->IsDataType("RS_ITEM_ROW"));
}
inline void AcceptCartDrop(const json& catalog,State& state,char* selected,size_t selectedCapacity){
    constexpr auto preview=ImGuiDragDropFlags_AcceptBeforeDelivery;
    if(const auto* group=ImGui::AcceptDragDropPayload("RS_CART_SELECTION",preview)){
        if(!group->IsDelivery())return;
        if(MatchesDraggedSelection(*group,state))SendSelectionToCart(catalog,state,selected,selectedCapacity);
        else CartStatus="Selection changed while dragging. Grab the selected items again.";
    }else if(const auto* payload=ImGui::AcceptDragDropPayload("RS_ITEM_ROW",preview)){
        if(!payload->IsDelivery())return;
        try{
            const auto item=ItemTransfer::Decode(payload->Data,payload->DataSize);
            const auto* row=Find(catalog,item.at("Path").get_ref<const std::string&>().c_str());
            AddToCart(row?*row:item);
        }catch(const std::exception& error){CartStatus=error.what();}
    }
}
inline bool SelectionCheck(const ImVec2& at,bool selected){
    const float side=ImGui::GetFrameHeight();const ImVec2 end(at.x+side,at.y+side);auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(at,end,ImGui::GetColorU32(ImGuiCol_FrameBg),3.f);draw->AddRect(at,end,ImGui::GetColorU32(selected?ImGuiCol_CheckMark:ImGuiCol_Border),3.f);
    if(selected)ImGui::RenderCheckMark(draw,{at.x+side*.22f,at.y+side*.2f},ImGui::GetColorU32(ImGuiCol_CheckMark),side*.6f);
    return ImGui::IsMouseHoveringRect(at,end);
}
inline void LoadViewPreference(State& state) {
    state.ViewInitialized=true;state.GridView=HelpyImGuiTheme::Current.itemGridView;
}
inline void SaveViewPreference(bool grid){HelpyImGuiTheme::Current.itemGridView=grid;HelpyImGuiTheme::SaveCurrent();}
inline void ResetViewPreference(){auto& theme=HelpyImGuiTheme::Current;theme.itemGridView=true;theme.itemInternalNames=false;theme.itemPageSize=ItemPaging::DefaultSize;theme.itemGridColumns=ItemBayLayout::DefaultColumns;theme.itemGridRows=ItemBayLayout::DefaultRows;theme.itemGridFitRows=true;theme.itemResultHeight=HelpyImGuiTheme::Theme{}.itemResultHeight;theme.itemCategory="All";HelpyImGuiTheme::SaveCurrent();}
inline void LayoutControls() {
    auto& theme=HelpyImGuiTheme::Current;
    const float side=ImGui::GetFrameHeight();if(HelpyImGuiTheme::SoftButton("##Item Bay settings",{side,side}))ImGui::OpenPopup("##item-bay-layout");
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();const ImVec2 center{(a.x+b.x)*.5f,(a.y+b.y)*.5f};auto* draw=ImGui::GetWindowDrawList();const auto color=ImGui::GetColorU32(theme.text);
    draw->AddCircle(center,side*.27f,color,16,std::max(1.f,theme.X(1.5f)));draw->AddCircle(center,side*.10f,color,12,std::max(1.f,theme.X(1.5f)));
    for(int tooth=0;tooth<8;++tooth){const float angle=tooth*6.2831853f/8;const auto x=std::cos(angle),y=std::sin(angle);draw->AddLine({center.x+x*side*.25f,center.y+y*side*.25f},{center.x+x*side*.39f,center.y+y*side*.39f},color,std::max(1.f,theme.X(2.f)));}
    if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Item Bay settings");
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::ItemBaySettings=HelpyPreviewHooks::LastRect();
#endif
    static bool dirty=false;
    const auto area=ImGui::GetMainViewport()->WorkSize;const float popupWidth=std::min(theme.X(350),area.x-theme.X(24));
    ImGui::SetNextWindowSizeConstraints({popupWidth,0},{popupWidth,std::max(1.f,area.y-theme.Y(24))});
    if(ImGui::BeginPopup("##item-bay-layout")) {
        ImGui::TextUnformatted("Item Bay settings");ImGui::Separator();
        CardScaleControl();const bool cacheWasOpen=HelpyIconPacks::Open;IconCacheControl(false);if(!cacheWasOpen&&HelpyIconPacks::Open)ImGui::CloseCurrentPopup();
        ImGui::Separator();
        bool cached=HelpyRuntimePreferences::CacheFirst.load(),passive=HelpyRuntimePreferences::PassiveRefresh.load();
        bool policyChanged=HelpyImGuiTheme::Toggle("Prefer saved cache",&cached);
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::ItemCacheFirstSwitch=HelpyPreviewHooks::LastRect();
#endif
        if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Use this platform's AppData cache first, including after restarting Helpy. Uncached searches still check the connected game. This is the same saved preference as Quick Settings > Cache.");
        policyChanged|=HelpyImGuiTheme::Toggle("Passive cache updates",&passive);
        if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Turn off for manual refreshes of cached searches. Cache misses still check the game. Refresh search checks the current text and filters; Refresh source checks the whole selected mod.");
        if(policyChanged){HelpyRuntimePreferences::CacheFirst=cached;HelpyRuntimePreferences::PassiveRefresh=passive;dirty=true;auto& list=LazyCatalog::For(0);list.Cache.clear();if(list.Started)LazyCatalog::Retry(0);}
        ImGui::Separator();
        if(HelpyImGuiTheme::Toggle("Show internal names",&theme.itemInternalNames))dirty=true;
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::ItemNamesSwitch=HelpyPreviewHooks::LastRect();
#endif
        ImGui::TextWrapped("Off: display names or nicknames. On: exact internal names. Changes apply immediately to loaded items. A missing display name uses a readable fallback until learned; no game query is started by this switch.");
        HelpyImGuiTheme::Toggle("Search as I type",&SearchAsYouType);
        if(ImGui::RadioButton("Grid",theme.itemGridView)){theme.itemGridView=true;dirty=true;}ImGui::SameLine();
        if(ImGui::RadioButton("List",!theme.itemGridView)){theme.itemGridView=false;dirty=true;}
        if(ImGui::CollapsingHeader("Layout & advanced")){ImGui::SetNextItemWidth(theme.X(180.0f));
        if(ImGui::SliderInt("Columns",&theme.itemGridColumns,1,ItemBayLayout::MaxColumns))dirty=true;
        ImGui::SetNextItemWidth(theme.X(180.0f));
        if(ImGui::SliderInt("Rows",&theme.itemGridRows,1,ItemBayLayout::MaxRows)){theme.itemGridFitRows=true;dirty=true;}
        if(HelpyImGuiTheme::Toggle("Size grid by rows",&theme.itemGridFitRows))dirty=true;
        if(HelpyImGuiTheme::Toggle("Show [PH] placeholder items",&theme.itemShowPlaceholders))dirty=true;
        if(HelpyImGuiTheme::SoftButton("Reset to 5 x 4")){theme.itemGridColumns=ItemBayLayout::DefaultColumns;theme.itemGridRows=ItemBayLayout::DefaultRows;theme.itemGridFitRows=true;dirty=true;}}
        if(HelpyImGuiTheme::SoftButton("Reload items / groupings")){HelpyCachedCatalog::Files.Invalidate();LazyCatalog::For(0).Cache.clear();ItemIconCache::RetryFailed();LazyCatalog::Retry(0);}
        if(HelpyImGuiTheme::SoftButton("Grouping reference...")){const auto directory=RuntimePaths::Helpy(HostServices::ModDirectory())/"settings";const auto active=directory/"item-groupings.json",example=directory/"item-groupings.example.json";const auto file=std::filesystem::is_regular_file(active)?active:std::filesystem::is_regular_file(example)?example:directory;ShellExecuteW(nullptr,L"open",file.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
        ImGui::BeginDisabled(HelpyIconPacks::Busy()||HelpyIconPacks::Building());if(HelpyImGuiTheme::SoftButton("Import taglines..."))HelpyIconPacks::ChooseTaglines(true);if(HelpyImGuiTheme::SoftButton("Export combined taglines..."))HelpyIconPacks::ChooseTaglines(false);ImGui::EndDisabled();
        if(!HelpyIconPacks::Status.empty())ImGui::TextWrapped("%s",HelpyIconPacks::Status.c_str());
        if(!HelpyCachedCatalog::Files.GroupingStatus.empty())ImGui::TextWrapped("%s",HelpyCachedCatalog::Files.GroupingStatus.c_str());ImGui::EndPopup();
    }
    if(dirty&&!ImGui::IsAnyItemActive()){HelpyImGuiTheme::SaveCurrent();dirty=false;}
}
inline void PageControls(State& state,std::size_t count,bool more,bool pending) {
    auto& theme=HelpyImGuiTheme::Current;const auto page=ItemPaging::Slice(count,state.PageIndex,state.PageSize,more);
    ImGui::AlignTextToFramePadding();
    if(page.Count)ImGui::TextDisabled("%zu-%zu%s",page.First+1,page.First+page.Count,more?"  (more available)":"");
    else if(pending||more||!LazyCatalog::For(0).Started)ImGui::Dummy({1,ImGui::GetTextLineHeight()});
    else ImGui::TextDisabled("No items");
    const float available=ImGui::GetContentRegionAvail().x;const bool narrow=available<theme.X(240);
    const char* previous=narrow?"<##Previous":"Previous";const char* next=narrow?">##Next":"Next";
    const float navigation=ImGui::CalcTextSize(previous,nullptr,true).x+ImGui::CalcTextSize(next,nullptr,true).x+4*ImGui::GetStyle().FramePadding.x+2*ImGui::GetStyle().ItemSpacing.x;
    const float pageWidth=std::min(theme.X(112),std::max(1.f,available-navigation)),controls=pageWidth+navigation;
    if(available>controls+theme.X(160))ImGui::SameLine(),ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x-controls);
    ImGui::SetNextItemWidth(pageWidth);const auto label=std::to_string(state.PageSize)+" / page";
    if(ImGui::BeginCombo("##items-per-page",label.c_str())){
        ImGui::TextDisabled("Items per page");
        for(const int size:{10,20,40,60,100})if(ImGui::Selectable(std::to_string(size).c_str(),state.PageSize==size)){
            theme.itemPageSize=size;HelpyImGuiTheme::SaveCurrent();
        }
        ImGui::Separator();int custom=theme.itemPageSize;ImGui::SetNextItemWidth(theme.X(150.0f));
        if(ImGui::InputInt("Custom",&custom)){theme.itemPageSize=ItemPaging::Size(custom);HelpyImGuiTheme::SaveCurrent();}
        ImGui::EndCombo();
    }
    ImGui::SameLine();ImGui::BeginDisabled(!page.Previous);
    if(HelpyImGuiTheme::SoftButton(previous)){--state.PageIndex;state.ResetScroll=true;}ImGui::EndDisabled();
    if(narrow&&ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))HelpyImGuiTheme::Tooltip("Previous page");
    ImGui::SameLine();ImGui::BeginDisabled(!page.Next||(pending&&page.First+page.Count>=count));
    if(HelpyImGuiTheme::SoftButton(next)){++state.PageIndex;state.ResetScroll=true;}ImGui::EndDisabled();
    if(narrow&&ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))HelpyImGuiTheme::Tooltip("Next page");
}
// Details are a tooltip, not a second hover target or a sticky selection.
// Only a source item hovered during this frame may keep them open.
struct DetailsPopupState {
    json Row;std::string Name,Path;ImVec2 AnchorMin{},AnchorMax{},Min{},Max{},SurfaceMin{},SurfaceSize{};
    int QueuedFrame=-1;bool Fits=true,Open=false,InspectNative=true;float Width=0;
    uint64_t Generation=0;
};
inline DetailsPopupState DetailsPopup;
inline void VerboseTooltip(const json& row,const std::string& name,const std::string& path,bool,bool fits,bool inspectNative=true) {
    if(ImGui::IsMouseDown(0)||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId))return;
    auto& popup=DetailsPopup;
    if(!popup.Open||popup.Path!=path){popup.Row=row;popup.Name=name;popup.Path=path;popup.AnchorMin=ImGui::GetItemRectMin();popup.AnchorMax=ImGui::GetItemRectMax();popup.Min=popup.Max={};popup.Width=0;}
    popup.AnchorMin=ImGui::GetItemRectMin();popup.AnchorMax=ImGui::GetItemRectMax();
    if(popup.Row!=row)popup.Row=row;popup.Name=name; // Different saved instances may share one ItemData path.
    popup.SurfaceMin=DetailsSurfaceMin;popup.SurfaceSize=DetailsSurfaceSize;
    popup.Fits=fits;popup.Open=true;popup.InspectNative=inspectNative;popup.QueuedFrame=ImGui::GetFrameCount();popup.Generation=SpawnToolRequests::CurrentGeneration();
    ImGui::SetKeyOwner(ImGuiKey_MouseWheelY,ImGui::GetItemID(),ImGuiInputFlags_LockThisFrame);
    if(inspectNative)ItemDetails::Request(path);
}
inline void RenderDetailsPopup() {
    auto& popup=DetailsPopup;if(!popup.Open)return;
    if(popup.QueuedFrame!=ImGui::GetFrameCount()||popup.Generation!=SpawnToolRequests::CurrentGeneration()
        ||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)||ImGui::IsKeyPressed(ImGuiKey_Escape)
        ||ImGui::IsMouseDown(0)||ImGui::IsMouseDown(1)){popup.Open=false;return;}
    ItemDetails::Poll();const auto& row=popup.Row;const auto* entry=ItemDetails::Find(popup.Path);const auto& native=entry?entry->Native:row;
    const auto& theme=HelpyImGuiTheme::Current;HelpyImGuiTheme::Scope skin(theme);
    auto title=theme.itemInternalNames?ItemPresentation::Name(row,true):HelpyPersonalization::Nickname(HelpyPersonalization::ItemKey(row));if(title.empty())title=ItemPresentation::CleanName(ItemPresentation::Text(native,"Name"));if(title.empty())title=popup.Name;
    const auto category=ItemPresentation::Text(row,"Category");auto description=ItemPresentation::Description(native);if(description.empty())description=ItemPresentation::Description(row);
    auto facts=ItemPresentation::Facts(native);
    if(row.contains("Pointer")){
        facts.insert(facts.begin(),{"Saved stack",row.value("Count",json(1)).dump()});
        facts.insert(facts.begin(),{"Saved location",ItemPresentation::Text(row,"Container")+" / "+ItemPresentation::Text(row,"Slot")});
        if(row.value("Equipped",false))facts.insert(facts.begin(),{"Equipped","Removing this instance also unlinks its loadout slot"});
    }
    const auto itemSource=ItemSource::Row(row);
    if(itemSource.Modified)facts.insert(facts.begin(),{"Modified by",itemSource.ModifiedBy.empty()?"RuneSchema":itemSource.ModifiedBy});
    if(theme.advancedMode&&!itemSource.Evidence.empty())facts.insert(facts.begin(),{"Source evidence",itemSource.Evidence});
    auto power=ItemBadges::Power(native);if(!power)power=ItemBadges::Power(row);
    const auto* viewport=ImGui::GetMainViewport();const auto origin=popup.SurfaceSize.x>0?popup.SurfaceMin:viewport->WorkPos,area=popup.SurfaceSize.x>0?popup.SurfaceSize:viewport->WorkSize;const float pad=theme.X(12),gap=theme.X(8);
    float labelWidth=0,valueWidth=0;
    for(const auto& fact:facts){labelWidth=std::max(labelWidth,ImGui::CalcTextSize(fact.Label.c_str()).x);valueWidth=std::max(valueWidth,ImGui::CalcTextSize(fact.Value.c_str()).x);}
    const float desired=std::max({theme.X(300),ImGui::CalcTextSize(title.c_str()).x+pad*2,std::min(labelWidth,theme.X(310))+std::min(valueWidth,theme.X(180))+pad*3});
    popup.Width=std::max(popup.Width,std::min(desired,theme.X(580)));
    const float width=std::min(popup.Width,std::max(1.0f,area.x-pad*2));
    const float content=std::max(1.0f,width-pad*2-ImGui::GetStyle().ScrollbarSize);
    const float labelColumn=std::clamp(labelWidth+gap,content*0.48f,content*0.65f),valueColumn=std::max(1.0f,content-labelColumn-gap);
    const auto heightOf=[](const std::string& text,float wrap){return ImGui::CalcTextSize(text.c_str(),nullptr,false,wrap).y;};
    float height=pad*2+heightOf(title,content)+gap+ImGui::GetTextLineHeightWithSpacing();
    if(!category.empty())height+=ImGui::GetTextLineHeightWithSpacing();if(power)height+=theme.Y(34);if(ItemBadges::Masterwork(row))height+=ImGui::GetTextLineHeightWithSpacing();
    if(!description.empty())height+=heightOf(description,content)+gap*2;
    for(const auto& fact:facts)height+=std::max(heightOf(fact.Label,labelColumn-gap),heightOf(fact.Value,valueColumn))+ImGui::GetStyle().CellPadding.y*2;
    if(!facts.empty())height+=gap*2;
    if(!entry||!entry->Error.empty())height+=ImGui::GetTextLineHeightWithSpacing()*2;
    if(theme.verboseTooltips)height+=heightOf(popup.Path,content)+gap*2;
    height+=theme.Y(20);height=std::min(height,std::max(1.0f,area.y-pad*2));
    const auto pointer=ImGui::GetIO().MousePos;
    const bool mouse=ImGui::IsMousePosValid()&&ImGui::IsMouseHoveringRect(popup.AnchorMin,popup.AnchorMax,false);
    const auto anchorMin=mouse?pointer:popup.AnchorMin,anchorMax=mouse?ImVec2(pointer.x+theme.X(12),pointer.y+theme.Y(12)):popup.AnchorMax;
    float x=anchorMax.x+gap;if(x+width>origin.x+area.x-pad)x=anchorMin.x-gap-width;
    x=std::clamp(x,origin.x+pad,std::max(origin.x+pad,origin.x+area.x-pad-width));
    const float y=std::clamp(anchorMin.y+gap,origin.y+pad,std::max(origin.y+pad,origin.y+area.y-pad-height));
    ImGui::SetNextWindowPos({x,y});ImGui::SetNextWindowSize({width,height});ImGui::SetNextWindowBgAlpha(0.98f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{pad,pad});
    if(ImGui::Begin("##helpy-item-details",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoInputs)){
        // Keep lengthy descriptions readable without turning the tooltip into
        // a sticky window. Wheel scrolling belongs to the hovered source item.
        if(ImGui::GetIO().MouseWheel!=0)ImGui::SetScrollY(std::clamp(ImGui::GetScrollY()-ImGui::GetIO().MouseWheel*ImGui::GetTextLineHeight()*3,0.0f,ImGui::GetScrollMaxY()));
        popup.Min=ImGui::GetWindowPos();const auto size=ImGui::GetWindowSize();popup.Max={popup.Min.x+size.x,popup.Min.y+size.y};
        SourceBanners::Draw(popup.Row,popup.Min,popup.Max);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+content);HelpyImGuiTheme::DetailsTextUnformatted(title.c_str());
        if(!HelpyPersonalization::Nickname(HelpyPersonalization::ItemKey(row)).empty())HelpyImGuiTheme::DetailsTextDisabled("{%s}",ItemPresentation::OriginalName(row).c_str());
        HelpyImGuiTheme::DetailsTextDisabled("Source: %s",ItemSource::DisplayName(itemSource).c_str());
        for(const auto& container:itemSource.Containers)HelpyImGuiTheme::DetailsText("PAK: %s",std::filesystem::path(container).filename().string().c_str());
        if(itemSource.Containers.size()>1)HelpyImGuiTheme::DetailsTextDisabled("Multiple suppliers; mount precedence unverified.");
        if(!category.empty())HelpyImGuiTheme::DetailsTextDisabled("%s",category.c_str());
        if(power){const auto pos=ImGui::GetCursorScreenPos();const float size=theme.Y(28);auto badge=row;badge["PowerLevel"]=*power;badge["Masterwork"]=false;DrawItemBadges(badge,pos,{pos.x+size,pos.y+size},theme);ImGui::Dummy({size,size});ImGui::SameLine();ImGui::Text("Power level %.0f",*power);}
        if(ItemBadges::Masterwork(row))HelpyImGuiTheme::DetailsTextColored(theme.accentHover,"Masterwork");
        if(!description.empty()){ImGui::Separator();HelpyImGuiTheme::DetailsTextUnformatted(description.c_str());}
        if(!facts.empty()){ImGui::Separator();if(ImGui::BeginTable("##native-item-facts",2,ImGuiTableFlags_SizingFixedFit,ImVec2(content,0))){
            ImGui::TableSetupColumn("Property",ImGuiTableColumnFlags_WidthFixed,labelColumn);ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthFixed,valueColumn);
            for(const auto& fact:facts){ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::PushStyleColor(ImGuiCol_Text,theme.textDisabled);ImGui::PushTextWrapPos(0);ImGui::TextWrapped("%s",fact.Label.c_str());ImGui::PopTextWrapPos();ImGui::PopStyleColor();ImGui::TableNextColumn();ImGui::PushTextWrapPos(0);ImGui::TextWrapped("%s",fact.Value.c_str());ImGui::PopTextWrapPos();}ImGui::EndTable();}}
        if(popup.Path.empty())HelpyImGuiTheme::DetailsTextUnformatted("Saved identity is unresolved. Load its source content to retrieve native stats; no item path is guessed.");
        else if(!entry)HelpyImGuiTheme::DetailsText("%s",!popup.InspectNative?"Right-click and choose Show item details to inspect this item.":SpawnToolRequests::IsAvailable()?"Loading native details...":"Load a world for native details.");else if(!entry->Error.empty())HelpyImGuiTheme::DetailsTextUnformatted(entry->Error.c_str());
        if(theme.verboseTooltips){ImGui::Separator();HelpyImGuiTheme::DetailsTextUnformatted(popup.Path.c_str());}
        if(!popup.Fits)HelpyImGuiTheme::DetailsTextUnformatted("Item path exceeds the supported field size.");ImGui::PopTextWrapPos();
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::TooltipScroll=ImGui::GetScrollY();HelpyPreviewHooks::TooltipScrollMax=ImGui::GetScrollMaxY();HelpyPreviewHooks::TooltipPath=popup.Path;HelpyPreviewHooks::TooltipFacts=facts.size();HelpyPreviewHooks::Tooltip={popup.Min.x,popup.Min.y,popup.Max.x,popup.Max.y};
#endif
    }ImGui::End();ImGui::PopStyleVar();
}
inline void InspectActions(const json& row){
    if(ImGui::MenuItem("Preview",nullptr,false,ItemPreviewRequest::Valid(row)))ItemPreviewRequest::Show(row);
    if(ImGui::MenuItem("Show item details"))PinDetails(row);
}
inline void ContextMenu(const json& catalog,const json& row,const std::string& path,const std::string& name,char* selected,size_t selectedCapacity,State& state,Result& out,bool allowMulti,bool persistCart,bool allowGive=true) {
    if(!HelpyMenu::BeginContextItem("##item-actions"))return;
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::ItemActionsPath=path;
#endif
    const size_t selectedCount=state.SelectedPaths.size();const bool multi=selectedCount>1&&IsSelected(state,path);
    ImGui::TextUnformatted(name.c_str());
    const auto origin=ItemSource::Row(row);ImGui::TextDisabled("Source: %s%s%s",ItemSource::Label(origin.Group),origin.Name.empty()?"":" / ",origin.Name.c_str());
    if(multi)ImGui::TextDisabled("Actions apply to all %zu selected items.",selectedCount);ImGui::Separator();
    InspectActions(row);
    const auto family=ItemFamily::RowKey(row);
    if(ImGui::MenuItem("View family",nullptr,false,!family.empty())){state.PendingFamilyKey=family;state.PendingFamilyProvider.clear();}
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::ViewFamily=HelpyPreviewHooks::LastRect();
#endif
    const auto provider=ItemSource::Provider(origin);
    if(ImGui::MenuItem("Show Mod Family",nullptr,false,!family.empty()&&origin.Group==ItemSource::Kind::Modded&&!provider.empty())){state.PendingFamilyKey=family;state.PendingFamilyProvider=provider;}
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::ViewModFamily=HelpyPreviewHooks::LastRect();
#endif
    if(ImGui::MenuItem(multi?"Add selected to cart":"Add to cart",nullptr,false,!SharedCart.Delivery())) {
        if(persistCart){if(multi)SendSelectionToCart(catalog,state,selected,selectedCapacity);else AddToCart(row);}
    }
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::AddCart=HelpyPreviewHooks::LastRect();
#endif
    ImGui::Separator();
    PersonalizationPanel::Menu(row);
    const std::string giveStackLabel=multi?("Give full stack ("+std::to_string(selectedCount)+" selected)"):"Give full stack";
    const std::string giveOneLabel=multi?("Give one ("+std::to_string(selectedCount)+" selected)"):"Give one";
    if(allowGive&&ImGui::MenuItem("Send to player...")){out.RequestedAction=ContextAction::SendToPlayer;out.ActionPath=path;out.ActionRow=row;}
#if defined(HELPY_PREVIEW)
    if(allowGive)HelpyPreviewHooks::QuickSendMenu=HelpyPreviewHooks::LastRect();
#endif
    if(allowGive&&multi&&ImGui::BeginMenu("Send selected")){
        if(ImGui::MenuItem(giveOneLabel.c_str())){out.RequestedAction=ContextAction::GiveOne;out.ActionPath=path;out.ActionCount=1;out.ActionItems=BuildActionItems(catalog,state,path,false);}
        if(ImGui::MenuItem(giveStackLabel.c_str())){out.RequestedAction=ContextAction::GiveStack;out.ActionPath=path;out.ActionCount=MaxStack(row);out.ActionItems=BuildActionItems(catalog,state,path,true);}ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Advanced")){if(ImGui::MenuItem("Reload item details",nullptr,false,ItemDetails::Pending==0))ItemDetails::Reload(path);ImGui::EndMenu();}
    ImGui::EndPopup();
}
inline std::string Ellipsize(const std::string& text,float maxWidth) {
    if(ImGui::CalcTextSize(text.c_str()).x<=maxWidth)return text;std::string out=text;
    while(!out.empty()&&ImGui::CalcTextSize((out+"...").c_str()).x>maxWidth){out.pop_back();while(!out.empty()&&(static_cast<unsigned char>(out.back())&0xc0)==0x80)out.pop_back();if(!out.empty()&&static_cast<unsigned char>(out.back())>=0xc0)out.pop_back();}return out+"...";
}
inline void DrawPlaceholder(const ImVec2& iconMin,const ImVec2& iconMax,const HelpyImGuiTheme::Theme& theme) {
    auto* draw=ImGui::GetWindowDrawList();const auto color=ImGui::GetColorU32(theme.border);draw->AddRect(iconMin,iconMax,color,theme.X(3.0f),0,theme.X(1.0f));
    const auto center=ImVec2((iconMin.x+iconMax.x)*0.5f,(iconMin.y+iconMax.y)*0.5f);draw->AddCircleFilled(center,theme.X(2.2f),color);
}
inline float SourceTagWidth(const json& row,float maximum,float size){
    const auto source=ItemSource::Row(row);if(source.Group!=ItemSource::Kind::Modded)return 0;
    return std::min(maximum,size);
}
inline void DrawSourceBadge(const json& row,const ImVec2& right,float maximum,float size){
    const auto source=ItemSource::Row(row);if(source.Group!=ItemSource::Kind::Modded)return;
    const auto& theme=HelpyImGuiTheme::Current;auto* draw=ImGui::GetWindowDrawList();
    const float width=SourceTagWidth(row,maximum,size);const ImVec2 point(right.x-width,right.y),end(right.x,right.y+size);
    const float pad=theme.X(2);const ImVec2 mark(point.x+pad,point.y+pad),markEnd(point.x+size-pad,point.y+size-pad);
    if(ModLogo.GetTexID()){
        draw->AddImage(ModLogo,mark,markEnd);
        if(ModLogoAccent.GetTexID())draw->AddImage(ModLogoAccent,mark,markEnd,{0,0},{1,1},ImGui::GetColorU32(theme.accent));
    }else draw->AddText({point.x+pad,point.y+pad},ImGui::GetColorU32(theme.accent),"R");
#if defined(HELPY_PREVIEW)
    ++HelpyPreviewHooks::ModBadges;
#endif
}
inline void DrawItemBadges(const json& row,const ImVec2& min,const ImVec2& max,const HelpyImGuiTheme::Theme& theme,float badgeSize,bool masterwork) {
    auto* draw=ImGui::GetWindowDrawList();float x=min.x;const float size=theme.Y(badgeSize);
    auto power=ItemBadges::Power(row);if(!power)if(const auto* entry=ItemDetails::Find(ItemPresentation::Text(row,"Path")))power=ItemBadges::Power(entry->Native);
    if(power) {
        char label[32]{};std::snprintf(label,sizeof(label),"%.0f",*power);
        if(x+size<=max.x) {
            // WBP_InventoryPowerLevelDisplay overlays the native number on its
            // glyph, rather than drawing a decorative diamond beside a label.
            // Item rows have no native zone comparison, so use NotCompared's
            // neutral glyph and do not fabricate danger colors or item tiers.
            const bool native=ItemIconCache::Draw(QuickDecorations::PowerLevelBadge,ImVec2(x,min.y),ImVec2(x+size,min.y+size),true);
            if(!native){std::snprintf(label,sizeof(label),"PL %.0f",*power);draw->AddRectFilled(ImVec2(x,min.y),ImVec2(x+size,min.y+size),ImGui::GetColorU32(theme.window),theme.X(4));}
            const float fontSize=theme.Y((native?12.0f:9.0f)*badgeSize/28.0f);auto* font=ImGui::GetFont();
            const auto extent=font->CalcTextSizeA(fontSize,1000.0f,0.0f,label);
            const ImVec2 pos(x+(size-extent.x)*0.5f,min.y+(size-extent.y)*0.5f);
            draw->AddText(font,fontSize,ImVec2(pos.x+theme.X(1.0f),pos.y+theme.Y(1.0f)),IM_COL32(0,0,0,230),label);
            draw->AddText(font,fontSize,pos,ImGui::GetColorU32(theme.text),label);
            x+=size+theme.X(7.0f);
        }
    }
    if(masterwork&&ItemBadges::Masterwork(row)&&x+ImGui::CalcTextSize("MW").x<=max.x)
        draw->AddText(ImVec2(x,min.y),ImGui::GetColorU32(theme.warning),"MW");
}
inline void PlacardTitle(const std::string& name,const ImVec2& min,const ImVec2& max,const HelpyImGuiTheme::Theme& theme) {
    auto* draw=ImGui::GetWindowDrawList();const float width=max.x-min.x;
    auto first=name,second=std::string{};
    if(ImGui::CalcTextSize(name.c_str()).x>width){
        size_t split=std::string::npos;
        for(auto at=name.find(' ');at!=name.npos;at=name.find(' ',at+1)){if(ImGui::CalcTextSize(name.substr(0,at).c_str()).x>width)break;split=at;}
        if(split!=name.npos){first=name.substr(0,split);second=Ellipsize(name.substr(split+1),width);}else first=Ellipsize(name,width);
    }
    const float line=ImGui::GetTextLineHeight(),height=second.empty()?line:line*2;
    const float y=min.y+std::max(0.0f,(max.y-min.y-height)*0.5f);
    const auto centered=[&](const std::string& text,float top){draw->AddText(ImVec2(min.x+std::max(0.0f,(width-ImGui::CalcTextSize(text.c_str()).x)*.5f),top),ImGui::GetColorU32(theme.text),text.c_str());};
    draw->PushClipRect(min,max,true);centered(first,y);
    if(!second.empty())centered(second,y+line);draw->PopClipRect();
}
// All item surfaces (browser, recipe nodes, creation slots, cart) share this
// renderer. Compact cards keep the same artwork, title, badge and border rules.
inline void DrawPlacard(const json& row,const ImVec2& min,const ImVec2& max,bool selected=false,bool hovered=false,float footerLeftInset=0) {
    const auto& theme=HelpyImGuiTheme::Current;auto* draw=ImGui::GetWindowDrawList();
    const auto path=ItemPresentation::Text(row,"Path");const auto* details=ItemDetails::Find(path);
    const auto name=ItemPresentation::Name(row,HelpyImGuiTheme::Current.itemInternalNames);
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::ItemLabels[path]=name;
#endif
    const auto icon=IconForRow(row);
    const float width=max.x-min.x,height=max.y-min.y;const bool compact=height<theme.Y(96);
    const float footer=compact?0:std::min(theme.Y(42),height*0.42f);
    hovered=hovered||(ImGui::IsWindowHovered()&&ImGui::IsMouseHoveringRect(min,max));
    const auto hoverId=ImGui::GetID((ItemPresentation::Text(row,"Path")+":placard:"+std::to_string(static_cast<int>(min.x))+":"+std::to_string(static_cast<int>(min.y))).c_str());
    const float fade=HelpyImGuiTheme::HoverFade(hoverId,hovered);
    const bool masterwork=ItemBadges::Masterwork(row);
    auto titleTheme=theme;if(masterwork)titleTheme.text=theme.accentHover;
    const auto border=masterwork?HelpyImGuiTheme::Mix(theme.border,theme.accentHover,0.42f):theme.border;
    draw->AddRectFilled(min,max,ImGui::GetColorU32(HelpyImGuiTheme::Mix(selected?theme.selection:theme.panelAlt,theme.controlHover,fade*0.7f)),theme.X(theme.childRounding));
    const auto& item=ImGui::GetCurrentContext()->LastItemData;
    const bool focused=ImGui::IsItemFocused()&&ImGui::GetIO().NavVisible&&item.Rect.Min.x==min.x&&item.Rect.Min.y==min.y&&item.Rect.Max.x==max.x&&item.Rect.Max.y==max.y;
    HelpyFocusHighlight::Draw(draw,min,max,ImGui::GetColorU32(focused?theme.accent:selected?theme.accentHover:HelpyImGuiTheme::Mix(border,theme.accent,fade*0.75f)),theme.X(theme.childRounding),focused,theme.X(selected?2.0f:1.0f));
    const float extent=std::max(1.0f,std::min({theme.Y(theme.gridIconSize),width-theme.X(20),height-footer-theme.Y(12)}));
    const ImVec2 artMin(compact?min.x+theme.X(6):min.x+(width-extent)*0.5f,min.y+(height-footer-extent)*0.5f),artMax(artMin.x+extent,artMin.y+extent);
    if(icon.empty()||!ItemIconCache::Draw(icon,artMin,artMax,true))DrawPlaceholder(artMin,artMax,theme);
    if(compact){
        const bool hasPower=ItemBadges::Power(row).has_value()||(details&&ItemBadges::Power(details->Native).has_value());
        const float sourceWidth=SourceTagWidth(row,theme.X(95),theme.Y(20));const float sourceGap=sourceWidth>0?theme.X(4):0;
        const float reserve=(hasPower?theme.X(32):0)+sourceWidth+sourceGap;
        PlacardTitle(name,ImVec2(artMax.x+theme.X(9),min.y+theme.Y(4)),ImVec2(max.x-theme.X(8)-reserve,max.y-theme.Y(4)),titleTheme);
        if(hasPower)DrawItemBadges(row,ImVec2(max.x-theme.X(30)-sourceWidth-sourceGap,min.y+theme.Y(5)),max,theme,22,false);
    }else{
        const float top=max.y-footer;draw->AddLine(ImVec2(min.x+theme.X(8),top),ImVec2(max.x-theme.X(8),top),ImGui::GetColorU32(theme.border));
        PlacardTitle(name,ImVec2(min.x+theme.X(9)+std::max(0.f,footerLeftInset),top+theme.Y(2)),ImVec2(max.x-theme.X(9),max.y-theme.Y(3)),titleTheme);
        DrawItemBadges(row,ImVec2(min.x+theme.X(6),min.y+theme.Y(4)),max,theme,28,false);
    }
    if(masterwork){
        // Keep MW in the lower-right of the artwork, clear of the title footer.
        const auto size=ImGui::CalcTextSize("MW");const ImVec2 pos(max.x-theme.X(6)-size.x,(compact?max.y-theme.Y(5):max.y-footer-theme.Y(4))-size.y);
        draw->AddRectFilled({pos.x-theme.X(2),pos.y},{max.x-theme.X(4),pos.y+size.y},ImGui::GetColorU32(theme.panelAlt),theme.X(2));
        draw->AddText(pos,ImGui::GetColorU32(theme.warning),"MW");
    }
    const float occupied=theme.X(38);
    DrawSourceBadge(row,{max.x-theme.X(5),min.y+theme.Y(4)},compact?theme.X(95):std::max(theme.Y(20),width-occupied-theme.X(12)),theme.Y(20));
    // Modified vanilla/official content retains its original provider; use a
    // small bottom-left edit mark rather than the mod-created top-right logo.
    const auto origin=ItemSource::Row(row);
    PersonalizationPanel::Marks(row,artMin,artMax);
    if(origin.Modified){
        const float size=theme.Y(compact?12:16);const ImVec2 point{min.x+theme.X(5),max.y-footer-size-theme.Y(4)};
        draw->AddRectFilled(point,{point.x+size,point.y+size},ImGui::GetColorU32(theme.panelAlt),theme.X(2));
        HelpyImGuiTheme::DrawIcon(HelpyImGuiTheme::Icon::Authoring,{point.x+size*.5f,point.y+size*.5f},size*.8f,theme.accent);
#if defined(HELPY_PREVIEW)
        ++HelpyPreviewHooks::ModifiedItemBadges;
#endif
    }
}
inline void DrawLoadingSpinner(const HelpyImGuiTheme::Theme& theme,const char* label="Loading items...") {
    const auto* draw=ImGui::GetWindowDrawList();const auto a=draw->GetClipRectMin(),b=draw->GetClipRectMax();
    const float radius=std::max(2.f,std::min(theme.Y(theme.loadingSpinnerRadius),std::min(b.x-a.x,b.y-a.y)*.5f-theme.Y(3)));
    const ImVec2 center((a.x+b.x)*.5f,(a.y+b.y)*.5f);
    const auto& loading=LazyCatalog::For(0);
    HelpyLoadingIndicator::Draw(center,radius,theme,label,"Stage: "+loading.PopulationStage+"\n"+loading.Coverage+"\nItems received: "+std::to_string(loading.Rows.size())+"\nRecords checked: "+std::to_string(loading.PopulationDone)+(loading.PopulationTotal?" / "+std::to_string(loading.PopulationTotal):" (total not known yet)")+"\nChecking local cache before requesting missing data from the runtime.");
    const auto restore=ImGui::GetCursorScreenPos();ImGui::SetCursorScreenPos({center.x-radius,center.y-radius});ImGui::InvisibleButton("##loading-actions",{radius*2,radius*2},ImGuiButtonFlags_EnableNav);
    if(ImGui::BeginPopupContextItem("##loading-menu")){
        if(ImGui::MenuItem("Cancel loading"))LazyCatalog::CancelLoading(0);
        if(ImGui::MenuItem("Retry this search"))LazyCatalog::Retry(0);ImGui::EndPopup();
    }ImGui::SetCursorScreenPos(restore);
}
inline void PopulationProgress(const LazyCatalog::State& lazy,const std::set<std::string>& visible,bool previousFrame=false,bool home=false){
    ItemPopulation::Icons icons;const auto& cache=ItemIconCache::detail::State();
    for(const auto& path:visible){const auto wanted=cache.WantedFrame.find(path);if(path.empty()||wanted==cache.WantedFrame.end()||wanted->second<ImGui::GetFrameCount()-(previousFrame?1:0))continue;++icons.Total;const auto* texture=ItemIconCache::Get(path);
        if(texture&&texture->Status==ImTextureStatus_OK)++icons.Ready;
        else if(ItemIconCache::Failed(path)||(!texture&&!HelpyEnvironment::NativeIconCaptureAvailable&&cache.DiskChecked.contains(path)))++icons.Unavailable;
    }
    const auto view=ItemPopulation::Describe(lazy.PopulationStage,!lazy.Rows.empty(),lazy.PopulationDone,icons,lazy.CheckingChanges,lazy.PopulationTotal);
    const auto& theme=HelpyImGuiTheme::Current;const auto start=ImGui::GetCursorScreenPos();const float width=std::max(1.f,ImGui::GetContentRegionAvail().x);
    const auto progressDetail="Stage: "+lazy.PopulationStage+"\n"+lazy.Coverage+"\nCatalogue records checked: "+std::to_string(lazy.PopulationDone)+(lazy.PopulationTotal?" / "+std::to_string(lazy.PopulationTotal):" (unknown total)")+"\nVisible icons ready: "+std::to_string(icons.Ready)+" / "+std::to_string(icons.Total)+"\nIcons unavailable: "+std::to_string(icons.Unavailable);
    if(home){
        if(view.Busy){const float radius=theme.Y(theme.loadingSpinnerRadius);HelpyLoadingIndicator::Draw({start.x+width*.5f,start.y+radius},radius,theme,view.Label,progressDetail);ImGui::Dummy({width,radius*2+theme.Y(4)});}
    }else{
        // A fixed slot beside View never adds/removes a row above the grid.
        // Empty bays keep their sole, larger centered loading wheel instead.
        ImGui::SameLine();const auto slot=ImGui::GetCursorScreenPos();const float size=ImGui::GetFrameHeight();ImGui::Dummy({size,size});
        if(view.Busy&&!lazy.Rows.empty()&&lazy.LastError.empty())HelpyLoadingIndicator::Draw({slot.x+size*.5f,slot.y+size*.5f},size*.34f,theme,view.Label,progressDetail);
    }
}
inline void SelectFamily(State& state,std::string family){
    state.FamilyKey=std::move(family);state.PendingFamilyKey.clear();state.Query.clear();state.SearchInput.clear();
    state.ActiveCategory="All";state.FavoritesOnly=false;state.PersonalTag.clear();
    state.PageIndex=0;state.ResetScroll=true;state.CatalogRevision=0;state.Matches.clear();
}
inline Result Render(const char* id,const json& catalog,char* filter,size_t filterCapacity,char* selected,size_t selectedCapacity,State& state,uint64_t revision,bool allowMulti=true,bool quickActions=false,bool fillSpace=false,bool uniformCards=false,bool showControls=true,float fixedViewport=0,const LazyCatalog::State* localCatalog=nullptr) {
    ItemDetails::Poll();Result out;std::set<std::string> visibleIcons;ImGui::PushID(id);LoadViewPreference(state);SyncLegacySelection(state,selected);
    if(state.PersonalRevision!=HelpyPersonalization::Revision.load()){state.PersonalRevision=HelpyPersonalization::Revision.load();state.CatalogRevision=0;state.Matches.clear();state.PageIndex=0;state.ResetScroll=true;}
    if(!filter||!filterCapacity||!selected||!selectedCapacity){ImGui::TextDisabled("Item picker buffer unavailable.");ImGui::PopID();return out;}
    if(!state.PendingFamilyKey.empty()){
        auto provider=std::move(state.PendingFamilyProvider);state.PendingFamilyProvider.clear();
        SelectFamily(state,std::move(state.PendingFamilyKey));filter[0]=0;
        if(!provider.empty()){state.SourceFilter=ItemSource::Kind::Modded;state.SourceProvider=provider;if(!localCatalog)HelpyImGuiTheme::Current.itemSource=provider;}
        if(!localCatalog)HelpyImGuiTheme::Current.itemCategory="All";
        if(!localCatalog)LazyCatalog::Search(0,BackendQuery(state));
    }
    const auto sourcePreference=ItemSource::Decode(HelpyImGuiTheme::Current.itemSource);
    const auto providerPreference=ContentSource::Valid(HelpyImGuiTheme::Current.itemSource)?HelpyImGuiTheme::Current.itemSource:std::string{};
    if(!localCatalog&&(state.SourceFilter!=sourcePreference||state.SourceProvider!=providerPreference)){state.SourceFilter=sourcePreference;state.SourceProvider=providerPreference;state.CatalogRevision=0;state.Matches.clear();state.PageIndex=0;state.ResetScroll=true;LazyCatalog::Search(0,BackendQuery(state));}
    const auto& theme=HelpyImGuiTheme::Current;if(!localCatalog&&state.ActiveCategory!=theme.itemCategory){state.ActiveCategory=theme.itemCategory;state.Matches.clear();state.CatalogRevision=0;state.PageIndex=0;state.ResetScroll=true;}
    if(state.ShowPlaceholders!=theme.itemShowPlaceholders||state.InternalNames!=theme.itemInternalNames){state.ShowPlaceholders=theme.itemShowPlaceholders;state.InternalNames=theme.itemInternalNames;state.PageIndex=0;state.ResetScroll=true;Rebuild(catalog,state,revision);}
    if(state.PageSize!=theme.itemPageSize){state.PageIndex=ItemPaging::Resize(state.PageIndex,state.PageSize,theme.itemPageSize);state.PageSize=ItemPaging::Size(theme.itemPageSize);state.ResetScroll=true;}
    if(state.CatalogRevision!=revision)Rebuild(catalog,state,revision);out.MatchCount=state.Matches.size();
    const float spacing=ImGui::GetStyle().ItemSpacing.x;
    const float width=ImGui::GetContentRegionAvail().x;
    if(showControls){
    const float searchButtonWidth=ImGui::CalcTextSize("Search").x+ImGui::GetStyle().FramePadding.x*2;
    const float refreshWidth=ImGui::CalcTextSize("Refresh search").x+ImGui::GetStyle().FramePadding.x*2;
    const float settingsWidth=ImGui::GetFrameHeight();const bool oneRow=width>=theme.X(360);
    ImGui::SetNextItemWidth(std::max(1.f,width-searchButtonWidth-spacing-(oneRow?refreshWidth+settingsWidth+spacing*2:0.f)));
    if(quickActions&&ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp,false))ImGui::SetKeyboardFocusHere();
    const bool enterSubmitted=HelpyTextInput::InputTextWithHint("##Search items","Search items...",filter,filterCapacity,ImGuiInputTextFlags_EnterReturnsTrue);
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::BaySearchFocus={ImGui::GetItemID(),ImGui::GetCurrentWindow()->ID};
#endif
    ImGui::SameLine();const bool searchSubmitted=HelpyImGuiTheme::SoftButton("Search");
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::ItemSearchButton=HelpyPreviewHooks::LastRect();
#endif
    if(state.SearchInput!=filter){state.SearchInput=filter;state.SearchDue=ImGui::GetTime()+(HelpyRuntimePreferences::CacheFirst.load()?0.1:0.4);}
    if(!SearchAsYouType)state.SearchDue=0;
    if(enterSubmitted||searchSubmitted||(state.SearchDue>0&&ImGui::GetTime()>=state.SearchDue)){
        state.SearchDue=0;state.Query=filter;state.PageIndex=0;state.ResetScroll=true;
        LazyCatalog::Search(0,BackendQuery(state));state.Matches.clear();state.CatalogRevision=0;
    }
    const bool refreshing=HelpyCachedCatalog::Check&&HelpyCachedCatalog::Check->Interactive;
    const bool cachedUpdate=!localCatalog&&state.Query==filter&&HelpyCachedCatalog::RefreshAvailable()&&BackendQuery(state)==LazyCatalog::For(0).Query;
    if(ImGui::GetItemRectMax().x+spacing+refreshWidth<=ImGui::GetWindowPos().x+ImGui::GetWindowContentRegionMax().x)ImGui::SameLine();
    ImGui::BeginDisabled(localCatalog||(!cachedUpdate&&(!HelpyCachedCatalog::Connected()||(!*filter&&state.FamilyKey.empty())))||refreshing);
    if(cachedUpdate){auto green=HelpyImGuiTheme::Current.success;green.w=.3f;ImGui::PushStyleColor(ImGuiCol_Button,green);green.w=.5f;ImGui::PushStyleColor(ImGuiCol_ButtonHovered,green);ImGui::PushStyleColor(ImGuiCol_Border,HelpyImGuiTheme::Current.success);}
    if(HelpyImGuiTheme::SoftButton(refreshing?"Refreshing...":"Refresh search")){
        const bool changed=state.Query!=filter;state.SearchDue=0;state.SearchInput=filter;state.Query=filter;
        if(changed){state.PageIndex=0;state.ResetScroll=true;state.Matches.clear();state.CatalogRevision=0;}
        if(cachedUpdate){state.PageIndex=0;state.ResetScroll=true;state.Matches.clear();state.CatalogRevision=0;HelpyCachedCatalog::RefreshCached();}else HelpyCachedCatalog::RefreshSearch(BackendQuery(state));
    }if(cachedUpdate)ImGui::PopStyleColor(3);ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))HelpyImGuiTheme::Tooltip(cachedUpdate?"Updated cached results are ready. Click to apply them to this page. Your current results stay in place until you refresh.":"Check uncached mounted items for the text currently entered, with the selected category, source and family filters. Keeps cached results and saves discoveries for later searches. Requires the mod to be enabled and mounted in a connected game.\n%s",HelpyCachedCatalog::CheckStatus.c_str());
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::RefreshSearch=HelpyPreviewHooks::LastRect();
#endif
    const auto inlineControl=[&](float next){if(ImGui::GetItemRectMax().x+spacing+next<=ImGui::GetWindowPos().x+ImGui::GetWindowContentRegionMax().x)ImGui::SameLine();};
    inlineControl(settingsWidth);LayoutControls();state.GridView=theme.itemGridView;
    const auto filterChanged=[&]{state.PageIndex=0;state.ResetScroll=true;LazyCatalog::Search(0,BackendQuery(state));state.Matches.clear();state.CatalogRevision=0;};
    const float categoryWidth=std::min(theme.X(155),width*.42f);
    ImGui::SetNextItemWidth(categoryWidth);
    if(ImGui::BeginCombo("##item-category",state.ActiveCategory=="All"?"All categories":state.ActiveCategory.c_str())){
        for(const char* category:ItemCategory::Categories)if(ImGui::Selectable(category,state.ActiveCategory==category)){
            state.ActiveCategory=category;HelpyImGuiTheme::Current.itemCategory=category;HelpyImGuiTheme::SaveCurrent();filterChanged();
        }ImGui::EndCombo();
    }
    ImGui::SameLine();ImGui::SetNextItemWidth(std::max(1.f,width-categoryWidth-spacing));SourceControl(state,catalog,revision,filterChanged);
    if(PersonalizationPanel::Filters(state.FavoritesOnly,state.PersonalTag)){state.CatalogRevision=0;state.Matches.clear();state.PageIndex=0;state.ResetScroll=true;}
    if(state.ActiveCategory!="All"||state.SourceFilter!=ItemSource::Kind::All||!state.SourceProvider.empty()||state.FavoritesOnly||!state.PersonalTag.empty()||!state.FamilyKey.empty()){
        HelpyMenu::NextControl("Clear filters");if(ImGui::SmallButton("Clear filters")){
            state.ActiveCategory="All";state.SourceFilter=ItemSource::Kind::All;state.SourceProvider.clear();state.FamilyKey.clear();state.FavoritesOnly=false;state.PersonalTag.clear();
            HelpyImGuiTheme::Current.itemCategory="All";HelpyImGuiTheme::Current.itemSource="all";HelpyImGuiTheme::SaveCurrent();filterChanged();
        }
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::ClearItemFilters=HelpyPreviewHooks::LastRect();
#endif
    }
    if(!state.FamilyKey.empty()){
        auto label=state.FamilyKey;std::replace(label.begin(),label.end(),'_',' ');for(size_t i=0;i<label.size();++i)if(i==0||label[i-1]==' ')label[i]=static_cast<char>(std::toupper(static_cast<unsigned char>(label[i])));
        if(ImGui::BeginTable("##family-filter",2,ImGuiTableFlags_SizingStretchProp)){
            ImGui::TableSetupColumn("Family",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Clear",ImGuiTableColumnFlags_WidthFixed,ImGui::CalcTextSize("Clear family").x+ImGui::GetStyle().FramePadding.x*2);
            ImGui::TableNextColumn();ImGui::TextWrapped("Family: %s",label.c_str());ImGui::TableNextColumn();
            if(ImGui::SmallButton("Clear family")){state.FamilyKey.clear();state.PageIndex=0;state.ResetScroll=true;state.CatalogRevision=0;state.Matches.clear();if(!localCatalog)LazyCatalog::Search(0,BackendQuery(state));}
            ImGui::EndTable();
        }
    }
    PopulationProgress(LazyCatalog::For(0),state.VisibleProgressIcons,true);
    }
    if(!localCatalog&&LazyCatalog::For(0).Started&&LazyCatalog::For(0).Query!=BackendQuery(state)){LazyCatalog::Search(0,BackendQuery(state));state.Matches.clear();state.CatalogRevision=0;}
    const auto& lazy=localCatalog?*localCatalog:LazyCatalog::For(0);
    if(!lazy.LastError.empty()&&!lazy.Pending){
        ImGui::PushStyleColor(ImGuiCol_Text,theme.warning);ImGui::TextWrapped("%s",lazy.LastError.c_str());ImGui::PopStyleColor();
        if(ImGui::SmallButton("Retry loading items"))LazyCatalog::Retry(0);
    }
    if(!lazy.HasMore&&!lazy.Pending&&lazy.Started){
        const int last=std::max(0,static_cast<int>((state.Matches.size()+state.PageSize-1)/state.PageSize)-1);
        if(state.PageIndex>last){state.PageIndex=last;state.ResetScroll=true;}
    }
    const auto page=ItemPaging::Slice(state.Matches.size(),state.PageIndex,state.PageSize,lazy.HasMore);
    const int wanted=(state.PageIndex+1)*state.PageSize;
    if(!localCatalog)LazyCatalog::FillTo(0,static_cast<int>(lazy.Rows.size())+std::max(0,wanted-static_cast<int>(state.Matches.size())));
    if(state.ProgressPage!=state.PageIndex){state.ProgressPage=state.PageIndex;state.VisibleProgressIcons.clear();}
    if(quickActions&&allowMulti){
        if(HelpyControllerInput::Selection.Controller&&!ImGui::IsAnyItemActive()&&!ImGui::GetIO().WantTextInput&&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)
            &&ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsKeyPressed(ImGuiKey_GamepadL3,false))state.MultiSelect=!state.MultiSelect;
        HelpyImGuiTheme::Toggle("Select multiple",&state.MultiSelect);
        if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Toggle cards with a mouse click or controller confirm. Ctrl+click also changes individual selections. Click a card's check box to add or remove it.");
        if(state.MultiSelect){
        HelpyMenu::NextControl("Select page");
        if(ImGui::SmallButton("Select page"))for(size_t i=0;i<page.Count&&state.SelectedPaths.size()<ItemCart::Model::Limit;++i){const auto index=state.Matches[page.First+i];const auto& row=catalog.at("Items").at(index);const auto path=ItemPresentation::Text(row,"Path");if(!IsSelected(state,path))ApplySelection(state,path,selected,selectedCapacity,true,true,&row,false);}
        HelpyMenu::NextControl("Clear selection");if(ImGui::SmallButton("Clear selection")){state.SelectedPaths.clear();state.SelectedRows=json::object();state.SelectedPath.clear();selected[0]=0;state.SelectedRevision=0;out.Changed=true;}
        }
    }
    if(quickActions&&allowMulti){
        HelpyControllerInput::ItemBayFrame=ImGui::GetFrameCount();HelpyControllerInput::ItemBayMultiSelect=state.MultiSelect;
        if(HelpyControllerInput::Selection.Controller&&!ImGui::IsAnyItemActive()&&!ImGui::GetIO().WantTextInput&&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)&&ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsKeyPressed(ImGuiKey_GamepadStart,false))SendSelectionToCart(catalog,state,selected,selectedCapacity);
        const bool dropping=CartPayload(ImGui::GetDragDropPayload());
        const bool canDrop=SharedCart.Delivery()==0;
        ImGui::BeginDisabled((state.SelectedPaths.empty()&&!dropping)||!canDrop);
        const auto label=(dropping?std::string("Drop into cart"):"Send to cart ("+std::to_string(state.SelectedPaths.size())+")")+"###send-selection-to-cart";
        if(ImGui::Button(label.c_str(),{ImGui::GetContentRegionAvail().x,0})&&!dropping)SendSelectionToCart(catalog,state,selected,selectedCapacity);
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::BaySendToCart=HelpyPreviewHooks::LastRect();
        HelpyPreviewHooks::BaySendFocus={ImGui::GetItemID(),ImGui::GetCurrentWindow()->ID};
#endif
        if(canDrop&&ImGui::BeginDragDropTarget()){
            AcceptCartDrop(catalog,state,selected,selectedCapacity);
            ImGui::EndDragDropTarget();
        }
        ImGui::EndDisabled();
    }
    const float availableLogical=ImGui::GetContentRegionAvail().y/std::max(0.01f,theme.Scale()*theme.density);
    const float maxPane=std::max(1.0f,availableLogical-(fillSpace?56.0f:quickActions?ItemPaging::CartHeight(SharedCart.Entries().size(),theme.advancedMode)+80.0f:100.0f));
    const float gapLogical=ImGui::GetStyle().ItemSpacing.y/std::max(0.01f,theme.Scale()*theme.density);
    const float desiredPane=fillSpace?maxPane:state.GridView&&theme.itemGridFitRows
        ?ItemBayLayout::RowsHeight(theme.itemGridRows,theme.gridCardHeight,gapLogical,theme.contentPaddingY)+2.0f
        :theme.itemResultHeight;
    const float minPane=state.GridView&&theme.itemGridFitRows?std::min(desiredPane,quickActions?140.0f:180.0f):(quickActions?140.0f:180.0f);
    state.ResultPaneHeightLogical=fixedViewport>0?fixedViewport:ItemBayLayout::PaneHeight(desiredPane,maxPane,minPane);
    HelpyImGuiTheme::BeginScroll("Rows",ImVec2(0,theme.Y(state.ResultPaneHeightLogical)),true,true);
    if(state.ResetScroll){ImGui::SetScrollY(0);state.ResetScroll=false;}
    const auto resultPos=ImGui::GetWindowPos(),resultSize=ImGui::GetWindowSize();
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::Browser=HelpyPreviewHooks::WindowRect();HelpyPreviewHooks::BrowserWindow=ImGui::GetCurrentWindow()->ID;
#endif
    if(quickActions && ImGui::BeginDragDropTargetCustom(ImRect(resultPos,ImVec2(resultPos.x+resultSize.x,resultPos.y+resultSize.y)),ImGui::GetID("return-to-items"))){
        if(const auto* payload=ImGui::AcceptDragDropPayload("RS_CART_PATH"))if(payload->DataSize>0 && payload->DataSize<=4097 && static_cast<const char*>(payload->Data)[payload->DataSize-1]=='\0')RemoveFromCart(static_cast<const char*>(payload->Data));
        ImGui::EndDragDropTarget();
    }

    const bool populationWait=lazy.LastError.empty()&&(state.CatalogRevision!=revision||!lazy.Started||lazy.Pending||lazy.PreparingAlphabetical||lazy.HasMore)&&state.Matches.empty();
    if(populationWait){if(localCatalog)ImGui::TextDisabled("Checking the cached matching items…");else{const auto progress=ItemPopulation::Describe(lazy.PopulationStage,false,lazy.PopulationDone,{},lazy.CheckingChanges,lazy.PopulationTotal);DrawLoadingSpinner(theme,progress.Label.c_str());}}
    else if(catalog.contains("Items")&&catalog["Items"].is_array()) {
        const auto& items=catalog["Items"];const int total=static_cast<int>(page.Count);
        if(!state.GridView) {
            const float iconSize=theme.Y(theme.listIconSize);const float itemHeight=std::max(ImGui::GetTextLineHeight()+theme.Y(4.0f),iconSize);const float rowHeight=itemHeight+ImGui::GetStyle().ItemSpacing.y;
            ImGuiListClipper clipper;clipper.Begin(total,rowHeight);
            while(clipper.Step()) {
                const int visibleFirst=clipper.DisplayStart,visibleLast=clipper.DisplayEnd;
                for(int rowIndex=visibleFirst;rowIndex<visibleLast;++rowIndex) {
                    const auto index=state.Matches[page.First+static_cast<size_t>(rowIndex)];if(index>=items.size())continue;
                    const auto& row=items[index];const auto path=row.value("Path",std::string{}),name=ItemPresentation::Name(row,theme.itemInternalNames),iconPath=IconForRow(row);const bool fits=path.size()<selectedCapacity,selectedThis=IsSelected(state,path);
#if defined(HELPY_PREVIEW)
                    HelpyPreviewHooks::ItemLabels[path]=name;
#endif
                    ImGui::PushID(rowIndex);ImGui::PushID(path.c_str());const float rowWidth=ImGui::GetContentRegionAvail().x;ImGui::BeginDisabled(!fits);const auto rowPos=ImGui::GetCursorScreenPos();
                    const bool clicked=ImGui::Selectable("##item",selectedThis,0,ImVec2(rowWidth,itemHeight));
                    if(fits&&ImGui::IsItemFocused()&&ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft,false))ImGui::OpenPopup("##item-actions");
#if defined(HELPY_PREVIEW)
                    HelpyPreviewHooks::ItemFocus[path]={ImGui::GetItemID(),ImGui::GetCurrentWindow()->ID};
#endif
                    const bool check=quickActions&&allowMulti&&SelectionCheck({rowPos.x+theme.X(4),rowPos.y+(itemHeight-ImGui::GetFrameHeight())*.5f},selectedThis);
                    if(clicked&&fits){const bool additive=ImGui::GetIO().KeyCtrl||ImGui::GetIO().KeyShift||state.MultiSelect||check||(quickActions&&allowMulti&&ControllerConfirm());if(ApplySelection(state,path,selected,selectedCapacity,additive,allowMulti,&row,!(quickActions&&allowMulti&&additive))){out.Changed=true;}}
                    if(fits)DragItem(row,quickActions&&allowMulti?&state.SelectedPaths:nullptr);
                    ImGui::EndDisabled();const float pad=theme.X(4.0f);const ImVec2 iconMin(rowPos.x+pad+(quickActions&&allowMulti?ImGui::GetFrameHeight()+pad:0.f),rowPos.y+(itemHeight-iconSize)*0.5f),iconMax(iconMin.x+iconSize,iconMin.y+iconSize);
                    if(!iconPath.empty())HelpyIconTrace::Event("render-row",iconPath,"name='"+name+"' row="+std::to_string(rowIndex));
                    if(iconPath.empty()||!ItemIconCache::Draw(iconPath,iconMin,iconMax,true))DrawPlaceholder(iconMin,iconMax,theme);
                    if(!iconPath.empty()&&ImGui::IsRectVisible(iconMin,iconMax))visibleIcons.insert(iconPath);
                    const float sourceWidth=SourceTagWidth(row,theme.X(112),theme.Y(20));
                    const float powerWidth=(ItemBadges::Power(row)||ItemBadges::Masterwork(row))?theme.X(130.0f):0.0f;
                    const float badgeWidth=powerWidth+sourceWidth;
                    const float textX=iconMax.x+theme.X(9.0f);const auto label=Ellipsize(name,std::max(theme.X(30.0f),rowPos.x+rowWidth-badgeWidth-textX));
                    ImGui::GetWindowDrawList()->AddText(ImVec2(textX,rowPos.y+(itemHeight-ImGui::GetTextLineHeight())*0.5f),ImGui::GetColorU32(fits?(ItemBadges::Masterwork(row)?theme.accentHover:theme.text):theme.textDisabled),label.c_str());
                    if(powerWidth>0)DrawItemBadges(row,ImVec2(rowPos.x+rowWidth-badgeWidth,rowPos.y+(itemHeight-ImGui::GetTextLineHeight())*0.5f),ImVec2(rowPos.x+rowWidth-sourceWidth,rowPos.y+itemHeight),theme);
                    DrawSourceBadge(row,{rowPos.x+rowWidth-theme.X(3),rowPos.y+(itemHeight-theme.Y(20))*.5f},theme.X(112),theme.Y(20));
                    ContextMenu(catalog,row,path,name,selected,selectedCapacity,state,out,allowMulti,true,quickActions);if(HelpyImGuiTheme::TooltipReady(std::string("item:")+path+":"+std::to_string(rowIndex),ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)))VerboseTooltip(row,name,path,allowMulti,fits,!(quickActions&&allowMulti&&(state.MultiSelect||state.SelectedPaths.size()>1||ImGui::GetIO().KeyCtrl||ImGui::GetIO().KeyShift||check)));
                    ImGui::PopID();ImGui::PopID();
                }
            }
        } else {
            const float spacing=ImGui::GetStyle().ItemSpacing.x,minCardWidth=theme.X(theme.gridMinCardWidth),avail=std::max(1.0f,ImGui::GetContentRegionAvail().x-theme.X(2.0f));
            const int columns=ItemBayLayout::Columns(avail,minCardWidth,spacing,theme.itemGridColumns);
            const float fittedHeight=(ImGui::GetContentRegionAvail().y-ImGui::GetStyle().ItemSpacing.y*(theme.itemGridRows-1))/std::max(1,theme.itemGridRows);
            const float cardWidth=std::max(1.0f,(avail-spacing*(columns-1))/columns),cardHeight=!uniformCards&&theme.itemGridFitRows?std::clamp(fittedHeight,std::min(theme.Y(112.0f),theme.Y(theme.gridCardHeight)),theme.Y(theme.gridCardHeight)):theme.Y(theme.gridCardHeight),
                iconSize=std::max(1.0f,std::min({theme.Y(theme.gridIconSize),cardWidth-theme.X(14.0f),cardHeight-theme.Y(44.0f)})),gridRowHeight=cardHeight+ImGui::GetStyle().ItemSpacing.y;
            const int gridRows=(total+columns-1)/columns;ImGuiListClipper clipper;clipper.Begin(gridRows,gridRowHeight);
            while(clipper.Step()) {
                const int visibleFirstRow=clipper.DisplayStart,visibleLastRow=clipper.DisplayEnd;const int visibleFirst=visibleFirstRow*columns,visibleLast=std::min(total,visibleLastRow*columns);
                for(int gridRow=visibleFirstRow;gridRow<visibleLastRow;++gridRow) {
                    for(int col=0;col<columns;++col) {
                        const int rowIndex=gridRow*columns+col;if(rowIndex>=total)break;const auto index=state.Matches[page.First+static_cast<size_t>(rowIndex)];if(index>=items.size())continue;
                        const auto& row=items[index];const auto path=row.value("Path",std::string{}),name=ItemPresentation::Name(row,theme.itemInternalNames),iconPath=IconForRow(row);const bool fits=path.size()<selectedCapacity,selectedThis=IsSelected(state,path);
                        if(!iconPath.empty())visibleIcons.insert(iconPath);
                        ImGui::PushID(rowIndex);ImGui::PushID(path.c_str());const auto cardMin=ImGui::GetCursorScreenPos();ImGui::BeginDisabled(!fits);const bool clicked=ImGui::InvisibleButton("##card",ImVec2(cardWidth,cardHeight),ImGuiButtonFlags_EnableNav);ImGui::EndDisabled();const auto cardMax=ImVec2(cardMin.x+cardWidth,cardMin.y+cardHeight);auto* draw=ImGui::GetWindowDrawList();
                        if(fits&&ImGui::IsItemFocused()&&ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft,false))ImGui::OpenPopup("##item-actions");
#if defined(HELPY_PREVIEW)
                        HelpyPreviewHooks::ItemFocus[path]={ImGui::GetItemID(),ImGui::GetCurrentWindow()->ID};
#endif
                        DrawPlacard(row,cardMin,cardMax,selectedThis,ImGui::IsItemHovered()||ImGui::IsItemFocused(),quickActions&&allowMulti?ImGui::GetFrameHeight()+theme.X(12):0.f);
                        const bool check=quickActions&&allowMulti&&SelectionCheck({cardMin.x+theme.X(6),cardMax.y-theme.Y(42)+std::max(0.f,(theme.Y(42)-ImGui::GetFrameHeight())*.5f)},selectedThis);
                        if(clicked&&fits){const bool additive=ImGui::GetIO().KeyCtrl||ImGui::GetIO().KeyShift||state.MultiSelect||check||(quickActions&&allowMulti&&ControllerConfirm());if(ApplySelection(state,path,selected,selectedCapacity,additive,allowMulti,&row,!(quickActions&&allowMulti&&additive))){out.Changed=true;}}
                        if(fits)DragItem(row,quickActions&&allowMulti?&state.SelectedPaths:nullptr);
                        ContextMenu(catalog,row,path,name,selected,selectedCapacity,state,out,allowMulti,true,quickActions);if(HelpyImGuiTheme::TooltipReady(std::string("item:")+path+":"+std::to_string(rowIndex),ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)))VerboseTooltip(row,name,path,allowMulti,fits,!(quickActions&&allowMulti&&(state.MultiSelect||state.SelectedPaths.size()>1||ImGui::GetIO().KeyCtrl||ImGui::GetIO().KeyShift||check)));
                        ImGui::PopID();ImGui::PopID();if(col<columns-1&&rowIndex+1<total)ImGui::SameLine();
                    }
                }
            }
        }
    }
    HelpyImGuiTheme::EndScroll(theme);
    state.VisibleProgressIcons=std::move(visibleIcons);
    if(!fillSpace){ImGui::PushID("results-bottom-splitter");const float splitH=std::max(theme.Y(theme.splitterWidth),theme.Y(5.0f));
    ImGui::InvisibleButton("##split",ImVec2(ImGui::GetContentRegionAvail().x,splitH));const bool splitHover=ImGui::IsItemHovered(),splitActive=ImGui::IsItemActive();
    if(splitHover||splitActive)ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    if(splitActive&&ImGui::GetIO().MouseDelta.y!=0.0f){state.ResultPaneHeightLogical+=ImGui::GetIO().MouseDelta.y/std::max(0.01f,theme.Scale()*theme.density);state.ResultPaneHeightLogical=ItemBayLayout::PaneHeight(state.ResultPaneHeightLogical,maxPane,minPane);HelpyImGuiTheme::Current.itemResultHeight=state.ResultPaneHeightLogical;if(state.GridView)HelpyImGuiTheme::Current.itemGridFitRows=false;}static bool paneDirty=false;if(splitActive)paneDirty=true;if(paneDirty&&!ImGui::IsMouseDown(ImGuiMouseButton_Left)){HelpyImGuiTheme::SaveCurrent();paneDirty=false;}
    const auto splitMin=ImGui::GetItemRectMin(),splitMax=ImGui::GetItemRectMax();if(splitHover||splitActive)ImGui::GetWindowDrawList()->AddLine(ImVec2(splitMin.x,(splitMin.y+splitMax.y)*0.5f),ImVec2(splitMax.x,(splitMin.y+splitMax.y)*0.5f),ImGui::GetColorU32(theme.accentHover),theme.X(1.5f));
    ImGui::PopID();
    }
    PageControls(state,state.Matches.size(),lazy.HasMore&&lazy.LastError.empty(),lazy.Pending||populationWait);
    if(lazy.LastError.empty()&&state.Matches.empty()&&!populationWait&&lazy.Started){
        ImGui::TextDisabled("No matching items in the loaded runtime window.");
#if defined(HELPY_PREVIEW)
        ++HelpyPreviewHooks::ItemEmptyNotices;
#endif
    }
    if(state.SelectedRevision!=revision||state.SelectedPath!=selected) {
        state.SelectedRevision=revision;state.SelectedPath=selected;state.SelectedIndex=static_cast<size_t>(-1);
        if(catalog.contains("Items")&&catalog["Items"].is_array()){const auto& items=catalog["Items"];for(size_t i=0;i<items.size();++i)if(items[i].value("Path",std::string{})==selected){state.SelectedIndex=i;break;}}
    }
    if(catalog.contains("Items")&&catalog["Items"].is_array()&&state.SelectedIndex<catalog["Items"].size())out.SelectedRow=&catalog["Items"][state.SelectedIndex];
    out.SelectedTitle=out.SelectedRow?ItemPresentation::Name(*out.SelectedRow,theme.itemInternalNames):selected;out.SelectedPaths=state.SelectedPaths;out.SelectedCount=state.SelectedPaths.size();
    if(!quickActions&&out.SelectedCount>1)ImGui::TextDisabled("%zu selected",out.SelectedCount);
    else if(!quickActions)ImGui::TextWrapped("Selected: %s",out.SelectedTitle.empty()?"None":out.SelectedTitle.c_str());
    if(!quickActions&&allowMulti&&theme.advancedMode)ImGui::TextDisabled("Shift+click to select multiple items.");ImGui::PopID();return out;
}
struct SharedBay {
    State Picker;
    char Filter[257]{},Selected[1025]{};
    json Catalog=json::object();uint64_t Generation=0,SnapshotRevision=~uint64_t{},RowsRevision=~uint64_t{},Revision=0;
    uint64_t DetailsRevision=~uint64_t{};
    json DetailsRow=json::object();bool DetailsExpanded=true,DetailsOpenRequested=false,Expanded=false;
};
inline SharedBay Bay;
inline bool DockedBay=false;
inline bool DetachedBay=false,RenderingDetachedBay=false;
inline std::function<void(const Result&)> ItemAction;
inline void SelectShared(const std::string& path){
    if(path.size()>=sizeof(Bay.Selected))return;
    const auto generation=SpawnToolRequests::CurrentGeneration();if(Bay.Generation!=generation){Bay.Generation=generation;Bay.Catalog=json::object();Bay.DetailsRow=json::object();Bay.Selected[0]=0;Bay.Picker.SelectedPaths.clear();Bay.Picker.SelectedRows=json::object();Bay.Picker.SelectedPath.clear();}
    std::snprintf(Bay.Selected,sizeof(Bay.Selected),"%s",path.c_str());Bay.Picker.SelectedPaths={path};Bay.Picker.SelectedRows=json::object();Bay.Picker.SelectedPath=path;Bay.Picker.SelectedRevision=0;
}
inline void PinDetails(const json& row){
    const auto path=ItemPresentation::Text(row,"Path");if(path.empty()||path.size()>=sizeof(Bay.Selected))return;
    SelectShared(path);Bay.Picker.SelectedRows[path]=row;Bay.DetailsRow=row;Bay.DetailsExpanded=true;Bay.DetailsOpenRequested=true;ItemDetails::Request(path);
}
inline void RenderSelectedDetails(){
    if(Bay.DetailsOpenRequested){ImGui::SetNextItemOpen(true);Bay.DetailsOpenRequested=false;}
    Bay.DetailsExpanded=ImGui::CollapsingHeader("Selected item details",ImGuiTreeNodeFlags_DefaultOpen);
    if(!Bay.DetailsExpanded)return;
    if(!*Bay.Selected){ImGui::TextDisabled("Select an item to inspect its description and stats.");return;}
    if(!Bay.Picker.MultiSelect&&Bay.Picker.SelectedPaths.size()<2)ItemDetails::Request(Bay.Selected);const auto* details=ItemDetails::Find(Bay.Selected);
    const auto& row=Bay.DetailsRow;const auto& native=details?details->Native:row;
    if(HelpyImGuiTheme::BeginPanel("##bay-selected-details",{0,0},false)){
        SourceBanners::DetailsBackground(row);
        auto name=HelpyImGuiTheme::Current.itemInternalNames?ItemPresentation::Name(row,true):HelpyPersonalization::Nickname(HelpyPersonalization::ItemKey(row));if(name.empty())name=ItemPresentation::CleanName(ItemPresentation::Text(native,"Name"));if(name.empty())name=ItemPresentation::Name(row);
        const auto a=ImGui::GetCursorScreenPos();const float size=HelpyImGuiTheme::Current.Y(36);
        ItemIconCache::Draw(IconForRow(row),a,{a.x+size,a.y+size},true);ImGui::Dummy({size,size});ImGui::SameLine();ImGui::TextWrapped("%s",name.c_str());
        if(!HelpyPersonalization::Nickname(HelpyPersonalization::ItemKey(row)).empty())ImGui::TextDisabled("{%s}",ItemPresentation::OriginalName(row).c_str());
        const auto origin=ItemSource::Row(row);ImGui::TextDisabled("Source: %s",ItemSource::DisplayName(origin).c_str());
        auto power=ItemBadges::Power(native);if(!power)power=ItemBadges::Power(row);
        if(power){const auto pos=ImGui::GetCursorScreenPos();const float badgeSize=HelpyImGuiTheme::Current.Y(28);auto badge=row;badge["PowerLevel"]=*power;DrawItemBadges(badge,pos,{pos.x+badgeSize,pos.y+badgeSize},HelpyImGuiTheme::Current);ImGui::Dummy({badgeSize,badgeSize});ImGui::SameLine();ImGui::Text("Power level %.0f",*power);}
        if(ItemBadges::Masterwork(row))ImGui::TextUnformatted("Masterwork");
        auto description=ItemPresentation::Description(native);if(description.empty())description=ItemPresentation::Description(row);
        if(!description.empty())ImGui::TextWrapped("%s",description.c_str());
        auto facts=ItemPresentation::Facts(native);
        const auto source=ItemSource::Row(row);
        if(source.Modified)facts.insert(facts.begin(),{"Modified by",source.ModifiedBy.empty()?"RuneSchema":source.ModifiedBy});
        if(HelpyImGuiTheme::Current.advancedMode&&!source.Evidence.empty())facts.insert(facts.begin(),{"Source evidence",source.Evidence});
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::BayDetailsFacts=facts.size();
#endif
        if(!facts.empty()&&ImGui::BeginTable("##bay-item-facts",2,ImGuiTableFlags_SizingStretchProp)){
            for(const auto& fact:facts){ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextWrapped("%s",fact.Label.c_str());ImGui::TableNextColumn();ImGui::TextWrapped("%s",fact.Value.c_str());}ImGui::EndTable();
        }
        if(!details)ImGui::TextDisabled("Reading native details...");else if(!details->Error.empty())ImGui::TextWrapped("%s",details->Error.c_str());
        if(ImGui::SmallButton("Reload details"))ItemDetails::Reload(Bay.Selected);ImGui::SameLine();if(ImGui::SmallButton("Copy item path"))ImGui::SetClipboardText(Bay.Selected);
    }PS::HelpyImGuiTheme::EndPanel();
}
inline Result RenderShared(const json& catalog,uint64_t revision,bool fillSpace=true){
    const auto sameRow=[&](float width){if(ImGui::GetItemRectMax().x+ImGui::GetStyle().ItemSpacing.x+width<=ImGui::GetWindowPos().x+ImGui::GetWindowContentRegionMax().x)ImGui::SameLine();};
    const auto generation=SpawnToolRequests::CurrentGeneration();if(Bay.Generation!=generation){Bay.Generation=generation;Bay.Catalog=json::object();Bay.SnapshotRevision=Bay.RowsRevision=~uint64_t{};Bay.DetailsRow=json::object();Bay.Selected[0]=0;Bay.Picker.SelectedPaths.clear();Bay.Picker.SelectedRows=json::object();Bay.Picker.SelectedPath.clear();}
    // Spawn pages request definition windows; the bay still owns the same item
    // window as Item Service, rather than treating resource definitions as items.
    const auto rowsRevision=LazyCatalog::For(0).RowsRevision;
    if(Bay.SnapshotRevision!=revision||Bay.RowsRevision!=rowsRevision){
        auto previous=Bay.Catalog.contains("Items")?std::move(Bay.Catalog["Items"]):json::array();Bay.Catalog=catalog;
        if(!Bay.Catalog.contains("Items")&&!previous.empty())Bay.Catalog["Items"]=std::move(previous);
        LazyCatalog::Overlay(Bay.Catalog,0);Bay.SnapshotRevision=revision;
        if(Bay.RowsRevision!=rowsRevision||!Bay.Catalog.contains("_CatalogRowsRevision"))++Bay.Revision;
        Bay.RowsRevision=rowsRevision;
    }
    if(!RenderingDetachedBay&&DetachedBay){ImGui::TextDisabled("Item Bay is in its separate window.");HelpyMenu::NextControl("Dock back");if(ImGui::SmallButton("Dock back"))DetachedBay=false;}
    if((DockedBay||DetachedBay)&&!RenderingDetachedBay){Result result;result.SelectedPaths=Bay.Picker.SelectedPaths;result.SelectedCount=result.SelectedPaths.size();result.SelectedTitle=Title(Bay.Catalog,Bay.Selected);result.SelectedRow=Find(Bay.Catalog,Bay.Selected);return result;}
    if(!RenderingDetachedBay){if(ImGui::SmallButton(Bay.Expanded?"Restore pane":"Expand pane"))Bay.Expanded=!Bay.Expanded;
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::BayExpand=HelpyPreviewHooks::LastRect();
#endif
    }
    if(!RenderingDetachedBay&&!DockedBay&&!HelpyImGuiTheme::NativePresentation){sameRow(HelpyImGuiTheme::Current.X(100));if(ImGui::SmallButton("Detach Item Bay"))DetachedBay=true;}
    const float available=ImGui::GetContentRegionAvail().y;const auto& theme=HelpyImGuiTheme::Current;
    const float splitter=theme.Y(8)+ImGui::GetStyle().ItemSpacing.y;
    const float browse=Bay.Expanded?std::max(1.f,available):available<theme.Y(380)?std::max(1.f,available-ImGui::GetFrameHeightWithSpacing()-splitter):ItemBayLayout::PaneHeight(Bay.DetailsExpanded?available*theme.itemBayBrowseRatio:available-ImGui::GetFrameHeightWithSpacing()-splitter,std::max(1.0f,available-theme.Y(45)-splitter),theme.Y(100));
    Result result;
    if(HelpyImGuiTheme::BeginPanel("##bay-browse-with-details",{0,browse},false))
        result=Render("shared-item-bay",Bay.Catalog,Bay.Filter,sizeof(Bay.Filter),Bay.Selected,sizeof(Bay.Selected),Bay.Picker,Bay.Revision,true,true,fillSpace,true);
    PS::HelpyImGuiTheme::EndPanel();
    if(!Bay.Expanded)ResizeDivider("item-bay-details",true,HelpyImGuiTheme::Current.itemBayBrowseRatio,.25f,.90f,1.0f/std::max(1.0f,available));
    if(Bay.DetailsRevision!=Bay.Revision||ItemPresentation::Text(Bay.DetailsRow,"Path")!=Bay.Selected){
        Bay.DetailsRevision=Bay.Revision;
        const auto* selected=result.SelectedRow?result.SelectedRow:Find(Bay.Catalog,Bay.Selected);
        if(selected)Bay.DetailsRow=*selected;
        else if(ItemPresentation::Text(Bay.DetailsRow,"Path")!=Bay.Selected)Bay.DetailsRow={{"Path",Bay.Selected}};
    }
    if(!Bay.Expanded)RenderSelectedDetails();
    if(result.RequestedAction!=ContextAction::None&&ItemAction)ItemAction(result);
    return result;
}
}
