#pragma once
#include "Loader/QuestSaveOwnership.h"
#include "Loader/JournalSaveOwnership.h"
#include "Core/SaveRegistrySnapshot.h"
#include <cctype>
#include <map>
#include <unordered_map>

namespace PS::SaveCleanup {
using Json=nlohmann::json;
struct Preview {
    Json Save;
    Json Removed=Json::array();
    std::map<std::string,size_t> Owners;
};
inline std::string OwnerKey(std::string value) {
    for(auto& character:value)character=static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return value;
}
inline std::set<std::string> AbsentOwners(const std::map<std::string,size_t>& owners,const std::set<std::string>& active) {
    std::set<std::string> keys,absent;
    for(const auto& owner:active)keys.insert(OwnerKey(owner));
    for(const auto& [owner,count]:owners)if(!keys.contains(OwnerKey(owner)))absent.insert(owner);
    return absent;
}
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
inline Preview Plan(const Json& source,const std::set<std::string>& requested,
    bool eraseProgress=false,const RegistrySnapshot* registry=nullptr,
    bool removePendingOwned=false,bool pruneRegistryProgress=false) {
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
                if(!item.is_object() || (item.contains("ItemData") && !item.at("ItemData").is_string())) {
                    result.Removed.push_back({{"Kind",section},{"Id",it.key()},{"Slot",it.key()},{"Mod","Malformed native record"}});
                    it=entries.erase(it);continue;
                }
                const auto id=item.value("ItemData",std::string{});
                bool remove=!id.empty() && !registry->Items.contains(id);
                if(std::string_view(section)=="Loadout" && item.contains("PlayerInventoryItemIndex")) {
                    if(!item.at("PlayerInventoryItemIndex").is_number_integer()) {
                        result.Removed.push_back({{"Kind",section},{"Id",it.key()},{"Slot",it.key()},{"Mod","Malformed equipped inventory index"}});
                        it=entries.erase(it);continue;
                    }
                    remove=remove || removedSlots.contains(std::to_string(item.at("PlayerInventoryItemIndex").get<int>()));
                }
                if(remove) {
                    if(std::string_view(section)=="Inventory")removedSlots.insert(it.key());
                    result.Removed.push_back({{"Kind",section},{"Id",id.empty()?it.key():id},{"Slot",it.key()},{"Mod","Registry-unknown (owner unavailable)"}});
                    it=entries.erase(it);
                }else ++it;
            }
        }
        if((eraseProgress || pruneRegistryProgress) && game.contains("Progress")) {
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
            if(!row.is_object() || !row.contains("QuestId") || !row.at("QuestId").is_string()) {
                retained.push_back(row);continue;
            }
            const auto id=row.at("QuestId").get<std::string>();
            if(!ids.insert(id).second) {
                retained.push_back(row);continue;
            }
            const auto owner=DragonWilds::Quests::OwnedBy(row);
            if(!owner.empty())++result.Owners[owner];
            const bool registryOrphan=registry && registry->QuestsComplete && !registry->Quests.contains(id);
            if(!registryOrphan && (owner.empty() || !selected.contains(owner))){retained.push_back(row);continue;}
            if(!removePendingOwned)CheckPending(row);
            removed.insert(id);
            result.Removed.push_back({{"Kind","Quest/dialogue"},{"Id",id},{"Mod",owner.empty()?"Registry-unknown (owner unavailable)":owner}});
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

// Automatic cleanup is deliberately narrower than the diagnostic registry
// repair above. It accepts only historical RuneSchema identities whose owner
// has been confirmed absent or explicitly disabled. Unknown third-party and
// vanilla records are never inferred to be removable.
inline Preview PlanOwned(const Json& source,
    const std::unordered_map<std::string,std::string>& retiredItems,
    const std::unordered_map<std::string,std::string>& retiredRecipes,
    const std::set<std::string>& retiredOwners) {
    auto result=Plan(source,retiredOwners,true,nullptr,true);
    auto& game=result.Save.at("GameProgress");
    std::set<std::string> removedInventorySlots;
    const auto record=[&](const char* kind,const std::string& id,
        const std::string& slot,const std::string& owner) {
        Json row={{"Kind",kind},{"Id",id},{"Mod",owner}};
        if(!slot.empty())row["Slot"]=slot;
        result.Removed.push_back(std::move(row));
        ++result.Owners[owner];
    };
    for(const auto* section:{"Inventory","PersonalInventory"}) {
        if(!game.contains(section))continue;
        auto& entries=game.at(section);
        if(!entries.is_object())continue;
        for(auto it=entries.begin();it!=entries.end();) {
            const auto& item=it.value();
            if(!item.is_object() || !item.contains("ItemData")
                || !item.at("ItemData").is_string()) {++it;continue;}
            const auto id=item.at("ItemData").get<std::string>();
            const auto owned=retiredItems.find(id);
            if(owned==retiredItems.end()) {++it;continue;}
            if(std::string_view(section)=="Inventory")removedInventorySlots.insert(it.key());
            record(section,id,it.key(),owned->second);
            it=entries.erase(it);
        }
    }
    if(game.contains("Loadout") && game.at("Loadout").is_object()) {
        auto& entries=game.at("Loadout");
        for(auto it=entries.begin();it!=entries.end();) {
            const auto& item=it.value();
            std::string id,owner;
            bool remove=false;
            if(item.is_object() && item.contains("ItemData")
                && item.at("ItemData").is_string()) {
                id=item.at("ItemData").get<std::string>();
                const auto owned=retiredItems.find(id);
                if(owned!=retiredItems.end()) {remove=true;owner=owned->second;}
            }
            if(!remove && item.is_object()
                && item.contains("PlayerInventoryItemIndex")
                && item.at("PlayerInventoryItemIndex").is_number_integer()) {
                const auto slot=std::to_string(
                    item.at("PlayerInventoryItemIndex").get<int>());
                if(removedInventorySlots.contains(slot)) {
                    remove=true;id=slot;
                    // The referenced inventory record was already removed and
                    // carries the same confirmed ownership. Recover it from
                    // the removal report without guessing from the loadout.
                    for(auto row=result.Removed.rbegin();row!=result.Removed.rend();++row)
                        if(row->value("Kind",std::string{})=="Inventory"
                            && row->value("Slot",std::string{})==slot) {
                            owner=row->value("Mod",std::string{});break;
                        }
                }
            }
            if(!remove) {++it;continue;}
            record("Loadout",id,it.key(),owner);
            it=entries.erase(it);
        }
    }
    if(game.contains("Progress") && game.at("Progress").is_object()) {
        auto& progress=game.at("Progress");
        for(const auto* field:{"ItemsPickedUp","MilestoneMaterialsPickedUp",
            "RecipesUnlocked","RecipesNew"}) {
            if(!progress.contains(field) || !progress.at(field).is_array())continue;
            auto& entries=progress.at(field);
            const auto& owned=std::string_view(field).starts_with("Recipes")
                ?retiredRecipes:retiredItems;
            for(auto it=entries.begin();it!=entries.end();) {
                if(!it->is_string()) {++it;continue;}
                const auto id=it->get<std::string>();
                const auto found=owned.find(id);
                if(found==owned.end()) {++it;continue;}
                record(field,id,{},found->second);
                it=entries.erase(it);
            }
        }
    }
    return result;
}
}
