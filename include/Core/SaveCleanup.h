#pragma once
#include "Loader/QuestSaveOwnership.h"
#include "Loader/JournalSaveOwnership.h"
#include "Core/SaveRegistrySnapshot.h"
#include <map>

namespace PS::SaveCleanup {
using Json=nlohmann::json;
struct Preview {
    Json Save;
    Json Removed=Json::array();
    std::map<std::string,size_t> Owners;
};
inline void RequireCharacter(const Json& save) {
    if(!save.is_object() || !save.contains("GameProgress") || !save.at("GameProgress").is_object())
        throw std::runtime_error("Only native character JSON saves are supported; world/SPUD saves require their own adapter");
}
inline Json JournalPayload(const Json& journal) {
    if(!journal.is_object())throw std::runtime_error("Unsupported journal save layout");
    auto result=journal;
    if(journal.contains(DragonWilds::JournalSave::Manifest)) {
        const auto& envelope=journal.at(DragonWilds::JournalSave::Manifest);
        if(!envelope.is_array())throw std::runtime_error("Unsupported native journal ownership envelope");
        const auto owners=DragonWilds::JournalSave::DecodeNative(envelope.get<std::vector<std::string>>());
        result.erase(DragonWilds::JournalSave::Manifest);
        DragonWilds::JournalSave::StoreOwnership(result,owners);
    }
    return result;
}
inline void CheckPending(const Json& row) {
    for(const auto& variable:row.at("QuestInts")) {
        if(!variable.is_object() || !variable.contains("QuestVariableName") || !variable.at("QuestVariableName").is_string())continue;
        const auto& name=variable.at("QuestVariableName").get_ref<const std::string&>();
        const auto value=variable.value("QuestVariableValue",Json{});
        if((name=="RuneSchema.Phase" && (value==2 || value==3)) || (name.starts_with("RuneSchema.Flag:") && value==1))
            throw std::runtime_error("Selected mod has an unfinished item/reward exchange; resolve it before cleanup");
    }
}
// Pure transformation: never reads/writes a file or queries a partially loaded registry.
// Installed and absent owners are explicit selections; neither is inferred from an asset prefix.
inline Preview Plan(const Json& source,const std::set<std::string>& requested,bool eraseProgress=false,const RegistrySnapshot* registry=nullptr) {
    RequireCharacter(source);
    for(const auto& owner:requested)DragonWilds::Quests::ValidateOwner(owner);
    const std::set<std::string> selected=eraseProgress?requested:std::set<std::string>{};
    Preview result{source};
    auto& game=result.Save.at("GameProgress");
    if(registry) {
        if(!registry->Ready())throw std::runtime_error("Item/recipe registries are incomplete; unknown-ID cleanup refused");
        std::set<std::string> removedSlots;
        for(const auto* section:{"Inventory","PersonalInventory","Loadout"}) {
            if(!game.contains(section))continue;
            auto& entries=game.at(section);
            if(!entries.is_object())throw std::runtime_error("Unsupported inventory/loadout save layout");
            for(auto it=entries.begin();it!=entries.end();) {
                const auto& item=it.value();
                if(!item.is_object())throw std::runtime_error("Malformed inventory/loadout record");
                if(item.contains("ItemData") && !item.at("ItemData").is_string())throw std::runtime_error("Malformed inventory item identity");
                const auto id=item.value("ItemData",std::string{});
                bool remove=!id.empty() && !registry->Items.contains(id);
                if(std::string_view(section)=="Loadout" && item.contains("PlayerInventoryItemIndex")) {
                    if(!item.at("PlayerInventoryItemIndex").is_number_integer())throw std::runtime_error("Malformed equipped inventory index");
                    remove=remove || removedSlots.contains(std::to_string(item.at("PlayerInventoryItemIndex").get<int>()));
                }
                if(remove) {
                    if(std::string_view(section)=="Inventory")removedSlots.insert(it.key());
                    result.Removed.push_back({{"Kind",section},{"Id",id.empty()?it.key():id},{"Slot",it.key()},{"Mod","Registry-unknown (owner unavailable)"}});
                    it=entries.erase(it);
                }else ++it;
            }
        }
        if(eraseProgress && game.contains("Progress")) {
            auto& progress=game.at("Progress");
            if(!progress.is_object())throw std::runtime_error("Unsupported item/recipe progress layout");
            for(const auto* field:{"ItemsPickedUp","MilestoneMaterialsPickedUp","RecipesUnlocked","RecipesNew"}) {
                if(!progress.contains(field))continue;
                auto& entries=progress.at(field);
                if(!entries.is_array())throw std::runtime_error("Unsupported item/recipe unlock list");
                const auto& known=std::string_view(field).starts_with("Recipes")?registry->Recipes:registry->Items;
                for(auto it=entries.begin();it!=entries.end();) {
                    if(!it->is_string())throw std::runtime_error("Malformed item/recipe unlock identity");
                    if(!known.contains(it->get<std::string>())) {
                        result.Removed.push_back({{"Kind",field},{"Id",*it},{"Mod","Registry-unknown (owner unavailable)"}});it=entries.erase(it);
                    }else ++it;
                }
            }
        }
    }
    if(game.contains("QuestProgress")) {
        auto& progress=game.at("QuestProgress");
        if(!progress.is_object() || !progress.contains("Quests") || !progress.at("Quests").is_array() || progress.at("Quests").size()>4096)
            throw std::runtime_error("Unsupported quest save layout");
        std::set<std::string> ids,removed,locations;
        auto retained=Json::array();
        for(const auto& row:progress.at("Quests")) {
            if(!row.is_object() || !row.contains("QuestId") || !row.at("QuestId").is_string())throw std::runtime_error("Malformed quest save record");
            const auto id=row.at("QuestId").get<std::string>();
            if(!ids.insert(id).second)throw std::runtime_error("Duplicate saved quest identity");
            const auto owner=DragonWilds::Quests::OwnedBy(row);
            if(!owner.empty())++result.Owners[owner];
            if(owner.empty() || !selected.contains(owner)){retained.push_back(row);continue;}
            CheckPending(row);
            removed.insert(id);
            result.Removed.push_back({{"Kind","Quest/dialogue"},{"Id",id},{"Mod",owner}});
            for(const auto& variable:row.at("QuestInts")) {
                const auto name=variable.value("QuestVariableName",std::string{});
                if(name.starts_with("RuneSchema.Location:") && variable.value("QuestVariableValue",Json{})==DragonWilds::Quests::OwnershipVersion)
                    locations.insert(name.substr(20));
            }
        }
        progress["Quests"]=std::move(retained);
        if(progress.contains("QuestTracked")) {
            if(!progress.at("QuestTracked").is_string())throw std::runtime_error("Unsupported tracked quest identity");
            if(removed.contains(progress.at("QuestTracked").get<std::string>()))progress["QuestTracked"]="";
        }
        if(progress.contains("QuestLocations") && !locations.empty()) {
            if(!progress.at("QuestLocations").is_array())throw std::runtime_error("Unsupported quest location save layout");
            auto kept=Json::array();
            for(const auto& row:progress.at("QuestLocations")) {
                if(!row.is_object() || !row.contains("QuestLocationId") || !row.at("QuestLocationId").is_string())throw std::runtime_error("Unsupported quest location record");
                const auto id=row.at("QuestLocationId").get<std::string>();
                if(locations.contains(id))result.Removed.push_back({{"Kind","Quest location"},{"Id",id}});
                else kept.push_back(row);
            }
            progress["QuestLocations"]=std::move(kept);
        }
    }
    if(game.contains("Journal")) {
        auto payload=JournalPayload(game.at("Journal"));
        const auto owners=DragonWilds::JournalSave::ReadOwnership(payload);
        for(const auto& [id,owner]:owners)++result.Owners[owner];
        const auto cleaned=DragonWilds::JournalSave::RemoveAbsent(payload,selected,true);
        auto native=cleaned.Journal;
        if(game.at("Journal").contains(DragonWilds::JournalSave::Manifest))
            native[DragonWilds::JournalSave::Manifest]=DragonWilds::JournalSave::EncodeNative(DragonWilds::JournalSave::ReadOwnership(cleaned.Journal));
        for(const auto& id:cleaned.Removed)result.Removed.push_back({{"Kind","Journal/lore"},{"Id",id},{"Mod",owners.at(id)}});
        game["Journal"]=std::move(native);
    }
    return result;
}
}
