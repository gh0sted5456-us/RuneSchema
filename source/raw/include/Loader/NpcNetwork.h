#pragma once
#include "Loader/VendorIdentity.h"
#include <string>
#include <nlohmann/json.hpp>
namespace DragonWilds::NpcNetwork {
inline constexpr unsigned ClientArrivalAttempts=40;
inline bool ClientComponentsReady(int interactions,int stations,bool merchant=true) {
    return interactions==1 && stations==(merchant?1:0);
}
inline bool NeedsIdentityReplacement(bool multiplayer,std::string_view actual,std::string_view expected) {
    return multiplayer && !expected.empty() && actual!=expected;
}
inline std::string ActorName(std::string_view definition) {
    const auto words=VendorIdentity::ForOwner(definition);
    constexpr char digits[]="0123456789abcdef";
    std::string name="RuneSchemaNPC_";
    for(const auto word:words)for(int shift=28;shift>=0;shift-=4)name+=digits[(word>>shift)&15];
    return name;
}
inline std::string GameplayFingerprint(const std::string& mod,nlohmann::json definition) {
    if(!definition.is_object())throw std::runtime_error("NPC compatibility requires an object");
    for(const auto* field:{"DisplayName","HideName","MerchantName","VendorHeaderImage","Map","OverheadIcon","VisualEffect"})
        definition.erase(field);
    return "gameplay-v1:"+ActorName(mod+":"+definition.dump());
}
inline bool Supported(bool enabled,bool human,bool resource,bool merchant,bool dialogue,bool locked,bool lore=false,bool visual=false) {
    (void)human;(void)resource;(void)locked;
    return enabled && (merchant || dialogue || lore || visual);
}
inline std::string DialogueFingerprint(const std::string& base,const nlohmann::json& dialogues,const nlohmann::json& quests) {
    if(base.empty() || !dialogues.is_object() || dialogues.empty() || !quests.is_array() || quests.empty())
        throw std::runtime_error("Multiplayer dialogue requires matching graphs and a prepared quest registry");
    return "dialogue-v1:"+ActorName(nlohmann::json::array({base,dialogues,quests}).dump());
}
}
