#pragma once
#include "Core/HelpyDeliveryFlags.h"
#include "Core/RuneSchemaDataRoot.h"
#include <chrono>
#include <stdexcept>
#include <string_view>
// Enforces Helpy delivery flags where items are actually granted. Call it only for
// player-driven deliveries: a player giving items to their own character, locally or through
// a server's Helpy. Deliveries a host makes to another player are never paused.
namespace PS::HelpyDeliveryGate {
inline std::filesystem::path Folder(){return RuneSchemaData::SharedDirectory("HELPY")/"flags";}
inline int64_t Now(){return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
// Throws the player-facing reason when the delivery must not run. world may be empty when the
// world-save GUID is unknown (for example on a client); character flags still apply.
inline void Require(std::string_view character,std::string_view world){
    const auto snapshot=HelpyDeliveryFlags::Read(Folder(),HelpyDeliveryFlags::Signature::DefaultVerifier(),Now());
    if(HelpyDeliveryFlags::NormalizeGuid(character).empty()){
        // Without a character identity a character pause cannot be ruled out.
        if(HelpyDeliveryFlags::AnyCharacterPause(snapshot))
            throw std::runtime_error("Helpy could not confirm which character this delivery is for, and a delivery pause is set on this PC. Wait for the character to finish loading, then retry.");
    }
    if(const auto block=HelpyDeliveryFlags::Check(snapshot,character,world))throw std::runtime_error(block->Message);
}
}
