#pragma once
#include "Generator/ToolRequest.h"
#include "Generator/HelpyWindowChrome.h"
#include "Generator/ItemIconCache.h"
#include "Core/ItemPresentation.h"
#include "Generator/MapLayerIcons.h"
namespace PS::PlayerInventoryPanel {
using Json=nlohmann::json;
inline bool Open=false,Detached=false,Refresh=false,AddWanted=false,Personal=false,Container=false;
inline Json Target,Snapshot;
inline std::string Status;
inline int Offset=0;inline uint64_t Pending=0,Sequence=0x5700000000000000ull,Generation=0;
inline std::chrono::steady_clock::time_point Started{};
inline HelpyWindowChrome::State Chrome;
inline void Request(const Json& row){Target=row;Snapshot=Json::object();Offset=0;Personal=false;Container=false;Pending=0;Open=true;Detached=true;Refresh=true;Generation=SpawnToolRequests::CurrentGeneration();Status="Reading native inventory...";HelpyTaskbar::Current.Restore(HelpyTaskbar::Window::Inventory);}
inline void RequestContainer(const Json& row){Request(row);Container=true;Status="Reading loaded placed-storage contents...";}
inline void Poll(){
    if(!Open)return;
    if(Generation!=SpawnToolRequests::CurrentGeneration()){Snapshot=Json::object();Pending=0;Refresh=false;Status="World changed; close this view and select a current party member.";return;}
    Json receipt;if(Pending&&SpawnToolRequests::TakeCompleted(Pending,receipt)){Pending=0;Status=receipt.value("Status",std::string{});
        const auto value=receipt.find("InventorySnapshot");if(value!=receipt.end()&&value->is_object()&&value->value("Generation",uint64_t{})==Generation&&value->value("PlayerGuid",std::string{})==Target.value("Guid",std::string{})&&value->value("Player",std::string{})==Target.value("Path",std::string{})&&value->value("Container",false)==Container&&value->value("Personal",false)==Personal)Snapshot=*value;}
    if(Pending&&std::chrono::steady_clock::now()-Started>std::chrono::seconds(15)){Pending=0;Status="Inventory read timed out; refresh to retry.";}
    if(Refresh&&!Pending){const auto id=++Sequence;if(SpawnToolRequests::TrySubmit({{"Action",Container?"InspectContainerInventory":"InspectPlayerInventory"},{"_RequestId",id},{"_Source","HelpyStandalone"},{"Player",Target.value("Path",std::string{})},{"PlayerGuid",Target.value("Guid",std::string{})},{"Generation",Generation},{"ObjectIndex",Target.value("ObjectIndex",-1)},{"ObjectSerial",Target.value("ObjectSerial",0)},{"Personal",Personal},{"Offset",Offset}})){Pending=id;Refresh=false;Started=std::chrono::steady_clock::now();}}
}
inline void Contents(){
    const auto& t=HelpyImGuiTheme::Current;
    if(Container){const float side=t.Y(48);const auto at=ImGui::GetCursorScreenPos();ImGui::Dummy({side,side});
        auto icon=ItemPresentation::Text(Snapshot,"Icon");if(icon.empty())icon=ItemPresentation::Text(Target,"Icon");
        if(!ItemIconCache::Draw(icon,at,{at.x+side,at.y+side},true))MapLayerIcons::Draw("chests",{at.x+side*.5f,at.y+side*.5f},side*.8f);
        ImGui::SameLine();ImGui::BeginGroup();ImGui::TextWrapped("%s",Target.value("Name",std::string("Placed storage")).c_str());
        ImGui::TextDisabled("Loaded storage | Host/server inspection");ImGui::EndGroup();
    }else ImGui::TextWrapped("%s",Target.value("Name",std::string("Player")).c_str());
    if(ImGui::Button(Detached?"Dock inventory":"Detach inventory"))Detached=!Detached;
    ImGui::BeginDisabled(Pending!=0);if(!Container){if(ImGui::Button("Inventory")){Personal=false;Offset=0;Snapshot=Json::object();Refresh=true;}ImGui::SameLine();if(ImGui::Button("Personal chest")){Personal=true;Offset=0;Snapshot=Json::object();Refresh=true;}ImGui::SameLine();}
    if(ImGui::Button("Refresh"))Refresh=true;ImGui::EndDisabled();
    ImGui::TextWrapped("%s",Status.c_str());ImGui::SeparatorText(Container?"Placed chest / storage":Personal?"Personal chest":"Inventory");
    ImGui::TextWrapped("%s",Container?"Read-only loaded-storage snapshot. Unloaded, replaced or ambiguous inventories are unavailable, never assumed empty.":"Host/server read-only snapshot. Personal chest reads the player's native personal-inventory component, not another nearby chest.");
    if(Container){ImGui::TextUnformatted("Lock status: Unknown");ImGui::TextUnformatted("Locked by: Unknown / not verified");
        ImGui::BeginDisabled();ImGui::Button("Add items directly");ImGui::SameLine();ImGui::Button("Override unlock");ImGui::EndDisabled();
        ImGui::TextWrapped("%s",Snapshot.value("AccessReason",std::string("Lock ownership, direct deposits and unlock overrides require a verified native access contract; no access is assumed.")).c_str());}
    if(Snapshot.contains("ObservedUnixMs"))ImGui::TextDisabled("Snapshot: %zu occupied slots on this page | %d total slots",Snapshot.value("Items",Json::array()).size(),Snapshot.value("Slots",0));
    ImGui::BeginDisabled(!Snapshot.contains("ObservedUnixMs")||Generation!=SpawnToolRequests::CurrentGeneration());if(!Container&&!Personal&&ImGui::Button("Add through Item Service"))AddWanted=true;ImGui::EndDisabled();
    ImGui::TextDisabled("Deletion and direct chest edits are not enabled until native persistence behavior is verified.");
    if(ImGui::BeginChild("##inventory-rows",{0,std::max(t.Y(80),ImGui::GetContentRegionAvail().y-t.Y(42))},true)){
        for(const auto& row:Snapshot.value("Items",Json::array())){ImGui::PushID(row.value("Slot",0));const float side=t.Y(34);const auto a=ImGui::GetCursorScreenPos();ImGui::Dummy({side,side});ItemIconCache::Draw(ItemPresentation::Text(row,"Icon"),a,{a.x+side,a.y+side},true);ImGui::SameLine();ImGui::BeginGroup();ImGui::TextWrapped("%s",row.value("Name",row.value("Path",std::string("Native item"))).c_str());
            if(row.contains("Count"))ImGui::Text("Stack: %d | Slot: %d",row.value("Count",0),row.value("Slot",0));if(row.contains("Reason"))ImGui::TextWrapped("%s",row.value("Reason",std::string{}).c_str());ImGui::EndGroup();ImGui::Separator();ImGui::PopID();}
        if(Snapshot.contains("Items")&&Snapshot["Items"].empty())ImGui::TextDisabled("No occupied slots in this page.");
    }ImGui::EndChild();
    ImGui::BeginDisabled(Pending!=0);if(ImGui::Button("Previous")&&Offset>0){Offset=std::max(0,Offset-32);Snapshot=Json::object();Refresh=true;}ImGui::SameLine();if(ImGui::Button("Next")&&Snapshot.value("HasMore",false)){Offset=Snapshot.value("NextOffset",Offset+32);Snapshot=Json::object();Refresh=true;}ImGui::EndDisabled();
}
inline void RenderDocked(){if(!Open||Detached||HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::Inventory))return;HelpyWindowChrome::Prepare(Chrome);ImGui::SetNextWindowSize({620,560},ImGuiCond_FirstUseEver);if(ImGui::Begin("Inventory inspector",&Open,ImGuiWindowFlags_NoSavedSettings)){HelpyWindowChrome::Render(Chrome,HelpyTaskbar::Window::Inventory);Contents();}ImGui::End();}
}
