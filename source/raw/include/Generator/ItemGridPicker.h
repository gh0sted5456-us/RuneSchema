#pragma once
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/ItemIconCache.h"
#include "Generator/ToolRequest.h"
#include "Generator/LazyCatalog.h"
#include "Core/ConfigFiles.h"
#include "Core/ItemCart.h"
#include "Core/ItemTransfer.h"
#include "Core/ItemBadges.h"
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
struct State {
    std::string Query,SelectedPath,ActiveCategory{"All"};
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
inline std::string CartStatus;
inline bool AddToCart(const json& row){const bool added=SharedCart.Add(row);if(added)CartStatus.clear();else CartStatus=SharedCart.Delivery()?"Wait for delivery to complete before changing the cart.":"Cart accepts up to 64 distinct items with canonical asset paths.";return added;}
inline void RemoveFromCart(const std::string& path){SharedCart.Remove(path);}
inline void ClearSharedCart(){SharedCart.Clear();CartStatus.clear();}
inline void DragItem(const json& row){
    if(!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))return;
    try{const auto data=ItemTransfer::Compact(row).dump();ImGui::SetDragDropPayload("RS_ITEM_ROW",data.c_str(),data.size()+1);}catch(const std::exception&){}
    ImGui::TextUnformatted(row.value("Name",row.value("Path",std::string{})).c_str());
    ImGui::TextDisabled("Drop into the cart, Transmutate, or recipe inputs/outputs.");ImGui::EndDragDropSource();
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
inline std::string BackendQuery(const State& state){return state.ActiveCategory=="All"?state.Query:"__helpycat:"+Lower(state.ActiveCategory)+(state.Query.empty()?std::string{}:" "+state.Query);}
inline int MaxStack(const json& row){try{const auto it=row.find("MaxStackSize");if(it!=row.end()){if(it->is_number_integer())return std::clamp(it->get<int>(),1,10000);if(it->is_number_unsigned())return std::clamp(static_cast<int>(it->get<unsigned>()),1,10000);if(it->is_number_float())return std::clamp(static_cast<int>(it->get<double>()),1,10000);}}catch(...){}return 1;}
inline const json* Find(const json& catalog,const char* selected) {
    if(!selected||!*selected||!catalog.contains("Items")||!catalog["Items"].is_array())return nullptr;
    for(const auto& row:catalog["Items"])if(row.value("Path",std::string{})==selected)return &row;return nullptr;
}
inline std::string Title(const json& catalog,const std::string& path){const auto* row=Find(catalog,path.c_str());return row?row->value("Name",path):path;}
inline json BuildActionItems(const json& catalog,const State& state,const std::string& clickedPath,bool fullStack) {
    json items=json::array();
    const bool useSelection=state.SelectedPaths.size()>1;
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
        if(state.ActiveCategory!="All"&&row.value("Category",std::string("Misc"))!=state.ActiveCategory)continue;
        if(query.empty()){state.Matches.push_back(i);continue;}
        const auto path=row.value("Path",std::string{}),name=row.value("Name",path);
        if(Lower(name+" "+path+" "+row.value("InternalName",std::string{})+" "+row.value("PersistenceID",std::string{})+" "+row.value("Tags",std::string{})+" "+row.value("Category",std::string{})).find(query)!=std::string::npos)
            state.Matches.push_back(i);
    }
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
    if(ImGui::Button("View"))ImGui::OpenPopup("##item-bay-layout");
    static bool dirty=false;
    if(ImGui::BeginPopup("##item-bay-layout")) {
        ImGui::TextUnformatted("Item view");ImGui::Separator();
        if(ImGui::RadioButton("Grid",theme.itemGridView)){theme.itemGridView=true;dirty=true;}ImGui::SameLine();
        if(ImGui::RadioButton("List",!theme.itemGridView)){theme.itemGridView=false;dirty=true;}
        ImGui::SeparatorText("Grid layout");ImGui::SetNextItemWidth(theme.X(180.0f));
        if(ImGui::SliderInt("Columns",&theme.itemGridColumns,1,ItemBayLayout::MaxColumns))dirty=true;
        ImGui::SetNextItemWidth(theme.X(180.0f));
        if(ImGui::SliderInt("Rows",&theme.itemGridRows,1,ItemBayLayout::MaxRows)){theme.itemGridFitRows=true;dirty=true;}
        if(ImGui::Checkbox("Size grid by rows",&theme.itemGridFitRows))dirty=true;
        ImGui::TextDisabled("Fits the available window. Page size is below the items.");
        ImGui::TextDisabled("Drag the lower divider for a free-height pane.");
        if(ImGui::Button("Reset to 5 x 4")){theme.itemGridColumns=ItemBayLayout::DefaultColumns;theme.itemGridRows=ItemBayLayout::DefaultRows;theme.itemGridFitRows=true;dirty=true;}
        ImGui::EndPopup();
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
    if(ImGui::Button("Previous")){--state.PageIndex;state.ResetScroll=true;}ImGui::EndDisabled();
    ImGui::SameLine();ImGui::BeginDisabled(!page.Next||pending);
    if(ImGui::Button("Next")){++state.PageIndex;state.ResetScroll=true;}ImGui::EndDisabled();
}
inline void VerboseTooltip(const json& row,const std::string& name,const std::string& path,bool allowMulti,bool fits) {
    ImGui::BeginTooltip();ImGui::TextUnformatted(name.c_str());ImGui::Separator();ImGui::TextUnformatted(path.c_str());
    if(const auto power=ItemBadges::Power(row))ImGui::Text("Power level: %.0f",*power);
    if(ItemBadges::Masterwork(row))ImGui::TextUnformatted("Masterwork");
    if(HelpyImGuiTheme::Current.verboseTooltips){
        const auto field=[&](const char* label,const char* key){const auto v=row.value(key,std::string{});if(!v.empty())ImGui::Text("%s: %s",label,v.c_str());};
        field("Internal","InternalName");field("Persistence ID","PersistenceID");field("Category","Category");field("Tags","Tags");field("Icon","Icon");field("Class","Class");field("Type","Type");ImGui::Text("Max stack: %d",MaxStack(row));
        const auto type=row.find("MasterworkType");if(type!=row.end()&&!type->is_null())ImGui::Text("Native masterwork type: %s",type->dump().c_str());
    }
    if(allowMulti)ImGui::TextDisabled("Shift+click toggles this item in the selection pool.");if(!fits)ImGui::TextUnformatted("Path exceeds the field capacity; it will not be truncated.");ImGui::EndTooltip();
}
inline void ContextMenu(const json& catalog,const json& row,const std::string& path,const std::string& name,char* selected,size_t selectedCapacity,State& state,Result& out,bool allowMulti,bool persistCart) {
    if(!ImGui::BeginPopupContextItem("##item-actions"))return;
    const size_t selectedCount=state.SelectedPaths.size();const bool multi=selectedCount>1;
    ImGui::TextUnformatted(name.c_str());ImGui::TextDisabled("Max stack: %d",MaxStack(row));
    if(multi)ImGui::TextDisabled("Actions apply to all %zu selected items.",selectedCount);ImGui::Separator();
    if(ImGui::MenuItem("Add to cart",nullptr,false,!SharedCart.Delivery())) {if(persistCart)AddToCart(row);}
    ImGui::Separator();
    const std::string giveStackLabel=multi?("Give full stack ("+std::to_string(selectedCount)+" selected)"):"Give full stack";
    const std::string giveOneLabel=multi?("Give One Now ("+std::to_string(selectedCount)+" selected)"):"Give One Now";
    if(ImGui::MenuItem(giveStackLabel.c_str())){out.RequestedAction=ContextAction::GiveStack;out.ActionPath=path;out.ActionCount=MaxStack(row);out.ActionItems=BuildActionItems(catalog,state,path,true);}
    if(ImGui::MenuItem(giveOneLabel.c_str())){out.RequestedAction=ContextAction::GiveOne;out.ActionPath=path;out.ActionCount=1;out.ActionItems=BuildActionItems(catalog,state,path,false);}
    ImGui::Separator();
    const std::string dropStackLabel=multi?("Drop Stack ("+std::to_string(selectedCount)+" selected)"):"Drop Stack";
    const std::string dropOneLabel=multi?("Drop One ("+std::to_string(selectedCount)+" selected)"):"Drop One";
    ImGui::BeginDisabled(true);ImGui::MenuItem(dropStackLabel.c_str());ImGui::MenuItem(dropOneLabel.c_str());ImGui::EndDisabled();
    ImGui::TextDisabled("Drop is safety-gated until native world-pickup ownership/replication is verified. Full-stack payloads resolve each selected item's own MaxStackSize.");
    ImGui::EndPopup();
}
inline std::string Ellipsize(const std::string& text,float maxWidth) {
    if(ImGui::CalcTextSize(text.c_str()).x<=maxWidth)return text;std::string out=text;
    while(out.size()>3&&ImGui::CalcTextSize((out+"...").c_str()).x>maxWidth)out.pop_back();return out+"...";
}
inline void DrawPlaceholder(const ImVec2& iconMin,const ImVec2& iconMax,const HelpyImGuiTheme::Theme& theme) {
    auto* draw=ImGui::GetWindowDrawList();const auto color=ImGui::GetColorU32(theme.icon);draw->AddRect(iconMin,iconMax,color,theme.X(3.0f),0,theme.X(1.2f));
    const auto center=ImVec2((iconMin.x+iconMax.x)*0.5f,(iconMin.y+iconMax.y)*0.5f);draw->AddCircleFilled(center,theme.X(2.2f),color);
}
inline void DrawItemBadges(const json& row,const ImVec2& min,const ImVec2& max,const HelpyImGuiTheme::Theme& theme) {
    auto* draw=ImGui::GetWindowDrawList();float x=min.x;const float size=theme.Y(28.0f);
    if(const auto power=ItemBadges::Power(row)) {
        char label[32]{};std::snprintf(label,sizeof(label),"%.0f",*power);
        if(x+size<=max.x) {
            // WBP_InventoryPowerLevelDisplay overlays the native number on its
            // glyph, rather than drawing a decorative diamond beside a label.
            // Item rows have no native zone comparison, so use NotCompared's
            // neutral glyph and do not fabricate danger colors or item tiers.
            ItemIconCache::Draw(QuickDecorations::PowerLevelBadge,ImVec2(x,min.y),ImVec2(x+size,min.y+size),true);
            const float fontSize=theme.Y(12.0f);auto* font=ImGui::GetFont();
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
inline void DrawLoadingSpinner(const HelpyImGuiTheme::Theme& theme,const char* label="Loading items...") {
    auto* draw=ImGui::GetWindowDrawList();const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();const float radius=theme.Y(theme.loadingSpinnerRadius),thickness=theme.X(theme.loadingSpinnerThickness);
    const ImVec2 center(pos.x+size.x*0.5f,pos.y+size.y*0.44f);const float start=static_cast<float>(ImGui::GetTime()*4.8);const float end=start+4.65f;
    draw->PathClear();draw->PathArcTo(center,radius,start,end,28);draw->PathStroke(ImGui::GetColorU32(theme.accentHover),0,thickness);
    const auto ts=ImGui::CalcTextSize(label);draw->AddText(ImVec2(center.x-ts.x*0.5f,center.y+radius+theme.Y(10.0f)),ImGui::GetColorU32(theme.textDisabled),label);
}
inline Result Render(const char* id,const json& catalog,char* filter,size_t filterCapacity,char* selected,size_t selectedCapacity,State& state,uint64_t revision,bool allowMulti=true,bool quickActions=false) {
    Result out;ImGui::PushID(id);LoadViewPreference(state);SyncLegacySelection(state,selected);
    if(!filter||!filterCapacity||!selected||!selectedCapacity){ImGui::TextDisabled("Item picker buffer unavailable.");ImGui::PopID();return out;}
    const auto& theme=HelpyImGuiTheme::Current;if(state.ActiveCategory!=theme.itemCategory){state.ActiveCategory=theme.itemCategory;state.Matches.clear();state.CatalogRevision=0;state.PageIndex=0;state.ResetScroll=true;}
    if(state.PageSize!=theme.itemPageSize){state.PageIndex=ItemPaging::Resize(state.PageIndex,state.PageSize,theme.itemPageSize);state.PageSize=ItemPaging::Size(theme.itemPageSize);state.ResetScroll=true;}
    if(state.CatalogRevision!=revision)Rebuild(catalog,state,revision);out.MatchCount=state.Matches.size();
    const float spacing=ImGui::GetStyle().ItemSpacing.x;
    const float controls=theme.X(210.0f),width=ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(std::max(theme.X(130.0f),width-controls-spacing*2));
    const bool enterSubmitted=ImGui::InputTextWithHint("##Search items","Search items...",filter,filterCapacity,ImGuiInputTextFlags_EnterReturnsTrue);
    if(state.SearchInput!=filter){state.SearchInput=filter;state.SearchDue=ImGui::GetTime()+0.3;}
    if(enterSubmitted||(state.SearchDue>0&&ImGui::GetTime()>=state.SearchDue)){
        state.SearchDue=0;state.Query=filter;state.PageIndex=0;state.ResetScroll=true;
        LazyCatalog::Search(0,BackendQuery(state));state.Matches.clear();state.CatalogRevision=0;
    }
    static constexpr const char* categories[]={"All","Weapons","Armor","Consumables","Ammo","Resources","Tools","Building","Quest","Misc"};
    if(ImGui::GetContentRegionAvail().x>controls)ImGui::SameLine();ImGui::SetNextItemWidth(theme.X(142.0f));
    if(ImGui::BeginCombo("##item-category",state.ActiveCategory=="All"?"All categories":state.ActiveCategory.c_str())){
        for(const char* category:categories)if(ImGui::Selectable(category,state.ActiveCategory==category)){
            state.ActiveCategory=category;HelpyImGuiTheme::Current.itemCategory=category;HelpyImGuiTheme::SaveCurrent();
            state.PageIndex=0;state.ResetScroll=true;LazyCatalog::Search(0,BackendQuery(state));state.Matches.clear();state.CatalogRevision=0;
        }ImGui::EndCombo();
    }
    if(ImGui::GetContentRegionAvail().x>theme.X(62.0f))ImGui::SameLine();LayoutControls();state.GridView=theme.itemGridView;
    ItemIconCache::Update();const auto& lazy=LazyCatalog::For(0);
    if(!lazy.HasMore&&!lazy.Pending&&lazy.Started){
        const int last=std::max(0,static_cast<int>((state.Matches.size()+state.PageSize-1)/state.PageSize)-1);
        if(state.PageIndex>last){state.PageIndex=last;state.ResetScroll=true;}
    }
    const auto page=ItemPaging::Slice(state.Matches.size(),state.PageIndex,state.PageSize,lazy.HasMore);
    LazyCatalog::FillTo(0,(state.PageIndex+1)*state.PageSize);
    const float availableLogical=ImGui::GetContentRegionAvail().y/std::max(0.01f,theme.Scale()*theme.density);
    const float maxPane=std::max(1.0f,availableLogical-(quickActions?ItemPaging::CartHeight(SharedCart.Entries().size(),theme.advancedMode)+80.0f:100.0f));
    const float gapLogical=ImGui::GetStyle().ItemSpacing.y/std::max(0.01f,theme.Scale()*theme.density);
    const float desiredPane=state.GridView&&theme.itemGridFitRows
        ?ItemBayLayout::RowsHeight(theme.itemGridRows,theme.gridCardHeight,gapLogical,theme.contentPaddingY)+2.0f
        :theme.itemResultHeight;
    const float minPane=state.GridView&&theme.itemGridFitRows?std::min(desiredPane,quickActions?140.0f:180.0f):(quickActions?140.0f:180.0f);
    state.ResultPaneHeightLogical=ItemBayLayout::PaneHeight(desiredPane,maxPane,minPane);
    HelpyImGuiTheme::BeginScroll("Rows",ImVec2(0,theme.Y(state.ResultPaneHeightLogical)),true);
    if(state.ResetScroll){ImGui::SetScrollY(0);state.ResetScroll=false;}
    const auto resultPos=ImGui::GetWindowPos(),resultSize=ImGui::GetWindowSize();
    if(quickActions && ImGui::BeginDragDropTargetCustom(ImRect(resultPos,ImVec2(resultPos.x+resultSize.x,resultPos.y+resultSize.y)),ImGui::GetID("return-to-items"))){
        if(const auto* payload=ImGui::AcceptDragDropPayload("RS_CART_PATH"))if(payload->DataSize>0 && payload->DataSize<=4097 && static_cast<const char*>(payload->Data)[payload->DataSize-1]=='\0')RemoveFromCart(static_cast<const char*>(payload->Data));
        ImGui::EndDragDropTarget();
    }
    const bool populationWait=lazy.Pending&&lazy.Rows.empty();
    if(populationWait)DrawLoadingSpinner(theme);
    else if(catalog.contains("Items")&&catalog["Items"].is_array()) {
        const auto& items=catalog["Items"];const int total=static_cast<int>(page.Count);
        if(!state.GridView) {
            const float iconSize=theme.Y(theme.listIconSize);const float itemHeight=std::max(ImGui::GetTextLineHeight()+theme.Y(4.0f),iconSize);const float rowHeight=itemHeight+ImGui::GetStyle().ItemSpacing.y;
            ImGuiListClipper clipper;clipper.Begin(total,rowHeight);
            while(clipper.Step()) {
                const int visibleFirst=clipper.DisplayStart,visibleLast=clipper.DisplayEnd;
                for(int rowIndex=visibleFirst;rowIndex<visibleLast;++rowIndex) {
                    const auto index=state.Matches[page.First+static_cast<size_t>(rowIndex)];if(index>=items.size())continue;
                    const auto& row=items[index];const auto path=row.value("Path",std::string{}),name=row.value("Name",path),iconPath=row.value("Icon",std::string{});const bool fits=path.size()<selectedCapacity,selectedThis=IsSelected(state,path);
                    ImGui::PushID(rowIndex);ImGui::PushID(path.c_str());const float rowWidth=ImGui::GetContentRegionAvail().x;ImGui::BeginDisabled(!fits);const auto rowPos=ImGui::GetCursorScreenPos();
                    if(ImGui::Selectable("##item",selectedThis,0,ImVec2(rowWidth,itemHeight))&&fits){if(ApplySelection(state,path,selected,selectedCapacity,ImGui::GetIO().KeyShift,allowMulti)){out.Changed=true;}}
                    if(fits)DragItem(row);
                    ImGui::EndDisabled();const float pad=theme.X(4.0f);const ImVec2 iconMin(rowPos.x+pad,rowPos.y+(itemHeight-iconSize)*0.5f),iconMax(iconMin.x+iconSize,iconMin.y+iconSize);
                    if(!iconPath.empty())HelpyIconTrace::Event("render-row",iconPath,"name='"+name+"' row="+std::to_string(rowIndex));
                    if(iconPath.empty()||!ItemIconCache::Draw(iconPath,iconMin,iconMax,true))DrawPlaceholder(iconMin,iconMax,theme);
                    const float badgeWidth=(ItemBadges::Power(row)||ItemBadges::Masterwork(row))?theme.X(130.0f):0.0f;
                    const float textX=iconMax.x+theme.X(9.0f);const auto label=Ellipsize(name,std::max(theme.X(30.0f),rowPos.x+rowWidth-badgeWidth-textX));
                    ImGui::GetWindowDrawList()->AddText(ImVec2(textX,rowPos.y+(itemHeight-ImGui::GetTextLineHeight())*0.5f),ImGui::GetColorU32(fits?theme.text:theme.textDisabled),label.c_str());
                    if(badgeWidth>0)DrawItemBadges(row,ImVec2(rowPos.x+rowWidth-badgeWidth,rowPos.y+(itemHeight-ImGui::GetTextLineHeight())*0.5f),ImVec2(rowPos.x+rowWidth,rowPos.y+itemHeight),theme);
                    if(quickActions)ContextMenu(catalog,row,path,name,selected,selectedCapacity,state,out,allowMulti,true);if(HelpyImGuiTheme::TooltipReady(std::string("item:")+path+":"+std::to_string(rowIndex),ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)))VerboseTooltip(row,name,path,allowMulti,fits);
                    ImGui::PopID();ImGui::PopID();
                }
                const int visibleRows=std::max(1,visibleLast-visibleFirst),adaptive=std::clamp(std::max(theme.overscanRows,visibleRows),theme.overscanRows,24),before=std::max(2,adaptive/2),after=adaptive;
                const int prefetchFirst=std::max(0,visibleFirst-before),prefetchLast=std::min(total,visibleLast+after);
                for(int rowIndex=prefetchFirst;rowIndex<prefetchLast;++rowIndex){if(rowIndex>=visibleFirst&&rowIndex<visibleLast)continue;const auto index=state.Matches[page.First+static_cast<size_t>(rowIndex)];if(index>=items.size())continue;const auto iconPath=items[index].value("Icon",std::string{});if(!iconPath.empty())ItemIconCache::Queue(iconPath,false);}
            }
        } else {
            const float spacing=ImGui::GetStyle().ItemSpacing.x,minCardWidth=theme.X(theme.gridMinCardWidth),avail=std::max(1.0f,ImGui::GetContentRegionAvail().x-theme.X(2.0f));
            const int columns=ItemBayLayout::Columns(avail,minCardWidth,spacing,theme.itemGridColumns);
            const float fittedHeight=(ImGui::GetContentRegionAvail().y-ImGui::GetStyle().ItemSpacing.y*(theme.itemGridRows-1))/std::max(1,theme.itemGridRows);
            const float cardWidth=std::max(1.0f,(avail-spacing*(columns-1))/columns),cardHeight=theme.itemGridFitRows?std::clamp(fittedHeight,theme.Y(72.0f),theme.Y(theme.gridCardHeight)):theme.Y(theme.gridCardHeight),
                iconSize=std::max(1.0f,std::min({theme.Y(theme.gridIconSize),cardWidth-theme.X(14.0f),cardHeight-theme.Y(44.0f)})),gridRowHeight=cardHeight+ImGui::GetStyle().ItemSpacing.y;
            const int gridRows=(total+columns-1)/columns;ImGuiListClipper clipper;clipper.Begin(gridRows,gridRowHeight);
            while(clipper.Step()) {
                const int visibleFirstRow=clipper.DisplayStart,visibleLastRow=clipper.DisplayEnd;const int visibleFirst=visibleFirstRow*columns,visibleLast=std::min(total,visibleLastRow*columns);
                for(int gridRow=visibleFirstRow;gridRow<visibleLastRow;++gridRow) {
                    for(int col=0;col<columns;++col) {
                        const int rowIndex=gridRow*columns+col;if(rowIndex>=total)break;const auto index=state.Matches[page.First+static_cast<size_t>(rowIndex)];if(index>=items.size())continue;
                        const auto& row=items[index];const auto path=row.value("Path",std::string{}),name=row.value("Name",path),iconPath=row.value("Icon",std::string{});const bool fits=path.size()<selectedCapacity,selectedThis=IsSelected(state,path);
                        ImGui::PushID(rowIndex);ImGui::PushID(path.c_str());const auto cardMin=ImGui::GetCursorScreenPos();ImGui::BeginDisabled(!fits);const bool clicked=ImGui::InvisibleButton("##card",ImVec2(cardWidth,cardHeight));ImGui::EndDisabled();const auto cardMax=ImVec2(cardMin.x+cardWidth,cardMin.y+cardHeight);auto* draw=ImGui::GetWindowDrawList();
                        const ImU32 bg=ImGui::GetColorU32(selectedThis?theme.selection:theme.panelAlt),border=ImGui::GetColorU32(selectedThis?theme.accentHover:theme.border);draw->AddRectFilled(cardMin,cardMax,bg,theme.X(theme.childRounding));draw->AddRect(cardMin,cardMax,border,theme.X(theme.childRounding),0,selectedThis?theme.X(2.0f):theme.X(1.0f));
                        const float iconX=(cardMin.x+cardMax.x-iconSize)*0.5f;const ImVec2 iconMin(iconX,cardMin.y+theme.Y(10.0f)),iconMax(iconX+iconSize,iconMin.y+iconSize);
                        if(!iconPath.empty())HelpyIconTrace::Event("render-row",iconPath,"name='"+name+"' row="+std::to_string(rowIndex));if(iconPath.empty()||!ItemIconCache::Draw(iconPath,iconMin,iconMax,true))DrawPlaceholder(iconMin,iconMax,theme);
                        const auto label=Ellipsize(name,cardWidth-theme.X(14.0f));const auto textSize=ImGui::CalcTextSize(label.c_str());draw->AddText(ImVec2(cardMin.x+(cardWidth-textSize.x)*0.5f,cardMax.y-theme.Y(29.0f)),ImGui::GetColorU32(fits?theme.text:theme.textDisabled),label.c_str());
                        DrawItemBadges(row,ImVec2(cardMin.x+theme.X(6.0f),cardMin.y+theme.Y(4.0f)),ImVec2(cardMax.x-theme.X(6.0f),cardMax.y),theme);
                        if(clicked&&fits){if(ApplySelection(state,path,selected,selectedCapacity,ImGui::GetIO().KeyShift,allowMulti)){out.Changed=true;}}
                        if(fits)DragItem(row);
                        if(quickActions)ContextMenu(catalog,row,path,name,selected,selectedCapacity,state,out,allowMulti,true);if(HelpyImGuiTheme::TooltipReady(std::string("item:")+path+":"+std::to_string(rowIndex),ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)))VerboseTooltip(row,name,path,allowMulti,fits);
                        ImGui::PopID();ImGui::PopID();if(col<columns-1&&rowIndex+1<total)ImGui::SameLine();
                    }
                }
                const int prefetchRows=std::max(1,theme.overscanRows/2),prefetchFirst=std::max(0,(visibleFirstRow-prefetchRows)*columns),prefetchLast=std::min(total,(visibleLastRow+prefetchRows)*columns);
                for(int rowIndex=prefetchFirst;rowIndex<prefetchLast;++rowIndex){if(rowIndex>=visibleFirst&&rowIndex<visibleLast)continue;const auto index=state.Matches[page.First+static_cast<size_t>(rowIndex)];if(index>=items.size())continue;const auto iconPath=items[index].value("Icon",std::string{});if(!iconPath.empty())ItemIconCache::Queue(iconPath,false);}
            }
        }
    }
    HelpyImGuiTheme::EndScroll(theme);ItemIconCache::Update();
    ImGui::PushID("results-bottom-splitter");const float splitH=std::max(theme.Y(theme.splitterWidth),theme.Y(5.0f));
    ImGui::InvisibleButton("##split",ImVec2(ImGui::GetContentRegionAvail().x,splitH));const bool splitHover=ImGui::IsItemHovered(),splitActive=ImGui::IsItemActive();
    if(splitHover||splitActive)ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    if(splitActive&&ImGui::GetIO().MouseDelta.y!=0.0f){state.ResultPaneHeightLogical+=ImGui::GetIO().MouseDelta.y/std::max(0.01f,theme.Scale()*theme.density);state.ResultPaneHeightLogical=ItemBayLayout::PaneHeight(state.ResultPaneHeightLogical,maxPane,minPane);HelpyImGuiTheme::Current.itemResultHeight=state.ResultPaneHeightLogical;if(state.GridView)HelpyImGuiTheme::Current.itemGridFitRows=false;}static bool paneDirty=false;if(splitActive)paneDirty=true;if(paneDirty&&!ImGui::IsMouseDown(ImGuiMouseButton_Left)){HelpyImGuiTheme::SaveCurrent();paneDirty=false;}
    const auto splitMin=ImGui::GetItemRectMin(),splitMax=ImGui::GetItemRectMax();if(splitHover||splitActive)ImGui::GetWindowDrawList()->AddLine(ImVec2(splitMin.x,(splitMin.y+splitMax.y)*0.5f),ImVec2(splitMax.x,(splitMin.y+splitMax.y)*0.5f),ImGui::GetColorU32(theme.accentHover),theme.X(1.5f));
    ImGui::PopID();
    PageControls(state,state.Matches.size(),lazy.HasMore&&lazy.LastError.empty(),lazy.Pending);
    if(!lazy.LastError.empty()&&!populationWait)ImGui::TextWrapped("Item query paused: %s",lazy.LastError.c_str());
    else if(state.Matches.empty()&&!populationWait&&lazy.Started)ImGui::TextDisabled("No matching items in the loaded runtime window.");
    if(state.SelectedRevision!=revision||state.SelectedPath!=selected) {
        state.SelectedRevision=revision;state.SelectedPath=selected;state.SelectedIndex=static_cast<size_t>(-1);
        if(catalog.contains("Items")&&catalog["Items"].is_array()){const auto& items=catalog["Items"];for(size_t i=0;i<items.size();++i)if(items[i].value("Path",std::string{})==selected){state.SelectedIndex=i;break;}}
    }
    if(catalog.contains("Items")&&catalog["Items"].is_array()&&state.SelectedIndex<catalog["Items"].size())out.SelectedRow=&catalog["Items"][state.SelectedIndex];
    out.SelectedTitle=out.SelectedRow?out.SelectedRow->value("Name",std::string(selected)):selected;out.SelectedPaths=state.SelectedPaths;out.SelectedCount=state.SelectedPaths.size();
    if(quickActions&&out.SelectedRow){
        ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(Ellipsize(out.SelectedTitle,std::max(theme.X(80.0f),ImGui::GetContentRegionAvail().x-theme.X(130.0f))).c_str());
        if(ImGui::GetContentRegionAvail().x>theme.X(115.0f))ImGui::SameLine();
        ImGui::BeginDisabled(SharedCart.Delivery()!=0);
        if(ImGui::SmallButton(out.SelectedCount>1?"Add selected to cart":"Add to cart")){
            if(out.SelectedCount>1){for(const auto& path:out.SelectedPaths)if(const auto* row=Find(catalog,path.c_str()))if(!AddToCart(*row))break;}
            else AddToCart(*out.SelectedRow);
        }ImGui::EndDisabled();
    }
    if(out.SelectedCount>1)ImGui::TextDisabled("%zu selected",out.SelectedCount);
    else if(!quickActions)ImGui::TextWrapped("Selected: %s",out.SelectedTitle.empty()?"None":out.SelectedTitle.c_str());
    if(allowMulti&&theme.advancedMode)ImGui::TextDisabled("Shift+click to select multiple items.");ImGui::PopID();return out;
}
}
