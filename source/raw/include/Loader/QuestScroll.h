#pragma once
#include "Loader/DialogueDefinition.h"
#include "Loader/ItemIdentity.h"
#include <map>
#include <mutex>
#include <optional>
#include <algorithm>
namespace DragonWilds::QuestScrolls {
struct Contract {
    std::string QuestKey,PersistenceId,EntryId;
    std::string Action="Accept";
    bool operator==(const Contract&) const = default;
};
inline Contract Parse(const std::string& mod,const nlohmann::json& value) {
    Dialogue::Fields(value,{"QuestID","PersistenceID","EntryID","Action"});
    const auto reference=Dialogue::Text(value.at("QuestID"),257);
    if(reference.find_first_of("/\\\r\n\t ")!=reference.npos)throw std::runtime_error("$QuestScroll QuestID must name a declared local quest or Mod:Id, not an asset/function path");
    Contract result{Dialogue::Reference(mod,value.at("QuestID")),Dialogue::Text(value.at("PersistenceID"),22),{}};
    if(!IsCanonicalPersistenceId(result.PersistenceId))throw std::runtime_error("$QuestScroll requires the item's canonical PersistenceID, distinct from the quest identity");
    if(value.contains("EntryID"))result.EntryId=Dialogue::Id(value.at("EntryID"));
    if(value.contains("Action"))result.Action=Dialogue::Text(value.at("Action"),16);
    if(result.Action!="Accept" && result.Action!="Abandon")throw std::runtime_error("$QuestScroll Action must be Accept or Abandon");
    if(result.Action=="Abandon" && !result.EntryId.empty())throw std::runtime_error("Cancellation scrolls cannot select a quest entry option");
    return result;
}
class Catalog {
    std::map<std::string,Contract> entries;
public:
    void Bind(const std::string& path,const Contract& contract) {
        if(path.empty()||path.front()!='/'||path.find('.')==path.npos||path.size()>1024||path.find("..")!=path.npos
            ||std::any_of(path.begin(),path.end(),[](unsigned char c){return c<32||c==127;}))throw std::runtime_error("Quest-scroll target requires an exact ItemData object path");
        if(const auto found=entries.find(path);found!=entries.end()) {
            if(found->second!=contract)throw std::runtime_error("Conflicting quest-scroll binding; restart after fixing the declarations");
            return;
        }
        if(entries.size()>=4096)throw std::runtime_error("Quest-scroll declaration limit reached");
        for(const auto& [other,binding]:entries)if(binding.PersistenceId==contract.PersistenceId)throw std::runtime_error("Quest-scroll PersistenceID is already bound to another item");
        entries.emplace(path,contract);
    }
    std::optional<Contract> Find(const std::string& path) const {const auto found=entries.find(path);return found==entries.end()?std::nullopt:std::optional(found->second);}
};
inline std::mutex Mutex;
inline Catalog Registered;
inline void Bind(const std::string& path,const Contract& contract){std::lock_guard lock(Mutex);Registered.Bind(path,contract);}
inline std::optional<Contract> Find(const std::string& path){std::lock_guard lock(Mutex);return Registered.Find(path);}
}
