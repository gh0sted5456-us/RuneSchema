#pragma once
#include "Loader/ItemIdentity.h"
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <stdexcept>

namespace DragonWilds::Quests {
using SaveJson=nlohmann::json;
inline constexpr int OwnershipVersion=7001;
inline void ValidateOwner(const std::string& owner) {
    if(owner.empty() || owner.size()>128 || owner=="." || owner==".." || owner.back()=='.' || owner.back()==' ')throw std::runtime_error("Invalid quest owner");
    for(unsigned char c:owner)if(c<32 || c==':' || c=='/' || c=='\\')throw std::runtime_error("Invalid quest owner");
}
// These exact native QuestInts markers travel with the player save, not runtime files.
inline SaveJson OwnershipVariables(const std::string& owner,const std::string& id) {
    ValidateOwner(owner);
    if(!IsCanonicalPersistenceId(id))throw std::runtime_error("Invalid owned quest identity");
    return SaveJson::array({
        {{"QuestVariableName","RuneSchema.Owner:"+owner},{"QuestVariableValue",OwnershipVersion}},
        {{"QuestVariableName","RuneSchema.Identity:"+id},{"QuestVariableValue",OwnershipVersion}}
    });
}
struct OwnershipInfo {std::set<std::string> Owners;bool Identity=false;};
inline OwnershipInfo InspectOwnership(const SaveJson& row) {
    OwnershipInfo result;
    if(!row.is_object() || !row.contains("QuestId") || !row.at("QuestId").is_string() || !row.contains("QuestInts") || !row.at("QuestInts").is_array())return {};
    const auto id=row.at("QuestId").get<std::string>();
    for(const auto& variable:row.at("QuestInts")) {
        if(!variable.is_object() || !variable.contains("QuestVariableName") || !variable.at("QuestVariableName").is_string())continue;
        const auto name=variable.at("QuestVariableName").get<std::string>();
        if(!name.starts_with("RuneSchema.Owner:") && !name.starts_with("RuneSchema.Identity:"))continue;
        if(variable.value("QuestVariableValue",SaveJson{})!=OwnershipVersion)throw std::runtime_error("Unsupported quest ownership version for QuestId='"+id+"'; cleanup refused");
        if(name.starts_with("RuneSchema.Owner:")) {
            const auto owner=name.substr(17);ValidateOwner(owner);result.Owners.insert(owner);
        } else {
            if(name!="RuneSchema.Identity:"+id || !IsCanonicalPersistenceId(id))throw std::runtime_error("Quest ownership identity mismatch for QuestId='"+id+"'");
            result.Identity=true;
        }
    }
    if(result.Owners.empty()!=!result.Identity)throw std::runtime_error("Incomplete quest ownership markers for QuestId='"+id+"'");
    return result;
}
inline std::string OwnedBy(const SaveJson& row) {
    const auto info=InspectOwnership(row);
    if(info.Owners.size()>1){
        std::string owners;for(const auto& owner:info.Owners){if(!owners.empty())owners+=", ";owners+="'"+owner+"'";}
        throw std::runtime_error("Quest ownership conflict: QuestId='"+row.at("QuestId").get<std::string>()
            +"' owners=["+owners+"]. Old-owner markers require a uniquely verified current quest declaration; progress retained.");
    }
    return info.Identity?*info.Owners.begin():std::string{};
}
// Called only with the uniquely registered current owner. Never choose an
// owner from array order or mod load order; never reset progress or receipts.
inline SaveJson NormalizeOwnership(const SaveJson& row,const std::string& currentOwner,bool verified) {
    if(!verified)throw std::runtime_error("Quest ownership migration requires a uniquely verified current declaration");
    ValidateOwner(currentOwner);
    const auto id=row.at("QuestId").get<std::string>();
    const auto canonical=OwnershipVariables(currentOwner,id);
    (void)InspectOwnership(row);
    auto result=row;auto retained=SaveJson::array();
    for(const auto& variable:row.at("QuestInts")){
        const auto name=variable.is_object()?variable.value("QuestVariableName",SaveJson{}):SaveJson{};
        if(name.is_string()&&(name.get_ref<const std::string&>().starts_with("RuneSchema.Owner:")
            ||name.get_ref<const std::string&>().starts_with("RuneSchema.Identity:")))continue;
        retained.push_back(variable);
    }
    for(const auto& variable:canonical)retained.push_back(variable);
    result["QuestInts"]=std::move(retained);return result;
}
struct CleanupResult {SaveJson Save;std::set<std::string> RemovedQuestIds;};
// Pure transformation only. Caller must establish a complete successful mod-directory scan,
// distinguish absent from disabled/failed mods, and arrange a native-safe save boundary.
inline CleanupResult CleanRemovedQuests(const SaveJson& original,const std::set<std::string>& confirmedAbsent,bool scanComplete) {
    if(!scanComplete)throw std::runtime_error("Incomplete mod scan; quest cleanup refused");
    for(const auto& owner:confirmedAbsent)ValidateOwner(owner);
    CleanupResult result{original,{}};
    auto& progress=result.Save.at("GameProgress").at("QuestProgress");
    auto& rows=progress.at("Quests");
    if(!rows.is_array() || rows.size()>4096)throw std::runtime_error("Unsupported quest save array");
    std::set<std::string> seen;
    for(const auto& row:rows) {
        if(row.is_object() && row.contains("QuestId") && row.at("QuestId").is_string())
            if(!seen.insert(row.at("QuestId").get<std::string>()).second)throw std::runtime_error("Duplicate quest identity; cleanup refused");
        const auto owner=OwnedBy(row);
        if(owner.empty())continue;
        const auto id=row.at("QuestId").get<std::string>();
        if(confirmedAbsent.contains(owner))result.RemovedQuestIds.insert(id);
    }
    if(result.RemovedQuestIds.empty())return result;
    for(auto it=rows.begin();it!=rows.end();) {
        if(it->is_object() && it->contains("QuestId") && it->at("QuestId").is_string() && result.RemovedQuestIds.contains(it->at("QuestId").get<std::string>()))it=rows.erase(it);
        else ++it;
    }
    if(progress.contains("QuestTracked") && progress.at("QuestTracked").is_string() && result.RemovedQuestIds.contains(progress.at("QuestTracked").get<std::string>()))progress["QuestTracked"]="";
    // Location records require their own explicit saved ownership manifest; never delete by prefix.
    return result;
}
}
