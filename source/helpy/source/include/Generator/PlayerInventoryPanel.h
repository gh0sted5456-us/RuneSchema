#pragma once
#include "Generator/ToolRequest.h"
#include "Generator/HelpyWindowChrome.h"
#include "Generator/ItemBayBrowser.h"
#include "Core/CharacterWorkbench.h"
#include "Generator/MapLayerIcons.h"
namespace PS::PlayerInventoryPanel {
using Json=nlohmann::json;
inline bool Open=false,Detached=false,Refresh=false,AddWanted=false,Personal=false,Container=false,WasDelivering=false,FocusCart=false;
inline Json Target,Snapshot;
inline std::string Status,Group="Inventory",SearchQuery,SelectedSlot;
inline char Search[257]{},BaySearch[257]{},BaySelected[4097]{};
inline ItemGridPicker::State Bay;
inline Json SelectedBay;
inline ItemSource::Kind Source=ItemSource::Kind::All;
inline std::function<bool()> BindRecipient;
inline std::function<void(float)> RenderCart;
inline int Offset=0;inline uint64_t Pending=0,Sequence=0x5700000000000000ull,Generation=0,Revision=1;
inline std::chrono::steady_clock::time_point Started{};
inline HelpyWindowChrome::State Chrome;
inline void Request(const Json& row){Target=row;Snapshot=Json::object();Offset=0;Personal=false;Container=false;Pending=0;Group="Inventory";SelectedSlot.clear();Open=true;Detached=true;Refresh=true;Generation=SpawnToolRequests::CurrentGeneration();Status="Reading native inventory...";HelpyTaskbar::Current.Restore(HelpyTaskbar::Window::Inventory);}
inline void RequestContainer(const Json& row){Request(row);Container=true;Group="Storage";Status="Reading loaded placed-storage contents...";}
inline void Reload(){Offset=0;Snapshot=Json::object();Refresh=true;++Revision;}
inline bool ValidSnapshot(){return Snapshot.contains("ObservedUnixMs")&&Generation==SpawnToolRequests::CurrentGeneration();}
inline void Poll(){
    if(!Open)return;
    if(Generation!=SpawnToolRequests::CurrentGeneration()){Snapshot=Json::object();Pending=0;Refresh=false;Status="World changed; close this view and select a current party member.";return;}
    Json receipt;if(Pending&&SpawnToolRequests::TakeCompleted(Pending,receipt)){
        Pending=0;Status=receipt.value("Status",std::string{});
        const auto value=receipt.find("InventorySnapshot");
        if(value!=receipt.end()&&value->is_object()&&value->value("Offset",0)==Offset&&value->value("Generation",uint64_t{})==Generation&&value->value("PlayerGuid",std::string{})==Target.value("Guid",std::string{})&&value->value("Player",std::string{})==Target.value("Path",std::string{})&&value->value("Container",false)==Container&&value->value("Personal",false)==Personal){
            if(Offset==0)Snapshot=*value;
            else{auto accumulated=Snapshot.value("Items",Json::array());for(const auto& row:value->value("Items",Json::array()))accumulated.push_back(row);const auto loadout=Snapshot.value("Loadout",Json::array());const auto loadoutError=Snapshot.value("LoadoutError",std::string{});Snapshot=*value;Snapshot["Items"]=std::move(accumulated);Snapshot["Loadout"]=loadout;if(!loadoutError.empty())Snapshot["LoadoutError"]=loadoutError;}
            ++Revision;const auto next=value->value("NextOffset",Offset);
            if(value->value("HasMore",false)&&next>Offset&&next<=1024){Offset=next;Refresh=true;Status="Reading remaining inventory pages...";}
            else if(value->value("HasMore",false))Status="Partial inventory snapshot: native page cursor did not advance or exceeds the 1,024-slot browser limit.";
        }
    }
    if(Pending&&std::chrono::steady_clock::now()-Started>std::chrono::seconds(15)){Pending=0;Refresh=false;Status="Inventory read timed out; refresh to retry.";}
    if(Refresh&&!Pending){const auto id=++Sequence;if(SpawnToolRequests::TrySubmit({{"Action",Container?"InspectContainerInventory":"InspectPlayerInventory"},{"_RequestId",id},{"_Source","HelpyStandalone"},{"Player",Target.value("Path",std::string{})},{"PlayerGuid",Target.value("Guid",std::string{})},{"Generation",Generation},{"ObjectIndex",Target.value("ObjectIndex",-1)},{"ObjectSerial",Target.value("ObjectSerial",0)},{"Personal",Personal},{"Offset",Offset}})){Pending=id;Refresh=false;Started=std::chrono::steady_clock::now();}}
}
inline bool Stage(const Json& row){
    if(Container||Personal){Status="Direct chest delivery is not verified. Select an inventory tab to stage delivery to this player.";return false;}
    if(!ValidSnapshot()||!BindRecipient||!BindRecipient()){Status="Current player identity is unavailable or a delivery is already running. Refresh before staging.";return false;}
    if(!ItemGridPicker::AddToCart(row)){Status=ItemGridPicker::CartStatus;return false;}
    FocusCart=true;Status="Item staged in the shared cart. Review quantities and press Give items to deliver.";return true;
}
inline void Inventory(float height){
    const auto& t=HelpyImGuiTheme::Current;ImGui::PushID("live-inventory-pane");ImGui::SeparatorText("Inventory");
    if(ImGui::BeginTabBar("##live-inventory-tabs",ImGuiTabBarFlags_FittingPolicyScroll)){
        const auto tabs=Container?std::vector<const char*>{"Storage"}:std::vector<const char*>{"Inventory","Quick slots","Ammo","Runes","Quest items","Personal storage","Equipped"};
        for(const auto* label:tabs)if(ImGui::BeginTabItem(label)){
            if(Group!=label){Group=label;const bool personal=Group=="Personal storage";if(Personal!=personal){Personal=personal;Reload();}SelectedSlot.clear();}ImGui::EndTabItem();
        }ImGui::EndTabBar();
    }
    ImGui::SetNextItemWidth(std::max(60.f,ImGui::GetContentRegionAvail().x-t.X(135)));
    const bool enter=ImGui::InputTextWithHint("##live-inventory-search","Search inventory…",Search,sizeof(Search),ImGuiInputTextFlags_EnterReturnsTrue);ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Search")||enter)SearchQuery=ItemGridPicker::Lower(Search);
    ImGui::SameLine();ImGui::BeginDisabled(Pending!=0);if(HelpyImGuiTheme::SoftButton("Refresh"))Reload();ImGui::EndDisabled();
    ImGui::SetNextItemWidth(-1);if(ImGui::BeginCombo("##inventory-source",ItemSource::Label(Source))){
        for(const auto value:{ItemSource::Kind::All,ItemSource::Kind::Vanilla,ItemSource::Kind::Expansion,ItemSource::Kind::Modded,ItemSource::Kind::Unknown})if(ImGui::Selectable(ItemSource::Label(value),Source==value))Source=value;ImGui::EndCombo();
    }
    const auto rows=Snapshot.value(Group=="Equipped"?"Loadout":"Items",Json::array());std::vector<size_t> matches;
    for(size_t i=0;i<rows.size();++i){const auto& row=rows[i];auto group=Container?std::string("Storage"):Personal?std::string("Personal storage"):row.value("Group",row.value("Slot",8)<8?std::string("Quick slots"):CharacterWorkbench::InventoryGroup(row));
        if(Group!="Equipped"&&group!=Group)continue;
        if(Source!=ItemSource::Kind::All&&ItemSource::Row(row).Group!=Source)continue;
        if(!SearchQuery.empty()&&ItemGridPicker::Lower(ItemPresentation::Name(row)+" "+ItemPresentation::Text(row,"Path")+" "+ItemPresentation::Text(row,"SourceName")).find(SearchQuery)==std::string::npos)continue;
        matches.push_back(i);
    }
    ImGui::TextDisabled("%zu items%s",matches.size(),Pending||Refresh?" · reading…":"");
    ImGui::BeginChild("##inventory-rows",{0,t.Y(height)},true);
    const auto dropMin=ImGui::GetWindowPos(),dropSize=ImGui::GetWindowSize();
#ifdef HELPY_PREVIEW
    HelpyPreviewHooks::LiveInventoryViewport=HelpyPreviewHooks::WindowRect();
#endif
    const int columns=ItemBayLayout::Columns(ImGui::GetContentRegionAvail().x,t.X(t.gridMinCardWidth),ImGui::GetStyle().ItemSpacing.x,t.itemGridColumns);
    const float width=std::max(1.f,(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x*(columns-1))/columns),cardHeight=t.Y(t.gridCardHeight);
    ImGuiListClipper clip;clip.Begin(int((matches.size()+columns-1)/columns),cardHeight+ImGui::GetStyle().ItemSpacing.y);
    while(clip.Step())for(int line=clip.DisplayStart;line<clip.DisplayEnd;++line)for(int column=0;column<columns;++column){const size_t index=size_t(line)*columns+column;if(index>=matches.size())break;
        const auto& row=rows[matches[index]];const auto slot=std::to_string(row.value("Slot",0)),path=ItemPresentation::Text(row,"Path"),name=ItemPresentation::Name(row);
        if(column)ImGui::SameLine();ImGui::PushID(slot.c_str());const auto a=ImGui::GetCursorScreenPos();if(ImGui::InvisibleButton("##inventory-card",{width,cardHeight})){SelectedSlot=slot;ItemGridPicker::PinDetails(row);}
        ItemGridPicker::DrawPlacard(row,a,{a.x+width,a.y+cardHeight},SelectedSlot==slot,ImGui::IsItemHovered());
        const auto count=std::string("×")+std::to_string(row.value("Count",1));const auto extent=ImGui::CalcTextSize(count.c_str());ImGui::GetWindowDrawList()->AddText({a.x+width-extent.x-t.X(8),a.y+t.Y(6)},ImGui::GetColorU32(t.text),count.c_str());
        if(HelpyImGuiTheme::TooltipReady("live-inventory:"+slot,ImGui::IsItemHovered()))ItemGridPicker::VerboseTooltip(row,name,path,false,true);
        if(ImGui::BeginPopupContextItem("##inventory-item-actions")){if(ImGui::MenuItem("Show item details"))ItemGridPicker::PinDetails(row);ImGui::TextDisabled("Slot %s · count %d",slot.c_str(),row.value("Count",1));ImGui::EndPopup();}
        ImGui::PopID();
    }
    if(matches.empty())ImGui::TextWrapped("%s",Pending||Refresh?"Reading inventory pages…":Group=="Equipped"&&Snapshot.contains("LoadoutError")?Snapshot["LoadoutError"].get_ref<const std::string&>().c_str():"No matching items in this tab.");
    if(ImGui::BeginDragDropTargetCustom(ImRect(dropMin,{dropMin.x+dropSize.x,dropMin.y+dropSize.y}),ImGui::GetID("##inventory-cart-drop"))){if(auto row=ItemGridPicker::AcceptItemDrop(Status))Stage(*row);ImGui::EndDragDropTarget();}
    ImGui::EndChild();ImGui::TextDisabled("Drop Item Bay items here to stage delivery—not move existing items.");ImGui::PopID();
}
inline void Contents(){
    const auto& t=HelpyImGuiTheme::Current;ImGui::PushID("live-inventory-workbench");
    ImGui::TextWrapped("%s",Target.value("Name",std::string("Player")).c_str());ImGui::SameLine();if(HelpyImGuiTheme::SoftButton(Detached?"Dock inventory":"Detach inventory"))Detached=!Detached;
    const bool delivering=ItemGridPicker::SharedCart.Delivery()!=0;if(WasDelivering&&!delivering&&Generation==SpawnToolRequests::CurrentGeneration())Reload();WasDelivering=delivering;
    ImGui::BeginChild("##inventory-workbench-body",{0,0});
    if(Container){ImGui::TextWrapped("Lock status: Unknown · locked by: not verified");ImGui::TextWrapped("%s",Snapshot.value("AccessReason",std::string("Direct chest deposits and unlock overrides require a verified native access contract.")).c_str());}
    ImGui::TextWrapped("%s",Status.c_str());const bool wide=ImGui::GetContentRegionAvail().x>=t.X(560);
    if(ImGui::BeginTable("##inventory-workbench-panes",wide?2:1,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_BordersInnerV)){
        ImGui::TableNextColumn();Inventory(280);if(wide)ImGui::TableNextColumn();ImGui::PushID("live-item-bay-pane");ImGui::SeparatorText("Item Bay");
        if(!LazyCatalog::For(0).Started&&!LazyCatalog::For(0).Pending)LazyCatalog::Search(0,ItemGridPicker::BackendQuery(Bay));
        Json catalog=Json::object();LazyCatalog::Overlay(catalog,0);ItemBayBrowser::Controls(Bay,BaySearch,sizeof(BaySearch));
        const auto result=ItemGridPicker::Render("live-inventory-item-bay",catalog,BaySearch,sizeof(BaySearch),BaySelected,sizeof(BaySelected),Bay,Revision+LazyCatalog::For(0).AppliedRevision,false,false,true,true,false,280);
        if(result.SelectedRow)SelectedBay=*result.SelectedRow;
        ImGui::BeginDisabled(SelectedBay.empty()||Container||Personal||delivering);if(HelpyImGuiTheme::SoftButton("Stage selected for delivery"))Stage(SelectedBay);ImGui::EndDisabled();ImGui::PopID();ImGui::EndTable();
    }
    ImGui::SeparatorText("Delivery cart");ImGui::TextWrapped("Review quantities, then Give items. Delivery uses Item Service's native validation and receipts; no automatic retries.");
    const bool populated=!ItemGridPicker::SharedCart.Entries().empty()||!ItemGridPicker::SharedCart.Outcomes().empty()||delivering;
    if(populated){const bool ready=!Container&&!Personal&&ValidSnapshot()&&BindRecipient&&BindRecipient();
        if(!ready)ImGui::TextDisabled("Delivery unavailable for this storage view or stale player snapshot.");
        ImGui::BeginDisabled(!ready);if(RenderCart)RenderCart(t.Y(400));ImGui::EndDisabled();
        if(FocusCart){ImGui::SetScrollHereY(1.f);FocusCart=false;}
    }else ImGui::TextDisabled("Your cart will appear here when you stage items.");
    ImGui::EndChild();ImGui::PopID();
}
inline void RenderDocked(){if(!Open||Detached||HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::Inventory))return;HelpyWindowChrome::Prepare(Chrome);ImGui::SetNextWindowSize({1000,760},ImGuiCond_FirstUseEver);if(ImGui::Begin("Inventory workbench",&Open,ImGuiWindowFlags_NoSavedSettings)){HelpyWindowChrome::Render(Chrome,HelpyTaskbar::Window::Inventory);Contents();}ImGui::End();}
}
