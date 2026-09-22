#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <stdexcept>
namespace DragonWilds::NpcIdentity {
inline constexpr wchar_t ClassPath[]=L"/RuneSchema/Networking/BPC_RuneSchemaIdentity.BPC_RuneSchemaIdentity_C";
inline constexpr wchar_t LegacyClassPath[]=L"/Game/RuneSchema/Networking/BPC_RuneSchemaIdentity.BPC_RuneSchemaIdentity_C";
inline constexpr size_t MaxPayload=4096;
struct Payload { std::string Mod,Npc,Fingerprint; uint64_t CueRevision=0; nlohmann::json Cue; };
inline Payload Decode(const std::string& text) {
    if(text.empty() || text.size()>MaxPayload)throw std::runtime_error("NPC identity payload length invalid");
    const auto data=nlohmann::json::parse(text);
    if(!data.is_object() || !data.contains("version")
        || !data["version"].is_number_integer() || (data["version"]!=1 && data["version"]!=2))
        throw std::runtime_error("NPC identity protocol unsupported");
    const auto read=[&](const char* key) {
        if(!data.contains(key) || !data[key].is_string())throw std::runtime_error("NPC identity field missing or not a string");
        auto value=data[key].get<std::string>();
        if(value.empty() || value.size()>256)throw std::runtime_error("NPC identity field length invalid");
        for(unsigned char c:value)if(c<32 || c==127)throw std::runtime_error("NPC identity contains control characters");
        return value;
    };
    Payload result{read("mod"),read("npc"),read("fingerprint")};
    if(data["version"]==2) {
        if(!data.contains("cueRevision") || !data["cueRevision"].is_number_unsigned()
            || !data.contains("cue") || !data["cue"].is_object())throw std::runtime_error("NPC visual cue envelope is invalid");
        result.CueRevision=data["cueRevision"].get<uint64_t>();result.Cue=data["cue"];
    }
    return result;
}
inline std::string Encode(const std::string& mod,const std::string& npc,const std::string& fingerprint) {
    const auto text=nlohmann::json{{"version",1},{"mod",mod},{"npc",npc},{"fingerprint",fingerprint}}.dump();
    (void)Decode(text);return text;
}
}
