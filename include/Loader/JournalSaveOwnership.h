#pragma once
#include "Loader/QuestSaveOwnership.h"
#include <map>
#include <array>
#include <exception>

namespace DragonWilds::JournalSave {
using Json=nlohmann::json;
using Owners=std::map<std::string,std::string>;
inline constexpr const char* Manifest="RuneSchemaOwnership";
inline void ValidateId(const std::string& id) {
    if(id.empty() || id.size()>1024)throw std::runtime_error("Invalid journal persistence identity");
    for(unsigned char c:id)if(c<32 || c==127)throw std::runtime_error("Invalid journal persistence identity");
}
inline Owners ReadOwnership(const Json& journal) {
    if(!journal.is_object())throw std::runtime_error("Journal save must be an object");
    if(!journal.contains(Manifest))return {};
    const auto& data=journal.at(Manifest);
    if(!data.is_object() || data.size()!=2 || data.value("Version",Json{})!=1
        || !data.contains("Entries") || !data.at("Entries").is_array() || data.at("Entries").size()>16384)
        throw std::runtime_error("Unsupported journal ownership manifest; cleanup refused");
    Owners result;
    for(const auto& entry:data.at("Entries")) {
        if(!entry.is_object() || entry.size()!=2 || !entry.contains("Id") || !entry.at("Id").is_string()
            || !entry.contains("Mod") || !entry.at("Mod").is_string())
            throw std::runtime_error("Malformed journal ownership record; cleanup refused");
        const auto id=entry.at("Id").get<std::string>(),mod=entry.at("Mod").get<std::string>();
        ValidateId(id);Quests::ValidateOwner(mod);
        if(!result.emplace(id,mod).second)throw std::runtime_error("Duplicate journal ownership identity; cleanup refused");
    }
    return result;
}
inline void StoreOwnership(Json& journal,const Owners& owners) {
    if(owners.empty()){journal.erase(Manifest);return;}
    auto entries=Json::array();
    for(const auto& [id,mod]:owners)entries.push_back({{"Id",id},{"Mod",mod}});
    journal[Manifest]={{"Version",1},{"Entries",std::move(entries)}};
}
// Native JSON bridge uses a string array to avoid constructing Unreal JSON
// object/map layouts. Zero strings means no ownership; one is the versioned
// manifest. This is save metadata, not the mod author's journal schema.
inline std::vector<std::string> EncodeNative(const Owners& owners) {
    if(owners.empty())return {};
    Json payload=Json::object();StoreOwnership(payload,owners);
    (void)ReadOwnership(payload);
    auto text=payload.at(Manifest).dump();
    if(text.size()>1024*1024)throw std::runtime_error("Journal ownership metadata too large");
    return {std::move(text)};
}
inline Owners DecodeNative(const std::vector<std::string>& strings) {
    if(strings.empty())return {};
    if(strings.size()!=1 || strings[0].empty() || strings[0].size()>1024*1024)
        throw std::runtime_error("Invalid native journal ownership envelope");
    return ReadOwnership(Json{{Manifest,Json::parse(strings[0])}});
}
using NativeFields=std::array<std::vector<std::string>,3>;
template<class Write> void ReplaceNativeFields(const NativeFields& before,const NativeFields& after,Write&& write) {
    size_t attempted=0;
    try {
        while(attempted<after.size()) {
            const auto index=attempted++;
            write(index,after[index]);
        }
    }catch(...) {
        const auto failure=std::current_exception();
        bool restored=true;
        // Include the failing setter: it may have mutated before verification.
        while(attempted) {
            const auto index=--attempted;
            try{write(index,before[index]);}catch(...){restored=false;}
        }
        if(!restored)throw std::runtime_error("Journal payload rollback failed; native load refused");
        std::rethrow_exception(failure);
    }
}
// Pure native-journal payload transformation. The caller supplies provenance
// from successfully registered generated entries, never a path/name prefix.
inline Json Record(const Json& journal,const Owners& registered) {
    auto owners=ReadOwnership(journal);
    for(const auto& [id,mod]:registered) {
        ValidateId(id);Quests::ValidateOwner(mod);
        const auto [found,added]=owners.emplace(id,mod);
        if(!added && found->second!=mod)throw std::runtime_error("Journal ownership transfer refused");
    }
    if(owners.size()>16384)throw std::runtime_error("Journal ownership limit exceeded");
    auto result=journal;StoreOwnership(result,owners);return result;
}
struct Cleanup {Json Journal;std::set<std::string> Removed;};
inline Cleanup RemoveAbsent(const Json& journal,const std::set<std::string>& absent,bool scanComplete) {
    if(!scanComplete)throw std::runtime_error("Incomplete mod scan; journal cleanup refused");
    for(const auto& mod:absent)Quests::ValidateOwner(mod);
    auto owners=ReadOwnership(journal);
    Cleanup result{journal,{}};
    for(const auto& [id,mod]:owners)if(absent.contains(mod))result.Removed.insert(id);
    for(const auto* field:{"UnlockedEntries","UnreadEntries"}) {
        if(!journal.contains(field))continue;
        const auto& source=journal.at(field);
        if(!source.is_array() || source.size()>65535)throw std::runtime_error("Invalid journal saved entry list");
        auto retained=Json::array();
        for(const auto& entry:source) {
            if(!entry.is_string())throw std::runtime_error("Invalid journal saved entry identity");
            const auto id=entry.get<std::string>();ValidateId(id);
            if(!result.Removed.contains(id))retained.push_back(entry);
        }
        result.Journal[field]=std::move(retained);
    }
    for(const auto& id:result.Removed)owners.erase(id);
    StoreOwnership(result.Journal,owners);
    return result;
}
}
