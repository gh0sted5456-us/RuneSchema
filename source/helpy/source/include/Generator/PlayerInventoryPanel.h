#pragma once
#include "Core/RuneSchemaProfile.h"
#include "Generator/ToolRequest.h"
#include "Generator/HelpyWindowChrome.h"
#include "Generator/ItemBayBrowser.h"
#include "Core/CharacterWorkbench.h"
#include "Generator/MapLayerIcons.h"
#include "Generator/AuthoringClient.h"
#include "Core/LoadoutEligibility.h"
namespace PS::PlayerInventoryPanel {
using Json=nlohmann::json;
inline bool Open=false,Detached=false,Refresh=false,AddWanted=false,Personal=false,Container=false,WasDelivering=false,FocusCart=false;
inline Json Target,Snapshot;
inline std::string Status,Group="Inventory",SearchQuery,SelectedSlot;
inline char Search[257]{},BaySearch[257]{},BaySelected[4097]{};
inline ItemGridPicker::State Bay;
inline Json SelectedBay;
inline Json SelectedInventory,GearTarget,GearRows=Json::array();inline bool ChangingGear=false,GearMore=false;inline size_t GearOffset=0;inline uint64_t GearPending=0,GearCatalogRequest=0;
inline std::string GearLocation,GearQuery,GearCursor;inline bool GearMutationUnconfirmed=false;
inline size_t GearWanted=48;inline uint64_t GearRevision=1;inline std::chrono::steady_clock::time_point GearStarted{};
inline uint64_t GearRequestedRevision=0;inline size_t GearRequestedOffset=0;inline std::string GearRequestedQuery;
inline ItemSource::Kind Source=ItemSource::Kind::All;
inline bool FavoritesOnly=false;inline std::string PersonalTag;
inline std::function<bool()> BindRecipient;
inline std::function<void(float)> RenderCart;
inline int Offset=0;inline uint64_t Pending=0,Sequence=0x5700000000000000ull,Generation=0,Revision=1;
inline std::chrono::steady_clock::time_point Started{};
inline HelpyWindowChrome::State Chrome;
inline void Request(const Json& row){Target=row;Snapshot=Json::object();Offset=0;Personal=false;Container=false;Pending=0;Group="Inventory";SelectedSlot.clear();SelectedInventory=Json::object();ChangingGear=false;GearMutationUnconfirmed=false;GearCatalogRequest=0;Open=true;Detached=true;Refresh=true;Generation=SpawnToolRequests::CurrentGeneration();Status="Reading native inventory...";HelpyTaskbar::Current.Restore(HelpyTaskbar::Window::Inventory);}
inline void ResetGearCatalog(){
    GearRows=Json::array();GearOffset=0;GearWanted=48;GearCursor.clear();GearMore=LoadoutEligibility::Valid(GearLocation);GearQuery=ItemGridPicker::BackendQuery(Bay);++GearRevision;Bay.CatalogRevision=0;Bay.PageIndex=0;
    GearRows=ItemGridPicker::CachedSlotRows(GearLocation,GearQuery,Generation);
}
inline void ChangeGear(const Json& player,const Json& item){Request(player);ChangingGear=true;GearTarget=item;GearLocation=item.value("Location",std::string{});Bay.Query.clear();BaySearch[0]=0;BaySelected[0]=0;Bay.SelectedPath.clear();Bay.SelectedPaths.clear();Bay.FavoritesOnly=false;Bay.PersonalTag.clear();Bay.ActiveCategory="All";SelectedBay=Json::object();Bay.SourceFilter=ItemSource::Kind::All;Bay.FamilyKey.clear();ResetGearCatalog();Status="Choose matching equipment. Equip an owned item, or add a new one to the delivery cart.";}
inline void GearAction(const char* action,const Json& item){
    if(GearPending||GearMutationUnconfirmed||Generation!=SpawnToolRequests::CurrentGeneration()){Status="Wait for the gear result or reopen the current party member before retrying.";return;}
    Json request={{"Action",action},{"_RequestId",++Sequence},{"_Source","HelpyStandalone"},{"Player",Target.value("Path",std::string{})},{"PlayerGuid",Target.value("Guid",std::string{})},{"Generation",Generation},{"Path",item.value("Path",std::string{})},{"Instance",item.value("Instance",Json{})},{"Slot",ChangingGear?GearTarget.value("Slot",-1):item.value("Slot",-1)}};
    if(std::string_view(action)=="ChangePlayerGear"){request["SourceSlot"]=item.value("Slot",-1);request["Location"]=GearLocation;request["ExpectedTarget"]=GearTarget.value("Instance",Json::array({0,0,0,0}));}
    const auto id=request.at("_RequestId").get<uint64_t>();if(SpawnToolRequests::TrySubmit(request)){GearPending=id;GearStarted=std::chrono::steady_clock::now();Status="Native gear action queued; no automatic retry.";}else Status="Runtime is busy; gear action was not submitted.";
}
inline uint64_t RepairPending=0,RepairGeneration=0;
inline std::string RepairStatus;inline std::chrono::steady_clock::time_point RepairStarted{};
inline void RestoreGear(const Json& player,const Json& item){
    if(RepairPending){RepairStatus="A durability repair is already pending.";return;}
    const auto generation=SpawnToolRequests::CurrentGeneration(),id=++Sequence;
    Json request={{"Action","RestorePlayerGear"},{"_RequestId",id},{"_Source","HelpyStandalone"},{"Player",player.value("Path",std::string{})},{"PlayerGuid",player.value("Guid",std::string{})},{"Generation",generation},{"Path",item.value("Path",std::string{})},{"Instance",item.value("Instance",Json{})},{"Slot",item.value("Slot",-1)}};
    if(SpawnToolRequests::TrySubmit(request)){RepairPending=id;RepairGeneration=generation;RepairStarted=std::chrono::steady_clock::now();RepairStatus="Restoring durability…";}
    else RepairStatus="Runtime busy; repair was not submitted.";
}
inline void PollRepair(){
    if(!RepairPending)return;
    if(RepairGeneration!=SpawnToolRequests::CurrentGeneration()){RepairPending=0;RepairStatus="World changed; repair result unconfirmed. No retry.";return;}
    Json result;if(SpawnToolRequests::TakeCompleted(RepairPending,result)){
        RepairPending=0;const bool confirmed=result.value("EquipmentAction",Json::object()).value("Confirmed",false);
        RepairStatus=confirmed?"Durability restored.":result.value("Status",std::string("Repair not confirmed. Check durability before retrying."));
    }else if(std::chrono::steady_clock::now()-RepairStarted>std::chrono::seconds(15)){RepairPending=0;RepairStatus="Repair result unconfirmed. Check durability before retrying.";}
}
inline void RequestContainer(const Json& row){Request(row);Container=true;Group="Storage";Status="Reading loaded placed-storage contents...";}
inline void Reload(){Offset=0;Snapshot=Json::object();Refresh=true;++Revision;}
inline bool ValidSnapshot(){return Snapshot.contains("ObservedUnixMs")&&Generation==SpawnToolRequests::CurrentGeneration();}
inline void Poll(){
    PollRepair();
    if(!Open)return;
    if(Generation!=SpawnToolRequests::CurrentGeneration()){Snapshot=Json::object();Pending=0;Refresh=false;GearMutationUnconfirmed|=GearPending!=0;GearPending=0;GearCatalogRequest=0;GearMore=false;Status="World changed; close this view and select a current party member.";return;}
    if(GearPending){Json result;if(SpawnToolRequests::TakeCompleted(GearPending,result)){GearPending=0;Status=result.value("Status",std::string("Gear action returned without a receipt; inspect inventory."));GearMutationUnconfirmed=!result.value("EquipmentAction",Json::object()).value("Confirmed",false);Reload();}}
    if(GearPending&&std::chrono::steady_clock::now()-GearStarted>std::chrono::seconds(15)){GearPending=0;GearMutationUnconfirmed=true;Status="Gear result unconfirmed. Inspect the inventory and reopen this player; no retry was attempted.";}
    if(ChangingGear&&LoadoutEligibility::Valid(GearLocation)){
        const bool fittingActive=AuthoringClient::EnsureActive();
        if(GearCatalogRequest&&AuthoringClient::LastReceipt==GearCatalogRequest){const auto& result=AuthoringClient::Result("SaveEditorSlotItems");GearCatalogRequest=0;
            if(result.contains("Error")){GearMore=false;Status=result.at("Error").get<std::string>();}
            else if(result.value("Slot",std::string{})==GearLocation&&GearRequestedRevision==GearRevision&&GearRequestedOffset==GearOffset&&GearRequestedQuery==GearQuery){for(const auto& row:result.value("Rows",Json::array()))if(LoadoutEligibility::Matches(row,GearLocation)&&GearRows.size()<2048){const auto path=ItemPresentation::Text(row,"Path");const auto found=std::find_if(GearRows.begin(),GearRows.end(),[&](const auto& value){return ItemPresentation::Text(value,"Path")==path;});if(found==GearRows.end())GearRows.push_back(row);else *found=row;}const auto next=result.value("NextOffset",size_t{});GearCursor=result.value("NextCursor",std::string{});GearMore=result.value("HasMore",false)&&next>GearOffset;GearOffset=next;++GearRevision;}}
        if(GearQuery!=ItemGridPicker::BackendQuery(Bay)&&!GearCatalogRequest)ResetGearCatalog();
        GearWanted=std::min<size_t>(2048,std::max(GearWanted,size_t((Bay.PageIndex+1)*Bay.PageSize)));
        if(fittingActive&&!GearCatalogRequest&&GearMore&&(GearOffset==0||GearRows.size()<GearWanted)&&!AuthoringClient::Busy()&&AuthoringClient::Submit("SaveEditorSlotItems",{{"Slot",GearLocation},{"Query",GearQuery},{"Offset",GearOffset},{"Cursor",GearCursor},{"CacheFirst",true}})){GearCatalogRequest=AuthoringClient::Pending;GearRequestedRevision=GearRevision;GearRequestedOffset=GearOffset;GearRequestedQuery=GearQuery;}
    }
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
    if(ImGui::BeginTabBar("##live-inventory-tabs",ImGuiTabBarFlags_FittingPolicyScroll|ImGuiTabBarFlags_Reorderable)){
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
    PersonalizationPanel::Filters(FavoritesOnly,PersonalTag);
    for(size_t i=0;i<rows.size();++i){const auto& row=rows[i];auto group=Container?std::string("Storage"):Personal?std::string("Personal storage"):row.value("Group",row.value("Slot",8)<8?std::string("Quick slots"):CharacterWorkbench::InventoryGroup(row));
        if(Group!="Equipped"&&group!=Group)continue;
        if(ChangingGear&&(Group=="Equipped"||Personal||!LoadoutEligibility::Matches(row,GearLocation)))continue;
        if(Source!=ItemSource::Kind::All&&ItemSource::Row(row).Group!=Source)continue;
        if(!PersonalizationPanel::Matches(row,FavoritesOnly,PersonalTag))continue;
        if(!SearchQuery.empty()&&ItemGridPicker::Lower(ItemPresentation::OriginalName(row)+" "+ItemPresentation::Name(row)+" "+HelpyPersonalization::SearchText(row)+" "+ItemPresentation::Text(row,"Path")+" "+ItemPresentation::Text(row,"SourceName")).find(SearchQuery)==std::string::npos)continue;
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
        if(column)ImGui::SameLine();ImGui::PushID(slot.c_str());const auto a=ImGui::GetCursorScreenPos();if(ImGui::InvisibleButton("##inventory-card",{width,cardHeight})){SelectedSlot=slot;SelectedInventory=row;ItemGridPicker::PinDetails(row);}
        ItemGridPicker::DrawPlacard(row,a,{a.x+width,a.y+cardHeight},SelectedSlot==slot,ImGui::IsItemHovered());
        const auto count=std::string("×")+std::to_string(row.value("Count",1));const auto extent=ImGui::CalcTextSize(count.c_str());ImGui::GetWindowDrawList()->AddText({a.x+width-extent.x-t.X(8),a.y+t.Y(6)},ImGui::GetColorU32(t.text),count.c_str());
        if(HelpyImGuiTheme::TooltipReady("live-inventory:"+slot,ImGui::IsItemHovered()))ItemGridPicker::VerboseTooltip(row,name,path,false,true);
        if(ImGui::BeginPopupContextItem("##inventory-item-actions")){ImGui::TextUnformatted(name.c_str());ImGui::Separator();ItemGridPicker::InspectActions(row);ImGui::Separator();PersonalizationPanel::Menu(row);
            ImGui::TextDisabled("Slot %s · count %d",slot.c_str(),row.value("Count",1));ImGui::EndPopup();}
        ImGui::PopID();
    }
    if(matches.empty())ImGui::TextWrapped("%s",Pending||Refresh?"Reading inventory pages…":Group=="Equipped"&&Snapshot.contains("LoadoutError")?Snapshot["LoadoutError"].get_ref<const std::string&>().c_str():"No matching items in this tab.");
    if(ImGui::BeginDragDropTargetCustom(ImRect(dropMin,{dropMin.x+dropSize.x,dropMin.y+dropSize.y}),ImGui::GetID("##inventory-cart-drop"))){if(auto row=ItemGridPicker::AcceptItemDrop(Status))Stage(*row);ImGui::EndDragDropTarget();}
    ImGui::EndChild();ImGui::TextDisabled("Drop Item Bay items here to stage delivery—not move existing items.");ImGui::PopID();
}
inline void Contents(){
    const auto& t=HelpyImGuiTheme::Current;ImGui::PushID("live-inventory-workbench");
    ImGui::TextWrapped("%s",RuneSchemaProfile::Name(Target).c_str());ImGui::SameLine();if(HelpyImGuiTheme::SoftButton(Detached?"Dock inventory":"Detach inventory"))Detached=!Detached;
    const bool delivering=ItemGridPicker::SharedCart.Delivery()!=0;if(WasDelivering&&!delivering&&Generation==SpawnToolRequests::CurrentGeneration())Reload();WasDelivering=delivering;
    ImGui::BeginChild("##inventory-workbench-body",{0,0});
    if(Container){ImGui::TextWrapped("Lock status: Unknown · locked by: not verified");ImGui::TextWrapped("%s",Snapshot.value("AccessReason",std::string("Direct chest deposits and unlock overrides require a verified native access contract.")).c_str());}
    ImGui::TextWrapped("%s",Status.c_str());
    if(ChangingGear){
        ImGui::SeparatorText(("Change gear · "+GearLocation).c_str());
        ItemBayBrowser::Controls(Bay,BaySearch,sizeof(BaySearch),[]{SelectedBay=Json::object();BaySelected[0]=0;ResetGearCatalog();},false);
        LazyCatalog::State view;view.Started=true;view.HasMore=GearMore;view.Pending=GearCatalogRequest!=0;
        Json catalog={{"Items",GearRows}};
        const auto result=ItemGridPicker::Render("matching-equipment-bay",catalog,BaySearch,sizeof(BaySearch),BaySelected,sizeof(BaySelected),Bay,Revision+GearRevision,false,false,true,true,false,320,&view);
        if(result.SelectedRow)SelectedBay=*result.SelectedRow;
        const Json* owned=nullptr;if(ValidSnapshot()&&Snapshot.contains("Items"))for(const auto& row:Snapshot["Items"])if(!SelectedBay.empty()&&ItemPresentation::Text(row,"Path")==ItemPresentation::Text(SelectedBay,"Path")&&row.contains("Instance")){owned=&row;break;}
        ImGui::BeginDisabled(SelectedBay.empty()||!ValidSnapshot()||GearPending||GearMutationUnconfirmed);
        if(owned){if(HelpyImGuiTheme::SoftButton("Equip selected item"))GearAction("ChangePlayerGear",*owned);}
        else if(HelpyImGuiTheme::SoftButton("Add selected to delivery cart"))Stage(SelectedBay);
        ImGui::EndDisabled();
        if(!ValidSnapshot())ImGui::TextDisabled("Checking owned inventory in the background; cached choices remain available.");
        ImGui::TextDisabled("Native slot, item-instance and host-authority checks still apply. No automatic grants or swaps.");
    }else{
    const bool wide=ImGui::GetContentRegionAvail().x>=t.X(560);
    if(ImGui::BeginTable("##inventory-workbench-panes",wide?2:1,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_BordersInnerV)){
        ImGui::TableNextColumn();Inventory(280);if(wide)ImGui::TableNextColumn();ImGui::PushID("live-item-bay-pane");ImGui::SeparatorText("Item Bay");
        if(!LazyCatalog::For(0).Started&&!LazyCatalog::For(0).Pending)LazyCatalog::Search(0,ItemGridPicker::BackendQuery(Bay));
        Json catalog=Json::object();LazyCatalog::Overlay(catalog,0);
        ItemBayBrowser::Controls(Bay,BaySearch,sizeof(BaySearch));
        const auto result=ItemGridPicker::Render("live-inventory-item-bay",catalog,BaySearch,sizeof(BaySearch),BaySelected,sizeof(BaySelected),Bay,Revision+LazyCatalog::For(0).AppliedRevision,false,false,true,true,false,280);
        if(result.SelectedRow)SelectedBay=*result.SelectedRow;
        ImGui::BeginDisabled(SelectedBay.empty()||Container||Personal||delivering);if(HelpyImGuiTheme::SoftButton("Stage selected for delivery"))Stage(SelectedBay);ImGui::EndDisabled();ImGui::PopID();ImGui::EndTable();
    }
    }
    const bool populated=!ItemGridPicker::SharedCart.Entries().empty()||!ItemGridPicker::SharedCart.Outcomes().empty()||delivering;
    if(populated||!ChangingGear){ImGui::SeparatorText("Delivery cart");ImGui::TextWrapped("Review quantities, then Give items. Delivery uses Item Service's native validation and receipts; no automatic retries.");}
    if(populated){const bool ready=!Container&&!Personal&&ValidSnapshot()&&BindRecipient&&BindRecipient();
        if(!ready)ImGui::TextDisabled("Delivery unavailable for this storage view or stale player snapshot.");
        ImGui::BeginDisabled(!ready);if(RenderCart)RenderCart(t.Y(400));ImGui::EndDisabled();
        if(FocusCart){ImGui::SetScrollHereY(1.f);FocusCart=false;}
    }else if(!ChangingGear)ImGui::TextDisabled("Your cart will appear here when you stage items.");
    ImGui::EndChild();ImGui::PopID();
}
inline void RenderDocked(){if(!Open||Detached||HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::Inventory))return;HelpyWindowChrome::Prepare(Chrome);ImGui::SetNextWindowSize({1000,760},ImGuiCond_FirstUseEver);if(ImGui::Begin("Inventory workbench",&Open,ImGuiWindowFlags_NoSavedSettings)){HelpyWindowChrome::Render(Chrome,HelpyTaskbar::Window::Inventory);Contents();}ImGui::End();}
}
