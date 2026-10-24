#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <stdexcept>
#include <vector>
#include <set>
namespace DragonWilds::NpcIdentity {
inline constexpr wchar_t ClassPath[]=L"/RuneSchema/Networking/BPC_RuneSchemaIdentity.BPC_RuneSchemaIdentity_C";
inline constexpr wchar_t LegacyClassPath[]=L"/Game/Mods/RuneSchema/Networking/BPC_RuneSchemaIdentity.BPC_RuneSchemaIdentity_C";
inline constexpr size_t MaxPayload=4096;
struct Payload { std::string Mod,Npc,Fingerprint; uint64_t CueRevision=0; nlohmann::json Cue; std::vector<std::string> EquipmentRules; };
inline Payload Decode(const std::string& text) {
    if(text.empty() || text.size()>MaxPayload)throw std::runtime_error("NPC identity payload length invalid");
    const auto data=nlohmann::json::parse(text);
    if(!data.is_object() || !data.contains("version")
        || !data["version"].is_number_integer() || (data["version"]!=1 && data["version"]!=2 && data["version"]!=3))
        throw std::runtime_error("NPC identity protocol unsupported");
    const auto expectedFields=data["version"]==1 ? 4u : data["version"]==2?6u:7u;
    if(data.size()!=expectedFields)throw std::runtime_error("NPC identity envelope contains unsupported fields");
    const auto read=[&](const char* key) {
        if(!data.contains(key) || !data[key].is_string())throw std::runtime_error("NPC identity field missing or not a string");
        auto value=data[key].get<std::string>();
        if(value.empty() || value.size()>256)throw std::runtime_error("NPC identity field length invalid");
        for(unsigned char c:value)if(c<32 || c==127)throw std::runtime_error("NPC identity contains control characters");
        return value;
    };
    Payload result{read("mod"),read("npc"),read("fingerprint")};
    if(data["version"]!=1) {
        if(!data.contains("cueRevision") || !data["cueRevision"].is_number_unsigned()
            || !data.contains("cue") || !data["cue"].is_object())throw std::runtime_error("NPC visual cue envelope is invalid");
        result.CueRevision=data["cueRevision"].get<uint64_t>();result.Cue=data["cue"];
    }
    if(data["version"]==3) {
        const auto& rules=data.at("equipmentRules");
        if(!rules.is_array() || rules.size()>16)throw std::runtime_error("Invalid NPC equipment rules envelope");
        std::set<std::string> seen;
        for(const auto& value:rules) {
            if(!value.is_string())throw std::runtime_error("NPC equipment rule identity must be text");
            const auto id=value.get<std::string>();
            if(id.empty() || id.size()>64 || id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=id.npos || !seen.insert(id).second)
                throw std::runtime_error("Invalid or duplicate NPC equipment rule identity");
            result.EquipmentRules.push_back(id);
        }
    }
    return result;
}
inline std::string Encode(const std::string& mod,const std::string& npc,const std::string& fingerprint) {
    const auto text=nlohmann::json{{"version",1},{"mod",mod},{"npc",npc},{"fingerprint",fingerprint}}.dump();
    (void)Decode(text);return text;
}
inline std::string WithEquipment(const std::string& text,const std::vector<std::string>& rules) {
    const auto prior=Decode(text);auto data=nlohmann::json::parse(text);
    data["version"]=3;data["equipmentRules"]=rules;
    data["cueRevision"]=prior.CueRevision;data["cue"]=prior.Cue.is_object()?prior.Cue:nlohmann::json::object();
    const auto encoded=data.dump();(void)Decode(encoded);return encoded;
}
}
