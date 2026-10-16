#pragma once
#include "Loader/DialogueDefinition.h"
#include "Loader/HumanNpc.h"
#include "Generator/SaveReport.h"
#include "Core/ConfigFiles.h"
#include <filesystem>

namespace DragonWilds::NpcEquipment {
using Json=nlohmann::json;
struct Rule {std::string Id,Quest;Json Equipment;};
inline bool RuleId(const std::string& id) {
    return !id.empty() && id.size()<=64 && id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==id.npos;
}
inline std::vector<Rule> Parse(const std::string& mod,const Json& data) {
    if(!data.contains("EquipmentWhen"))return {};
    if(!HumanNpc::IsHuman(data))throw std::runtime_error("EquipmentWhen requires Type Human");
    const auto& entries=data.at("EquipmentWhen");
    if(!entries.is_array() || entries.empty() || entries.size()>16)throw std::runtime_error("EquipmentWhen requires 1..16 rules");
    std::set<std::string> ids,slots;std::vector<Rule> rules;
    for(const auto& entry:entries) {
        Dialogue::Fields(entry,{"Id","QuestCompleted","Policy","Equipment"});
        Rule rule{Dialogue::Text(entry.at("Id"),64),Dialogue::Reference(mod,entry.at("QuestCompleted")),entry.at("Equipment")};
        if(!RuleId(rule.Id) || !ids.insert(rule.Id).second)throw std::runtime_error("EquipmentWhen rule IDs must be unique letters, digits, underscores or hyphens");
        if(entry.at("Policy")!="WorldEverCompleted")throw std::runtime_error("EquipmentWhen Policy must explicitly be WorldEverCompleted");
        if(!rule.Equipment.is_object() || rule.Equipment.empty())throw std::runtime_error("EquipmentWhen requires a nonempty wearable slot map");
        for(const auto& [slot,value]:rule.Equipment.items()) {
            const auto canonical=std::string(HumanNpc::CanonicalEquipmentSlot(slot));
            bool known=false;for(const auto* key:HumanNpc::EquipmentKeys)known|=slot==key;
            if(!known || slot=="MainHand" || slot=="OffHand" || !slots.insert(canonical).second)
                throw std::runtime_error("EquipmentWhen supports nonoverlapping wearable slots only");
            const auto path=Dialogue::Text(value,1024);
            if(!IsCanonicalPersistenceId(path) && (path.front()!='/' || path.find_first_of("\r\n\t")!=path.npos))
                throw std::runtime_error("EquipmentWhen requires item asset paths or canonical PersistenceIDs");
        }
        rules.push_back(std::move(rule));
    }
    return rules;
}
inline std::string Key(const std::string& npc,const Rule& rule) {return npc+":"+rule.Id;}
inline Json Read(const std::filesystem::path& path,const std::string& world) {
    if(PS::SaveReport::Guid(world)!=world)throw std::runtime_error("NPC equipment requires a stable world-save GUID");
    if(!std::filesystem::exists(path))return {{"Version",1},{"WorldSaveGuid",world},{"Rules",Json::object()}};
    auto data=Json::parse(PS::ConfigFiles::Read(path,1024*1024));
    Dialogue::Fields(data,{"Version","WorldSaveGuid","Rules"});
    if(data.at("Version")!=1 || data.at("WorldSaveGuid")!=world || !data.at("Rules").is_object() || data["Rules"].size()>4096)
        throw std::runtime_error("NPC equipment world ledger is mismatched or unsupported; original preserved");
    for(const auto& [key,value]:data["Rules"].items()) {
        if(key.empty() || key.size()>512 || key.find_first_of("\r\n\t")!=key.npos)throw std::runtime_error("Invalid NPC equipment ledger identity");
        Dialogue::Reference("_",value);
    }
    return data;
}
inline bool Latched(const Json& data,const std::string& npc,const Rule& rule) {
    const auto found=data.at("Rules").find(Key(npc,rule));
    if(found==data.at("Rules").end())return false;
    if(*found!=rule.Quest)throw std::runtime_error("NPC equipment rule was rebound to a different quest; use a new rule Id");
    return true;
}
inline Json Latch(const std::filesystem::path& path,const std::string& world,const std::string& npc,const Rule& rule) {
    auto data=Read(path,world);
    if(Latched(data,npc,rule))return data;
    if(data["Rules"].size()>=4096)throw std::runtime_error("NPC equipment world ledger is full");
    data["Rules"][Key(npc,rule)]=rule.Quest;
    PS::ConfigFiles::Write(path,data.dump(2));
    const auto confirmed=Read(path,world);
    if(!Latched(confirmed,npc,rule))throw std::runtime_error("NPC equipment world ledger write was not confirmed");
    return confirmed;
}
inline Json Overlay(Json equipment,const std::vector<Rule>& rules,const std::vector<std::string>& active) {
    std::set<std::string> seen;
    for(const auto& id:active) {
        if(!seen.insert(id).second)throw std::runtime_error("Duplicate NPC equipment rule in replicated state");
        const auto rule=std::find_if(rules.begin(),rules.end(),[&](const auto& value){return value.Id==id;});
        if(rule==rules.end())throw std::runtime_error("Unknown NPC equipment rule in replicated state");
        for(const auto& [slot,item]:rule->Equipment.items()) {
            if(slot=="Trinket2")equipment.erase("Necklace");
            if(slot=="Necklace")equipment.erase("Trinket2");
            equipment[slot]=item;
        }
    }
    return equipment;
}
}
