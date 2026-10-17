#pragma once
#include "Core/ItemNameOrder.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/ItemIconCache.h"
#include "Generator/ToolRequest.h"
#include "Generator/LazyCatalog.h"
#include "Generator/ItemDetails.h"
#include "Core/ConfigFiles.h"
#include "Core/ItemCart.h"
#include "Core/ItemTransfer.h"
#include "Core/ItemBadges.h"
#include "Core/ItemSource.h"
#include "Core/ItemFamily.h"
#include "Core/HelpyPreviewHooks.h"
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
inline void DrawItemBadges(const json& row,const ImVec2& min,const ImVec2& max,const HelpyImGuiTheme::Theme& theme,float badgeSize=28.0f);
struct State {
    std::string Query,SelectedPath,ActiveCategory{"All"};
    ItemSource::Kind SourceFilter=ItemSource::Kind::All;
    std::string FamilyKey,PendingFamilyKey;
    std::string SearchInput;
    double SearchDue=0;
    int PageIndex=0,PageSize=ItemPaging::DefaultSize;
    bool ResetScroll=false;
    std::vector<size_t> Matches;
    std::vector<std::string> SelectedPaths;
    uint64_t CatalogRevision=0,SelectedRevision=0;
    size_t SelectedIndex=static_cast<size_t>(-1);
    bool GridView=false,ViewInitialized=false;
    float ResultPaneHeightLogical=-1.0f;
    uint64_t ViewPreferenceRevision=0;
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
    if(hover&&!active)ImGui::SetTooltip("Drag to resize");
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
inline void DragItem(const json& row){
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::ItemRects[row.value("Path",std::string{})]=HelpyPreviewHooks::LastRect();
#endif
    if(!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))return;
    try{auto compact=ItemTransfer::Compact(row);compact["Name"]=ItemPresentation::Name(row);compact["Icon"]=IconForRow(row);const auto data=compact.dump();ImGui::SetDragDropPayload("RS_ITEM_ROW",data.c_str(),data.size()+1);}catch(const std::exception&){}
    ImGui::TextUnformatted(ItemPresentation::Name(row).c_str());
    ImGui::TextDisabled("Drop into the cart, Transmog & Creation, or recipe inputs/outputs.");ImGui::EndDragDropSource();
}
// Quick actions can be hidden in an editor without disabling item transfers.
inline std::optional<json> AcceptItemDrop(std::string& error) {
    if(const auto* payload=ImGui::AcceptDragDropPayload("RS_ITEM_ROW"))try {
        auto item=ItemTransfer::Decode(payload->Data,static_cast<std::size_t>(payload->DataSize));error.clear();return item;
    }catch(const std::exception& e){error=e.what();}return {};
}
enum class ContextAction { None, GiveOne, GiveStack, DropOne, DropStack };
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
};
inline std::string Lower(std::string value){for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;}
inline std::string BackendQuery(const State& state){return ItemSource::Query(state.SourceFilter,state.ActiveCategory,
    state.FamilyKey.empty()?state.Query:"__helpyfamily:"+state.FamilyKey+(state.Query.empty()?std::string{}:" "+state.Query));}
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
    const auto query=Lower(state.Query);const auto& items=catalog["Items"];state.Matches.reserve(items.size());
    for(size_t i=0;i<items.size();++i) {
        const auto& row=items[i];
        if(!row.is_object())continue;
        if(!ItemSource::Matches(state.SourceFilter,ItemSource::Row(row))||!ItemFamily::Matches(state.FamilyKey,ItemPresentation::Name(row)))continue;
        if(state.ActiveCategory!="All"&&row.value("Category",std::string("Misc"))!=state.ActiveCategory)continue;
        if(query.empty()){state.Matches.push_back(i);continue;}
        const auto path=row.value("Path",std::string{}),name=ItemPresentation::Name(row);
        if(Lower(name+" "+path+" "+row.value("InternalName",std::string{})+" "+row.value("PersistenceID",std::string{})+" "+row.value("Tags",std::string{})+" "+row.value("Category",std::string{})).find(query)!=std::string::npos)
            state.Matches.push_back(i);
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
inline bool ApplySelection(State& state,const std::string& path,char* selected,size_t selectedCapacity,bool additive,bool allowMulti) {
    if(path.empty()||!selected||path.size()>=selectedCapacity)return false;
    if(!allowMulti)additive=false;
    if(additive) {
        const auto found=std::find(state.SelectedPaths.begin(),state.SelectedPaths.end(),path);
        if(found==state.SelectedPaths.end()){if(state.SelectedPaths.size()>=ItemCart::Model::Limit)return false;state.SelectedPaths.push_back(path);}else state.SelectedPaths.erase(found);
    } else {
        state.SelectedPaths.clear();state.SelectedPaths.push_back(path);
    }
    const std::string primary=IsSelected(state,path)?path:(state.SelectedPaths.empty()?std::string{}:state.SelectedPaths.back());
    std::snprintf(selected,selectedCapacity,"%s",primary.c_str());state.SelectedPath=primary;state.SelectedRevision=0;return true;
}
inline void LoadViewPreference(State& state) {
    state.ViewInitialized=true;state.GridView=HelpyImGuiTheme::Current.itemGridView;
}
inline void SaveViewPreference(bool grid){HelpyImGuiTheme::Current.itemGridView=grid;HelpyImGuiTheme::SaveCurrent();}
inline void ResetViewPreference(){auto& theme=HelpyImGuiTheme::Current;theme.itemGridView=true;theme.itemPageSize=ItemPaging::DefaultSize;theme.itemGridColumns=ItemBayLayout::DefaultColumns;theme.itemGridRows=ItemBayLayout::DefaultRows;theme.itemGridFitRows=true;theme.itemResultHeight=HelpyImGuiTheme::Theme{}.itemResultHeight;theme.itemCategory="All";HelpyImGuiTheme::SaveCurrent();}
inline void LayoutControls() {
    auto& theme=HelpyImGuiTheme::Current;
    if(HelpyImGuiTheme::SoftButton("View"))ImGui::OpenPopup("##item-bay-layout");
    static bool dirty=false;
    if(ImGui::BeginPopup("##item-bay-layout")) {
        ImGui::TextUnformatted("Item view");ImGui::Separator();
        ImGui::Checkbox("Search as I type",&SearchAsYouType);
        if(ImGui::RadioButton("Grid",theme.itemGridView)){theme.itemGridView=true;dirty=true;}ImGui::SameLine();
        if(ImGui::RadioButton("List",!theme.itemGridView)){theme.itemGridView=false;dirty=true;}
        ImGui::SeparatorText("Grid layout");ImGui::SetNextItemWidth(theme.X(180.0f));
        if(ImGui::SliderInt("Columns",&theme.itemGridColumns,1,ItemBayLayout::MaxColumns))dirty=true;
        ImGui::SetNextItemWidth(theme.X(180.0f));
        if(ImGui::SliderInt("Rows",&theme.itemGridRows,1,ItemBayLayout::MaxRows)){theme.itemGridFitRows=true;dirty=true;}
        if(ImGui::Checkbox("Size grid by rows",&theme.itemGridFitRows))dirty=true;
        ImGui::TextDisabled("Fits the available window. Page size is below the items.");
        ImGui::TextDisabled("Drag the lower divider for a free-height pane.");
        if(HelpyImGuiTheme::SoftButton("Reset to 5 x 4")){theme.itemGridColumns=ItemBayLayout::DefaultColumns;theme.itemGridRows=ItemBayLayout::DefaultRows;theme.itemGridFitRows=true;dirty=true;}
        if(HelpyImGuiTheme::SoftButton("Reload items")){ItemIconCache::RetryFailed();LazyCatalog::Retry(0);}ImGui::EndPopup();
    }
    if(dirty&&!ImGui::IsAnyItemActive()){HelpyImGuiTheme::SaveCurrent();dirty=false;}
}
inline void PageControls(State& state,std::size_t count,bool more,bool pending) {
    auto& theme=HelpyImGuiTheme::Current;const auto page=ItemPaging::Slice(count,state.PageIndex,state.PageSize,more);
    ImGui::AlignTextToFramePadding();
    if(pending)ImGui::TextDisabled("Loading items...");
    else if(page.Count)ImGui::TextDisabled("%zu-%zu%s",page.First+1,page.First+page.Count,more?"  (more available)":"");
    else ImGui::TextDisabled("No items");
    const float controls=theme.X(330.0f);if(ImGui::GetContentRegionAvail().x>controls)ImGui::SameLine(),ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x-controls);
    ImGui::SetNextItemWidth(theme.X(112.0f));const auto label=std::to_string(state.PageSize)+" / page";
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
    if(HelpyImGuiTheme::SoftButton("Previous")){--state.PageIndex;state.ResetScroll=true;}ImGui::EndDisabled();
    ImGui::SameLine();ImGui::BeginDisabled(!page.Next||pending);
    if(HelpyImGuiTheme::SoftButton("Next")){++state.PageIndex;state.ResetScroll=true;}ImGui::EndDisabled();
}
// Details are a tooltip, not a second hover target or a sticky selection.
// Only a source item hovered during this frame may keep them open.
struct DetailsPopupState {
    json Row;std::string Name,Path;ImVec2 AnchorMin{},AnchorMax{},Min{},Max{},SurfaceMin{},SurfaceSize{};
    int QueuedFrame=-1;bool Fits=true,Open=false;
    uint64_t Generation=0;
};
inline DetailsPopupState DetailsPopup;
inline void VerboseTooltip(const json& row,const std::string& name,const std::string& path,bool,bool fits) {
    if(ImGui::IsMouseDown(0)||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId))return;
    auto& popup=DetailsPopup;
    if(!popup.Open||popup.Path!=path){popup.Row=row;popup.Name=name;popup.Path=path;popup.AnchorMin=ImGui::GetItemRectMin();popup.AnchorMax=ImGui::GetItemRectMax();popup.Min=popup.Max={};}
    popup.AnchorMin=ImGui::GetItemRectMin();popup.AnchorMax=ImGui::GetItemRectMax();
    popup.SurfaceMin=DetailsSurfaceMin;popup.SurfaceSize=DetailsSurfaceSize;
    popup.Fits=fits;popup.Open=true;popup.QueuedFrame=ImGui::GetFrameCount();popup.Generation=SpawnToolRequests::CurrentGeneration();
    ImGui::SetKeyOwner(ImGuiKey_MouseWheelY,ImGui::GetItemID(),ImGuiInputFlags_LockThisFrame);
    ItemDetails::Request(path);
}
inline void RenderDetailsPopup() {
    auto& popup=DetailsPopup;if(!popup.Open)return;
    if(popup.QueuedFrame!=ImGui::GetFrameCount()||popup.Generation!=SpawnToolRequests::CurrentGeneration()
        ||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)||ImGui::IsKeyPressed(ImGuiKey_Escape)
        ||ImGui::IsMouseDown(0)||ImGui::IsMouseDown(1)){popup.Open=false;return;}
    ItemDetails::Poll();const auto& row=popup.Row;const auto* entry=ItemDetails::Find(popup.Path);const auto& native=entry?entry->Native:row;
    const auto& theme=HelpyImGuiTheme::Current;HelpyImGuiTheme::Scope skin(theme);
    auto title=ItemPresentation::CleanName(ItemPresentation::Text(native,"Name"));if(title.empty())title=popup.Name;
    const auto category=ItemPresentation::Text(row,"Category");auto description=ItemPresentation::Description(native);if(description.empty())description=ItemPresentation::Description(row);
    auto facts=ItemPresentation::Facts(native);
    const auto itemSource=ItemSource::Row(row);
    if(itemSource.Group==ItemSource::Kind::Modded)facts.insert(facts.begin(),{"Mod",itemSource.Name.empty()?"Mod source unavailable":itemSource.Name});
    auto power=ItemBadges::Power(native);if(!power)power=ItemBadges::Power(row);
    const auto* viewport=ImGui::GetMainViewport();const auto origin=popup.SurfaceSize.x>0?popup.SurfaceMin:viewport->WorkPos,area=popup.SurfaceSize.x>0?popup.SurfaceSize:viewport->WorkSize;const float pad=theme.X(12),gap=theme.X(8);
    float labelWidth=0,valueWidth=0;
    for(const auto& fact:facts){labelWidth=std::max(labelWidth,ImGui::CalcTextSize(fact.Label.c_str()).x);valueWidth=std::max(valueWidth,ImGui::CalcTextSize(fact.Value.c_str()).x);}
    const float desired=std::max({theme.X(300),ImGui::CalcTextSize(title.c_str()).x+pad*2,std::min(labelWidth,theme.X(310))+std::min(valueWidth,theme.X(180))+pad*3});
    const float width=std::min(std::min(desired,theme.X(580)),std::max(1.0f,area.x-pad*2));
    const float content=std::max(1.0f,width-pad*2-ImGui::GetStyle().ScrollbarSize);
    const float labelColumn=std::clamp(labelWidth+gap,content*0.48f,content*0.65f),valueColumn=std::max(1.0f,content-labelColumn-gap);
    const auto heightOf=[](const std::string& text,float wrap){return ImGui::CalcTextSize(text.c_str(),nullptr,false,wrap).y;};
    float height=pad*2+heightOf(title,content)+gap;
    if(!category.empty())height+=ImGui::GetTextLineHeightWithSpacing();if(power)height+=theme.Y(34);if(ItemBadges::Masterwork(row))height+=ImGui::GetTextLineHeightWithSpacing();
    if(!description.empty())height+=heightOf(description,content)+gap*2;
    for(const auto& fact:facts)height+=std::max(heightOf(fact.Label,labelColumn-gap),heightOf(fact.Value,valueColumn))+ImGui::GetStyle().CellPadding.y*2;
    if(!facts.empty())height+=gap*2;
    if(!entry||!entry->Error.empty())height+=ImGui::GetTextLineHeightWithSpacing()*2;
    if(theme.verboseTooltips)height+=heightOf(popup.Path,content)+gap*2;
    height+=theme.Y(20);height=std::min(height,std::max(1.0f,area.y-pad*2));
    float x=popup.AnchorMax.x+gap;if(x+width>origin.x+area.x-pad)x=popup.AnchorMin.x-gap-width;
    x=std::clamp(x,origin.x+pad,std::max(origin.x+pad,origin.x+area.x-pad-width));
    const float y=std::clamp(popup.AnchorMin.y,origin.y+pad,std::max(origin.y+pad,origin.y+area.y-pad-height));
    ImGui::SetNextWindowPos({x,y});ImGui::SetNextWindowSize({width,height});ImGui::SetNextWindowBgAlpha(0.98f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{pad,pad});
    if(ImGui::Begin("##helpy-item-details",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoInputs)){
        // Keep lengthy descriptions readable without turning the tooltip into
        // a sticky window. Wheel scrolling belongs to the hovered source item.
        if(ImGui::GetIO().MouseWheel!=0)ImGui::SetScrollY(std::clamp(ImGui::GetScrollY()-ImGui::GetIO().MouseWheel*ImGui::GetTextLineHeight()*3,0.0f,ImGui::GetScrollMaxY()));
        popup.Min=ImGui::GetWindowPos();const auto size=ImGui::GetWindowSize();popup.Max={popup.Min.x+size.x,popup.Min.y+size.y};
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+content);ImGui::TextWrapped("%s",title.c_str());
        if(!category.empty())ImGui::TextDisabled("%s",category.c_str());
        if(power){const auto pos=ImGui::GetCursorScreenPos();const float size=theme.Y(28);auto badge=row;badge["PowerLevel"]=*power;badge["Masterwork"]=false;DrawItemBadges(badge,pos,{pos.x+size,pos.y+size},theme);ImGui::Dummy({size,size});ImGui::SameLine();ImGui::Text("Power level %.0f",*power);}
        if(ItemBadges::Masterwork(row))ImGui::TextColored(theme.accentHover,"Masterwork");
        if(!description.empty()){ImGui::Separator();ImGui::TextWrapped("%s",description.c_str());}
        if(!facts.empty()){ImGui::Separator();if(ImGui::BeginTable("##native-item-facts",2,ImGuiTableFlags_SizingFixedFit,ImVec2(content,0))){
            ImGui::TableSetupColumn("Property",ImGuiTableColumnFlags_WidthFixed,labelColumn);ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthFixed,valueColumn);
            for(const auto& fact:facts){ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::PushStyleColor(ImGuiCol_Text,theme.textDisabled);ImGui::PushTextWrapPos(0);ImGui::TextWrapped("%s",fact.Label.c_str());ImGui::PopTextWrapPos();ImGui::PopStyleColor();ImGui::TableNextColumn();ImGui::PushTextWrapPos(0);ImGui::TextWrapped("%s",fact.Value.c_str());ImGui::PopTextWrapPos();}ImGui::EndTable();}}
        if(!entry)ImGui::TextWrapped("%s",SpawnToolRequests::IsAvailable()?"Loading native details...":"Load a world for native details.");else if(!entry->Error.empty())ImGui::TextWrapped("%s",entry->Error.c_str());
        if(theme.verboseTooltips){ImGui::Separator();ImGui::TextWrapped("%s",popup.Path.c_str());}
        if(!popup.Fits)ImGui::TextWrapped("Item path exceeds the supported field size.");ImGui::PopTextWrapPos();
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::TooltipScroll=ImGui::GetScrollY();HelpyPreviewHooks::TooltipScrollMax=ImGui::GetScrollMaxY();HelpyPreviewHooks::TooltipPath=popup.Path;HelpyPreviewHooks::TooltipFacts=facts.size();HelpyPreviewHooks::Tooltip={popup.Min.x,popup.Min.y,popup.Max.x,popup.Max.y};
#endif
    }ImGui::End();ImGui::PopStyleVar();
}
inline void ContextMenu(const json& catalog,const json& row,const std::string& path,const std::string& name,char* selected,size_t selectedCapacity,State& state,Result& out,bool allowMulti,bool persistCart,bool allowGive=true) {
    if(!ImGui::BeginPopupContextItem("##item-actions"))return;
    const size_t selectedCount=state.SelectedPaths.size();const bool multi=selectedCount>1&&IsSelected(state,path);
    ImGui::TextUnformatted(name.c_str());ImGui::TextDisabled("Max stack: %d",MaxStack(row));
    const auto origin=ItemSource::Row(row);ImGui::TextDisabled("Source: %s%s%s",ItemSource::Label(origin.Group),origin.Name.empty()?"":" / ",origin.Name.c_str());
    if(multi)ImGui::TextDisabled("Actions apply to all %zu selected items.",selectedCount);ImGui::Separator();
    if(ImGui::MenuItem("Show item details"))PinDetails(row);
    const auto family=ItemFamily::Key(name);
    if(ImGui::MenuItem("View family",nullptr,false,!family.empty()))state.PendingFamilyKey=family;
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::ViewFamily=HelpyPreviewHooks::LastRect();
#endif
    if(ImGui::MenuItem(multi?"Add selected to cart":"Add to cart",nullptr,false,!SharedCart.Delivery())) {
        if(persistCart){if(multi){for(const auto& target:state.SelectedPaths)if(const auto* item=Find(catalog,target.c_str()))AddToCart(*item);}else AddToCart(row);}
    }
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::AddCart=HelpyPreviewHooks::LastRect();
#endif
    ImGui::Separator();
    if(!allowGive){if(ImGui::MenuItem("Reload item details",nullptr,false,ItemDetails::Pending==0))ItemDetails::Reload(path);ImGui::EndPopup();return;}
    const std::string giveStackLabel=multi?("Give full stack ("+std::to_string(selectedCount)+" selected)"):"Give full stack";
    const std::string giveOneLabel=multi?("Give one ("+std::to_string(selectedCount)+" selected)"):"Give one";
    if(ImGui::MenuItem(giveStackLabel.c_str())){out.RequestedAction=ContextAction::GiveStack;out.ActionPath=path;out.ActionCount=MaxStack(row);out.ActionItems=BuildActionItems(catalog,state,path,true);}
    if(ImGui::MenuItem(giveOneLabel.c_str())){out.RequestedAction=ContextAction::GiveOne;out.ActionPath=path;out.ActionCount=1;out.ActionItems=BuildActionItems(catalog,state,path,false);}
    ImGui::Separator();
    if(ImGui::MenuItem("Reload item details",nullptr,false,ItemDetails::Pending==0))ItemDetails::Reload(path);
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
inline void DrawItemBadges(const json& row,const ImVec2& min,const ImVec2& max,const HelpyImGuiTheme::Theme& theme,float badgeSize) {
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
    if(ItemBadges::Masterwork(row)&&x+ImGui::CalcTextSize("MW").x<=max.x)
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
    draw->PushClipRect(min,max,true);draw->AddText(ImVec2(min.x,y),ImGui::GetColorU32(theme.text),first.c_str());
    if(!second.empty())draw->AddText(ImVec2(min.x,y+line),ImGui::GetColorU32(theme.text),second.c_str());draw->PopClipRect();
}
// All item surfaces (browser, recipe nodes, creation slots, cart) share this
// renderer. Compact cards keep the same artwork, title, badge and border rules.
inline void DrawPlacard(const json& row,const ImVec2& min,const ImVec2& max,bool selected=false,bool hovered=false) {
    const auto& theme=HelpyImGuiTheme::Current;auto* draw=ImGui::GetWindowDrawList();
    const auto path=ItemPresentation::Text(row,"Path");const auto* details=ItemDetails::Find(path);
    const auto name=ItemPresentation::Name(row);
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
    draw->AddRect(min,max,ImGui::GetColorU32(HelpyImGuiTheme::Mix(border,theme.accent,std::max(selected?0.45f:0.0f,fade*0.75f))),theme.X(theme.childRounding),0,theme.X(1.0f));
    const float extent=std::max(1.0f,std::min({theme.Y(theme.gridIconSize),width-theme.X(20),height-footer-theme.Y(12)}));
    const ImVec2 artMin(compact?min.x+theme.X(6):min.x+(width-extent)*0.5f,min.y+(height-footer-extent)*0.5f),artMax(artMin.x+extent,artMin.y+extent);
    if(icon.empty()||!ItemIconCache::Draw(icon,artMin,artMax,true))DrawPlaceholder(artMin,artMax,theme);
    if(compact){
        const bool hasPower=ItemBadges::Power(row).has_value()||(details&&ItemBadges::Power(details->Native).has_value());
        const float sourceWidth=SourceTagWidth(row,theme.X(95),theme.Y(20));const float sourceGap=sourceWidth>0?theme.X(4):0;
        const float reserve=(hasPower?theme.X(32):0)+sourceWidth+sourceGap;
        PlacardTitle(name,ImVec2(artMax.x+theme.X(9),min.y+theme.Y(4)),ImVec2(max.x-theme.X(8)-reserve,max.y-theme.Y(4)),titleTheme);
        if(hasPower)DrawItemBadges(row,ImVec2(max.x-theme.X(30)-sourceWidth-sourceGap,min.y+theme.Y(5)),max,theme,22);
    }else{
        const float top=max.y-footer;draw->AddLine(ImVec2(min.x+theme.X(8),top),ImVec2(max.x-theme.X(8),top),ImGui::GetColorU32(theme.border));
        PlacardTitle(name,ImVec2(min.x+theme.X(9),top+theme.Y(2)),ImVec2(max.x-theme.X(9),max.y-theme.Y(3)),titleTheme);
        DrawItemBadges(row,ImVec2(min.x+theme.X(6),min.y+theme.Y(4)),max,theme);
    }
    const float occupied=theme.X(ItemBadges::Masterwork(row)?64:38);
    DrawSourceBadge(row,{max.x-theme.X(5),min.y+theme.Y(4)},compact?theme.X(95):std::max(theme.Y(20),width-occupied-theme.X(12)),theme.Y(20));
}
inline void DrawLoadingSpinner(const HelpyImGuiTheme::Theme& theme,const char* label="Loading items...") {
    auto* draw=ImGui::GetWindowDrawList();const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();const float radius=theme.Y(theme.loadingSpinnerRadius),thickness=theme.X(theme.loadingSpinnerThickness);
    const ImVec2 center(pos.x+size.x*0.5f,pos.y+size.y*0.44f);const float start=static_cast<float>(ImGui::GetTime()*4.8);const float end=start+4.65f;
    draw->PathClear();draw->PathArcTo(center,radius,start,end,28);draw->PathStroke(ImGui::GetColorU32(theme.accentHover),0,thickness);
    const auto ts=ImGui::CalcTextSize(label);draw->AddText(ImVec2(center.x-ts.x*0.5f,center.y+radius+theme.Y(10.0f)),ImGui::GetColorU32(theme.textDisabled),label);
}
inline Result Render(const char* id,const json& catalog,char* filter,size_t filterCapacity,char* selected,size_t selectedCapacity,State& state,uint64_t revision,bool allowMulti=true,bool quickActions=false,bool fillSpace=false,bool uniformCards=false) {
    ItemDetails::Poll();Result out;ImGui::PushID(id);LoadViewPreference(state);SyncLegacySelection(state,selected);
    if(!filter||!filterCapacity||!selected||!selectedCapacity){ImGui::TextDisabled("Item picker buffer unavailable.");ImGui::PopID();return out;}
    if(!state.PendingFamilyKey.empty()){
        state.FamilyKey=std::move(state.PendingFamilyKey);state.PendingFamilyKey.clear();state.Query.clear();state.SearchInput.clear();filter[0]=0;
        state.ActiveCategory="All";HelpyImGuiTheme::Current.itemCategory="All";state.PageIndex=0;state.ResetScroll=true;state.CatalogRevision=0;state.Matches.clear();
        LazyCatalog::Search(0,BackendQuery(state));
    }
    const auto sourcePreference=ItemSource::Decode(HelpyImGuiTheme::Current.itemSource);
    if(state.SourceFilter!=sourcePreference){state.SourceFilter=sourcePreference;state.CatalogRevision=0;state.Matches.clear();state.PageIndex=0;state.ResetScroll=true;LazyCatalog::Search(0,BackendQuery(state));}
    const auto& theme=HelpyImGuiTheme::Current;if(state.ActiveCategory!=theme.itemCategory){state.ActiveCategory=theme.itemCategory;state.Matches.clear();state.CatalogRevision=0;state.PageIndex=0;state.ResetScroll=true;}
    if(state.PageSize!=theme.itemPageSize){state.PageIndex=ItemPaging::Resize(state.PageIndex,state.PageSize,theme.itemPageSize);state.PageSize=ItemPaging::Size(theme.itemPageSize);state.ResetScroll=true;}
    if(state.CatalogRevision!=revision)Rebuild(catalog,state,revision);out.MatchCount=state.Matches.size();
    const float spacing=ImGui::GetStyle().ItemSpacing.x;
    const float width=ImGui::GetContentRegionAvail().x;
    const float categoryWidth=theme.X(width<theme.X(600)?105:142),sourceWidth=theme.X(width<theme.X(600)?100:132);
    const float controls=categoryWidth+sourceWidth+ImGui::CalcTextSize("Search").x+ImGui::CalcTextSize("View").x+ImGui::GetStyle().FramePadding.x*4+spacing*4;
    ImGui::SetNextItemWidth(std::max(theme.X(90.0f),width-controls));
    const bool enterSubmitted=ImGui::InputTextWithHint("##Search items","Search items...",filter,filterCapacity,ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();const bool searchSubmitted=HelpyImGuiTheme::SoftButton("Search");
    if(state.SearchInput!=filter){state.SearchInput=filter;state.SearchDue=ImGui::GetTime()+0.4;}
    if(!SearchAsYouType)state.SearchDue=0;
    if(enterSubmitted||searchSubmitted||(state.SearchDue>0&&ImGui::GetTime()>=state.SearchDue)){
        state.SearchDue=0;state.Query=filter;state.PageIndex=0;state.ResetScroll=true;
        LazyCatalog::Search(0,BackendQuery(state));state.Matches.clear();state.CatalogRevision=0;
    }
    static constexpr const char* categories[]={"All","Weapons","Armor","Consumables","Ammo","Resources","Tools","Building","Quest","Misc"};
    const auto inlineControl=[&](float next){if(ImGui::GetItemRectMax().x+spacing+next<=ImGui::GetWindowPos().x+ImGui::GetWindowContentRegionMax().x)ImGui::SameLine();};
    inlineControl(categoryWidth);ImGui::SetNextItemWidth(categoryWidth);
    if(ImGui::BeginCombo("##item-category",state.ActiveCategory=="All"?"All categories":state.ActiveCategory.c_str())){
        for(const char* category:categories)if(ImGui::Selectable(category,state.ActiveCategory==category)){
            state.ActiveCategory=category;HelpyImGuiTheme::Current.itemCategory=category;HelpyImGuiTheme::SaveCurrent();
            state.PageIndex=0;state.ResetScroll=true;LazyCatalog::Search(0,BackendQuery(state));state.Matches.clear();state.CatalogRevision=0;
        }ImGui::EndCombo();
    }
    inlineControl(sourceWidth);ImGui::SetNextItemWidth(sourceWidth);
    if(ImGui::BeginCombo("##item-source",ItemSource::Label(state.SourceFilter))){
        for(const auto kind:{ItemSource::Kind::All,ItemSource::Kind::Vanilla,ItemSource::Kind::Expansion,ItemSource::Kind::Modded,ItemSource::Kind::Unknown}){
            if(ImGui::Selectable(ItemSource::Label(kind),state.SourceFilter==kind)){
                state.SourceFilter=kind;HelpyImGuiTheme::Current.itemSource=ItemSource::Token(kind);HelpyImGuiTheme::SaveCurrent();
                state.PageIndex=0;state.ResetScroll=true;LazyCatalog::Search(0,BackendQuery(state));state.Matches.clear();state.CatalogRevision=0;
            }
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::SourceOptions[ItemSource::Token(kind)]=HelpyPreviewHooks::LastRect();
#endif
        }
        ImGui::EndCombo();
    }
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::SourceFilter=HelpyPreviewHooks::LastRect();
#endif
    inlineControl(theme.X(62));LayoutControls();state.GridView=theme.itemGridView;
    if(!state.FamilyKey.empty()){
        auto label=state.FamilyKey;std::replace(label.begin(),label.end(),'_',' ');for(size_t i=0;i<label.size();++i)if(i==0||label[i-1]==' ')label[i]=static_cast<char>(std::toupper(static_cast<unsigned char>(label[i])));
        ImGui::TextWrapped("Family: %s",label.c_str());ImGui::SameLine();
        if(ImGui::SmallButton("Clear family")){state.FamilyKey.clear();state.PageIndex=0;state.ResetScroll=true;state.CatalogRevision=0;state.Matches.clear();LazyCatalog::Search(0,BackendQuery(state));}
    }
    if(LazyCatalog::For(0).Started&&LazyCatalog::For(0).Query!=BackendQuery(state)){LazyCatalog::Search(0,BackendQuery(state));state.Matches.clear();state.CatalogRevision=0;}
    const auto& lazy=LazyCatalog::For(0);
    if(!lazy.LastError.empty()&&!lazy.Pending){
        ImGui::PushStyleColor(ImGuiCol_Text,theme.warning);ImGui::TextWrapped("%s",lazy.LastError.c_str());ImGui::PopStyleColor();
        if(ImGui::SmallButton("Retry loading items"))LazyCatalog::Retry(0);
    }
    if(!lazy.HasMore&&!lazy.Pending&&lazy.Started){
        const int last=std::max(0,static_cast<int>((state.Matches.size()+state.PageSize-1)/state.PageSize)-1);
        if(state.PageIndex>last){state.PageIndex=last;state.ResetScroll=true;}
    }
    const auto page=ItemPaging::Slice(state.Matches.size(),state.PageIndex,state.PageSize,lazy.HasMore);
    LazyCatalog::FillTo(0,(state.PageIndex+1)*state.PageSize);
    const float availableLogical=ImGui::GetContentRegionAvail().y/std::max(0.01f,theme.Scale()*theme.density);
    const float maxPane=std::max(96.0f,availableLogical-(fillSpace?56.0f:quickActions?ItemPaging::CartHeight(SharedCart.Entries().size(),theme.advancedMode)+80.0f:100.0f));
    const float gapLogical=ImGui::GetStyle().ItemSpacing.y/std::max(0.01f,theme.Scale()*theme.density);
    const float desiredPane=fillSpace?maxPane:state.GridView&&theme.itemGridFitRows
        ?ItemBayLayout::RowsHeight(theme.itemGridRows,theme.gridCardHeight,gapLogical,theme.contentPaddingY)+2.0f
        :theme.itemResultHeight;
    const float minPane=state.GridView&&theme.itemGridFitRows?std::min(desiredPane,quickActions?140.0f:180.0f):(quickActions?140.0f:180.0f);
    state.ResultPaneHeightLogical=ItemBayLayout::PaneHeight(desiredPane,maxPane,minPane);
    HelpyImGuiTheme::BeginScroll("Rows",ImVec2(0,theme.Y(state.ResultPaneHeightLogical)),true);
    if(state.ResetScroll){ImGui::SetScrollY(0);state.ResetScroll=false;}
    const auto resultPos=ImGui::GetWindowPos(),resultSize=ImGui::GetWindowSize();
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::Browser=HelpyPreviewHooks::WindowRect();
#endif
    if(quickActions && ImGui::BeginDragDropTargetCustom(ImRect(resultPos,ImVec2(resultPos.x+resultSize.x,resultPos.y+resultSize.y)),ImGui::GetID("return-to-items"))){
        if(const auto* payload=ImGui::AcceptDragDropPayload("RS_CART_PATH"))if(payload->DataSize>0 && payload->DataSize<=4097 && static_cast<const char*>(payload->Data)[payload->DataSize-1]=='\0')RemoveFromCart(static_cast<const char*>(payload->Data));
        ImGui::EndDragDropTarget();
    }
    const bool populationWait=lazy.Pending&&state.Matches.empty();
    if(populationWait)DrawLoadingSpinner(theme,lazy.PreparingAlphabetical?"Preparing A-Z list...":"Loading items...");
    else if(catalog.contains("Items")&&catalog["Items"].is_array()) {
        const auto& items=catalog["Items"];const int total=static_cast<int>(page.Count);
        if(!state.GridView) {
            const float iconSize=theme.Y(theme.listIconSize);const float itemHeight=std::max(ImGui::GetTextLineHeight()+theme.Y(4.0f),iconSize);const float rowHeight=itemHeight+ImGui::GetStyle().ItemSpacing.y;
            ImGuiListClipper clipper;clipper.Begin(total,rowHeight);
            while(clipper.Step()) {
                const int visibleFirst=clipper.DisplayStart,visibleLast=clipper.DisplayEnd;
                for(int rowIndex=visibleFirst;rowIndex<visibleLast;++rowIndex) {
                    const auto index=state.Matches[page.First+static_cast<size_t>(rowIndex)];if(index>=items.size())continue;
                    const auto& row=items[index];const auto path=row.value("Path",std::string{}),name=ItemPresentation::Name(row),iconPath=IconForRow(row);const bool fits=path.size()<selectedCapacity,selectedThis=IsSelected(state,path);
                    ImGui::PushID(rowIndex);ImGui::PushID(path.c_str());const float rowWidth=ImGui::GetContentRegionAvail().x;ImGui::BeginDisabled(!fits);const auto rowPos=ImGui::GetCursorScreenPos();
                    if(ImGui::Selectable("##item",selectedThis,0,ImVec2(rowWidth,itemHeight))&&fits){if(ApplySelection(state,path,selected,selectedCapacity,ImGui::GetIO().KeyShift,allowMulti)){out.Changed=true;}}
                    if(fits)DragItem(row);
                    ImGui::EndDisabled();const float pad=theme.X(4.0f);const ImVec2 iconMin(rowPos.x+pad,rowPos.y+(itemHeight-iconSize)*0.5f),iconMax(iconMin.x+iconSize,iconMin.y+iconSize);
                    if(!iconPath.empty())HelpyIconTrace::Event("render-row",iconPath,"name='"+name+"' row="+std::to_string(rowIndex));
                    if(iconPath.empty()||!ItemIconCache::Draw(iconPath,iconMin,iconMax,true))DrawPlaceholder(iconMin,iconMax,theme);
                    const float sourceWidth=SourceTagWidth(row,theme.X(112),theme.Y(20));
                    const float powerWidth=(ItemBadges::Power(row)||ItemBadges::Masterwork(row))?theme.X(130.0f):0.0f;
                    const float badgeWidth=powerWidth+sourceWidth;
                    const float textX=iconMax.x+theme.X(9.0f);const auto label=Ellipsize(name,std::max(theme.X(30.0f),rowPos.x+rowWidth-badgeWidth-textX));
                    ImGui::GetWindowDrawList()->AddText(ImVec2(textX,rowPos.y+(itemHeight-ImGui::GetTextLineHeight())*0.5f),ImGui::GetColorU32(fits?(ItemBadges::Masterwork(row)?theme.accentHover:theme.text):theme.textDisabled),label.c_str());
                    if(powerWidth>0)DrawItemBadges(row,ImVec2(rowPos.x+rowWidth-badgeWidth,rowPos.y+(itemHeight-ImGui::GetTextLineHeight())*0.5f),ImVec2(rowPos.x+rowWidth-sourceWidth,rowPos.y+itemHeight),theme);
                    DrawSourceBadge(row,{rowPos.x+rowWidth-theme.X(3),rowPos.y+(itemHeight-theme.Y(20))*.5f},theme.X(112),theme.Y(20));
                    ContextMenu(catalog,row,path,name,selected,selectedCapacity,state,out,allowMulti,true,quickActions);if(HelpyImGuiTheme::TooltipReady(std::string("item:")+path+":"+std::to_string(rowIndex),ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)))VerboseTooltip(row,name,path,allowMulti,fits);
                    ImGui::PopID();ImGui::PopID();
                }
            }
        } else {
            const float spacing=ImGui::GetStyle().ItemSpacing.x,minCardWidth=theme.X(theme.gridMinCardWidth),avail=std::max(1.0f,ImGui::GetContentRegionAvail().x-theme.X(2.0f));
            const int columns=ItemBayLayout::Columns(avail,minCardWidth,spacing,theme.itemGridColumns);
            const float fittedHeight=(ImGui::GetContentRegionAvail().y-ImGui::GetStyle().ItemSpacing.y*(theme.itemGridRows-1))/std::max(1,theme.itemGridRows);
            const float cardWidth=std::max(1.0f,(avail-spacing*(columns-1))/columns),cardHeight=!uniformCards&&theme.itemGridFitRows?std::clamp(fittedHeight,theme.Y(72.0f),theme.Y(theme.gridCardHeight)):theme.Y(theme.gridCardHeight),
                iconSize=std::max(1.0f,std::min({theme.Y(theme.gridIconSize),cardWidth-theme.X(14.0f),cardHeight-theme.Y(44.0f)})),gridRowHeight=cardHeight+ImGui::GetStyle().ItemSpacing.y;
            const int gridRows=(total+columns-1)/columns;ImGuiListClipper clipper;clipper.Begin(gridRows,gridRowHeight);
            while(clipper.Step()) {
                const int visibleFirstRow=clipper.DisplayStart,visibleLastRow=clipper.DisplayEnd;const int visibleFirst=visibleFirstRow*columns,visibleLast=std::min(total,visibleLastRow*columns);
                for(int gridRow=visibleFirstRow;gridRow<visibleLastRow;++gridRow) {
                    for(int col=0;col<columns;++col) {
                        const int rowIndex=gridRow*columns+col;if(rowIndex>=total)break;const auto index=state.Matches[page.First+static_cast<size_t>(rowIndex)];if(index>=items.size())continue;
                        const auto& row=items[index];const auto path=row.value("Path",std::string{}),name=ItemPresentation::Name(row),iconPath=IconForRow(row);const bool fits=path.size()<selectedCapacity,selectedThis=IsSelected(state,path);
                        ImGui::PushID(rowIndex);ImGui::PushID(path.c_str());const auto cardMin=ImGui::GetCursorScreenPos();ImGui::BeginDisabled(!fits);const bool clicked=ImGui::InvisibleButton("##card",ImVec2(cardWidth,cardHeight));ImGui::EndDisabled();const auto cardMax=ImVec2(cardMin.x+cardWidth,cardMin.y+cardHeight);auto* draw=ImGui::GetWindowDrawList();
                        DrawPlacard(row,cardMin,cardMax,selectedThis,ImGui::IsItemHovered());
                        if(clicked&&fits){if(ApplySelection(state,path,selected,selectedCapacity,ImGui::GetIO().KeyShift,allowMulti)){out.Changed=true;}}
                        if(fits)DragItem(row);
                        ContextMenu(catalog,row,path,name,selected,selectedCapacity,state,out,allowMulti,true,quickActions);if(HelpyImGuiTheme::TooltipReady(std::string("item:")+path+":"+std::to_string(rowIndex),ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)))VerboseTooltip(row,name,path,allowMulti,fits);
                        ImGui::PopID();ImGui::PopID();if(col<columns-1&&rowIndex+1<total)ImGui::SameLine();
                    }
                }
            }
        }
    }
    HelpyImGuiTheme::EndScroll(theme);
    if(!fillSpace){ImGui::PushID("results-bottom-splitter");const float splitH=std::max(theme.Y(theme.splitterWidth),theme.Y(5.0f));
    ImGui::InvisibleButton("##split",ImVec2(ImGui::GetContentRegionAvail().x,splitH));const bool splitHover=ImGui::IsItemHovered(),splitActive=ImGui::IsItemActive();
    if(splitHover||splitActive)ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    if(splitActive&&ImGui::GetIO().MouseDelta.y!=0.0f){state.ResultPaneHeightLogical+=ImGui::GetIO().MouseDelta.y/std::max(0.01f,theme.Scale()*theme.density);state.ResultPaneHeightLogical=ItemBayLayout::PaneHeight(state.ResultPaneHeightLogical,maxPane,minPane);HelpyImGuiTheme::Current.itemResultHeight=state.ResultPaneHeightLogical;if(state.GridView)HelpyImGuiTheme::Current.itemGridFitRows=false;}static bool paneDirty=false;if(splitActive)paneDirty=true;if(paneDirty&&!ImGui::IsMouseDown(ImGuiMouseButton_Left)){HelpyImGuiTheme::SaveCurrent();paneDirty=false;}
    const auto splitMin=ImGui::GetItemRectMin(),splitMax=ImGui::GetItemRectMax();if(splitHover||splitActive)ImGui::GetWindowDrawList()->AddLine(ImVec2(splitMin.x,(splitMin.y+splitMax.y)*0.5f),ImVec2(splitMax.x,(splitMin.y+splitMax.y)*0.5f),ImGui::GetColorU32(theme.accentHover),theme.X(1.5f));
    ImGui::PopID();
    }
    PageControls(state,state.Matches.size(),lazy.HasMore&&lazy.LastError.empty(),lazy.Pending);
    if(lazy.LastError.empty()&&state.Matches.empty()&&!populationWait&&lazy.Started)ImGui::TextDisabled("No matching items in the loaded runtime window.");
    if(state.SelectedRevision!=revision||state.SelectedPath!=selected) {
        state.SelectedRevision=revision;state.SelectedPath=selected;state.SelectedIndex=static_cast<size_t>(-1);
        if(catalog.contains("Items")&&catalog["Items"].is_array()){const auto& items=catalog["Items"];for(size_t i=0;i<items.size();++i)if(items[i].value("Path",std::string{})==selected){state.SelectedIndex=i;break;}}
    }
    if(catalog.contains("Items")&&catalog["Items"].is_array()&&state.SelectedIndex<catalog["Items"].size())out.SelectedRow=&catalog["Items"][state.SelectedIndex];
    out.SelectedTitle=out.SelectedRow?ItemPresentation::Name(*out.SelectedRow):selected;out.SelectedPaths=state.SelectedPaths;out.SelectedCount=state.SelectedPaths.size();
    if(!quickActions&&out.SelectedCount>1)ImGui::TextDisabled("%zu selected",out.SelectedCount);
    else if(!quickActions)ImGui::TextWrapped("Selected: %s",out.SelectedTitle.empty()?"None":out.SelectedTitle.c_str());
    if(!quickActions&&allowMulti&&theme.advancedMode)ImGui::TextDisabled("Shift+click to select multiple items.");ImGui::PopID();return out;
}
struct SharedBay {
    State Picker;
    char Filter[257]{},Selected[1025]{};
    json Catalog=json::object();uint64_t Generation=0;
    json DetailsRow=json::object();bool DetailsExpanded=true,DetailsOpenRequested=false;
};
inline SharedBay Bay;
inline bool DockedBay=false;
inline bool DetachedBay=false,RenderingDetachedBay=false;
inline std::function<void(const Result&)> ItemAction;
inline void SelectShared(const std::string& path){
    if(path.size()>=sizeof(Bay.Selected))return;
    const auto generation=SpawnToolRequests::CurrentGeneration();if(Bay.Generation!=generation){Bay.Generation=generation;Bay.Catalog=json::object();Bay.DetailsRow=json::object();}
    std::snprintf(Bay.Selected,sizeof(Bay.Selected),"%s",path.c_str());Bay.Picker.SelectedPaths={path};Bay.Picker.SelectedPath=path;Bay.Picker.SelectedRevision=0;
}
inline void PinDetails(const json& row){
    const auto path=ItemPresentation::Text(row,"Path");if(path.empty()||path.size()>=sizeof(Bay.Selected))return;
    SelectShared(path);Bay.DetailsRow=row;Bay.DetailsExpanded=true;Bay.DetailsOpenRequested=true;ItemDetails::Request(path);
}
inline void RenderSelectedDetails(){
    if(Bay.DetailsOpenRequested){ImGui::SetNextItemOpen(true);Bay.DetailsOpenRequested=false;}
    Bay.DetailsExpanded=ImGui::CollapsingHeader("Selected item details",ImGuiTreeNodeFlags_DefaultOpen);
    if(!Bay.DetailsExpanded)return;
    if(!*Bay.Selected){ImGui::TextDisabled("Select an item to inspect its description and stats.");return;}
    ItemDetails::Request(Bay.Selected);const auto* details=ItemDetails::Find(Bay.Selected);
    const auto& row=Bay.DetailsRow;const auto& native=details?details->Native:row;
    if(ImGui::BeginChild("##bay-selected-details",{0,0},false)){
        auto name=ItemPresentation::CleanName(ItemPresentation::Text(native,"Name"));if(name.empty())name=ItemPresentation::Name(row);
        const auto a=ImGui::GetCursorScreenPos();const float size=HelpyImGuiTheme::Current.Y(36);
        ItemIconCache::Draw(IconForRow(row),a,{a.x+size,a.y+size},true);ImGui::Dummy({size,size});ImGui::SameLine();ImGui::TextWrapped("%s",name.c_str());
        const auto origin=ItemSource::Row(row);ImGui::TextDisabled("Source: %s%s%s",ItemSource::Label(origin.Group),origin.Name.empty()?"":" / ",origin.Name.c_str());
        auto power=ItemBadges::Power(native);if(!power)power=ItemBadges::Power(row);
        if(power){const auto pos=ImGui::GetCursorScreenPos();const float badgeSize=HelpyImGuiTheme::Current.Y(28);auto badge=row;badge["PowerLevel"]=*power;DrawItemBadges(badge,pos,{pos.x+badgeSize,pos.y+badgeSize},HelpyImGuiTheme::Current);ImGui::Dummy({badgeSize,badgeSize});ImGui::SameLine();ImGui::Text("Power level %.0f",*power);}
        if(ItemBadges::Masterwork(row))ImGui::TextUnformatted("Masterwork");
        auto description=ItemPresentation::Description(native);if(description.empty())description=ItemPresentation::Description(row);
        if(!description.empty())ImGui::TextWrapped("%s",description.c_str());
        auto facts=ItemPresentation::Facts(native);
        const auto source=ItemSource::Row(row);
        if(source.Group==ItemSource::Kind::Modded)facts.insert(facts.begin(),{"Mod",source.Name.empty()?"Mod source unavailable":source.Name});
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::BayDetailsFacts=facts.size();
#endif
        if(!facts.empty()&&ImGui::BeginTable("##bay-item-facts",2,ImGuiTableFlags_SizingStretchProp)){
            for(const auto& fact:facts){ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextWrapped("%s",fact.Label.c_str());ImGui::TableNextColumn();ImGui::TextWrapped("%s",fact.Value.c_str());}ImGui::EndTable();
        }
        if(!details)ImGui::TextDisabled("Reading native details...");else if(!details->Error.empty())ImGui::TextWrapped("%s",details->Error.c_str());
        if(ImGui::SmallButton("Reload details"))ItemDetails::Reload(Bay.Selected);ImGui::SameLine();if(ImGui::SmallButton("Copy item path"))ImGui::SetClipboardText(Bay.Selected);
    }ImGui::EndChild();
}
inline Result RenderShared(const json& catalog,uint64_t revision,bool fillSpace=true){
    const auto generation=SpawnToolRequests::CurrentGeneration();if(Bay.Generation!=generation){Bay.Generation=generation;Bay.Catalog=json::object();Bay.DetailsRow=json::object();Bay.Selected[0]=0;Bay.Picker.SelectedPaths.clear();Bay.Picker.SelectedPath.clear();}
    const auto previous=Bay.Catalog.value("Items",json::array());Bay.Catalog=catalog;
    if(!Bay.Catalog.contains("Items")&&!previous.empty())Bay.Catalog["Items"]=previous;
    // Spawn pages request definition windows; the bay still owns the same item
    // window as Item Service, rather than treating resource definitions as items.
    LazyCatalog::Overlay(Bay.Catalog,0);
    if(!RenderingDetachedBay){
        if(DetachedBay){ImGui::TextDisabled("Item Bay is in its separate window.");ImGui::SameLine();if(ImGui::SmallButton("Dock back"))DetachedBay=false;}
        else if(!DockedBay&&ImGui::SmallButton("Detach Item Bay"))DetachedBay=true;
    }
    if((DockedBay||DetachedBay)&&!RenderingDetachedBay){Result result;result.SelectedPaths=Bay.Picker.SelectedPaths;result.SelectedCount=result.SelectedPaths.size();result.SelectedTitle=Title(Bay.Catalog,Bay.Selected);result.SelectedRow=Find(Bay.Catalog,Bay.Selected);return result;}
    if(!RenderingDetachedBay&&ImGui::GetContentRegionAvail().x>HelpyImGuiTheme::Current.X(300))ImGui::SameLine();CardScaleControl();
    const float available=ImGui::GetContentRegionAvail().y;const auto& theme=HelpyImGuiTheme::Current;
    const float splitter=theme.Y(8)+ImGui::GetStyle().ItemSpacing.y;
    const float browse=ItemBayLayout::PaneHeight(Bay.DetailsExpanded?available*theme.itemBayBrowseRatio:available-ImGui::GetFrameHeightWithSpacing()-splitter,std::max(1.0f,available-theme.Y(45)-splitter),theme.Y(100));
    Result result;
    if(ImGui::BeginChild("##bay-browse-with-details",{0,browse},false))
        result=Render("shared-item-bay",Bay.Catalog,Bay.Filter,sizeof(Bay.Filter),Bay.Selected,sizeof(Bay.Selected),Bay.Picker,revision,true,true,fillSpace,true);
    ImGui::EndChild();
    ResizeDivider("item-bay-details",true,HelpyImGuiTheme::Current.itemBayBrowseRatio,.25f,.90f,1.0f/std::max(1.0f,available));
    if(const auto* selected=Find(Bay.Catalog,Bay.Selected))Bay.DetailsRow=*selected;
    else if(ItemPresentation::Text(Bay.DetailsRow,"Path")!=Bay.Selected)Bay.DetailsRow={{"Path",Bay.Selected}};
    RenderSelectedDetails();
    if(result.RequestedAction!=ContextAction::None&&ItemAction)ItemAction(result);
    return result;
}
}
