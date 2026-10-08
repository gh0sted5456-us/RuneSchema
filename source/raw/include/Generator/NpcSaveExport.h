#pragma once
#include "Loader/HumanNpc.h"
#include "Generator/SaveReport.h"
#include <set>
namespace PS::NpcSaveExport {
using Json=nlohmann::json;
inline Json Preview(const Json& save) {
    if(!save.is_object() || !save.contains("GameProgress"))throw std::runtime_error("Select a native character JSON save");
    const auto name=save.at("meta_data").at("char_name").get<std::string>();
    if(name.empty() || name.size()>256)throw std::runtime_error("Saved character name is invalid");
    Json npc={{"Id","imported_human"},{"Type","Human"},{"Enabled",false},{"NoInteract",true},{"HideName",false},
        {"DisplayName",name},{"Location",Json::array({0,0,"$"})},
        {"Rotation",{{"Pitch",0},{"Yaw",0},{"Roll",0}}},{"Appearance",Json::object()},{"Equipment",Json::object()}};
    const auto& customization=save.at("Customization").at("CustomizationData");
    for(const auto* key:DragonWilds::HumanNpc::AppearanceKeys)npc["Appearance"][key]=customization.at(key).at("rowName");
    DragonWilds::HumanNpc::Validate(npc);
    Json rows=Json::array(),warnings=Json::array();
    const auto& game=save.at("GameProgress");
    const auto loadout=game.value("Loadout",Json::object());
    if(!loadout.is_object() || loadout.size()>64)throw std::runtime_error("Unsupported saved loadout");
    for(const auto& [slot,value]:loadout.items()) {
        if(slot=="MaxSlotIndex")continue;
        auto item=value;
        if(item.is_object() && item.contains("PlayerInventoryItemIndex")) {
            const auto index=item.at("PlayerInventoryItemIndex").get<int>();
            if(index<0 || !game.contains("Inventory") || !game.at("Inventory").contains(std::to_string(index))) {
                warnings.push_back("Loadout "+slot+": missing inventory reference");continue;
            }
            item=game.at("Inventory").at(std::to_string(index));
        }
        if(!item.is_object() || !item.contains("ItemData") || !item.at("ItemData").is_string()) {
            warnings.push_back("Loadout "+slot+": unsupported item record");continue;
        }
        rows.push_back({{"Location",slot},{"PersistenceID",item.at("ItemData")},{"AssetPath",nullptr},{"Slot",""}});
    }
    return {{"NPC",npc},{"Rows",rows},{"Warnings",warnings}};
}
inline void ApplyResolved(Json& preview) {
    auto& npc=preview.at("NPC");auto& warnings=preview.at("Warnings");
    std::set<std::string> ambiguous;
    for(const auto& row:preview.at("Rows")) {
        auto slot=row.value("Slot",std::string{});
        if(DragonWilds::HumanNpc::IsMainHandSlot(slot))slot="MainHand";
        else if(DragonWilds::HumanNpc::IsOffHandSlot(slot))slot="OffHand";
        bool allowed=false;for(const auto* key:DragonWilds::HumanNpc::EquipmentKeys)if(slot==key)allowed=true;
        if(!allowed || !row.at("AssetPath").is_string()) {
            warnings.push_back("Loadout "+row.at("Location").get<std::string>()+": unresolved or unsupported equipment ("+row.at("PersistenceID").get<std::string>()+")");continue;
        }
        const auto path=row.at("AssetPath").get<std::string>();
        auto& equipment=npc["Equipment"];
        if(ambiguous.contains(slot))continue;
        if(equipment.contains(slot) && equipment.at(slot)!=path) {
            equipment.erase(slot);ambiguous.insert(slot);warnings.push_back(slot+": multiple equipped candidates; select its item path manually");continue;
        }
        equipment[slot]=path;
    }
    DragonWilds::HumanNpc::Validate(npc);
}
}
