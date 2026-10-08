#pragma once
#include "Loader/VendorIdentity.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
namespace DragonWilds::DialogueIdentity {
inline std::string GraphKey(const std::string& mod,const std::string& npc,
        const std::string& dialogue,const std::string& character,bool completed,
        const nlohmann::json& definition) {
    if(mod.empty() || npc.empty() || dialogue.empty() || character.empty() || !definition.is_object())
        throw std::runtime_error("Dialogue graph identity requires owner, character and definition");
    return nlohmann::json::array({"RuneSchema.DialogueGraph.v1",mod,npc,dialogue,character,completed,definition}).dump();
}
inline VendorIdentity::Words Node(const std::string& graph,const std::string& node) {
    if(graph.empty() || node.empty())throw std::runtime_error("Dialogue node identity is empty");
    auto words=VendorIdentity::ForOwner(nlohmann::json::array({graph,node}).dump());
    words[0]=0x44475352;
    return words;
}
}
