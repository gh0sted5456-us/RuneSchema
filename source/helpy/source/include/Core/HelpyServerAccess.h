#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>

namespace PS::HelpyServerAccess {
// One prompt per connected character/world; dismissing it must not interrupt play again.
struct Prompt {
    uint64_t Generation=~uint64_t{};std::string Guid;bool Seen=false;
    bool Observe(uint64_t generation,const nlohmann::json& access){
        const auto guid=access.is_object()?access.value("CharacterGuid",std::string{}):std::string{};
        if(generation!=Generation||(!guid.empty()&&guid!=Guid)){Generation=generation;Guid=guid;Seen=false;}
        const bool required=access.is_object()&&access.value("Available",false)&&access.value("PinRequired",false)&&!access.value("Allowed",false);
        if(required&&!Seen){Seen=true;return true;}return false;
    }
};
}
