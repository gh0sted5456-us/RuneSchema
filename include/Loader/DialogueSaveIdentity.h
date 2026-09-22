#pragma once
#include "Loader/VendorIdentity.h"
#include "Loader/QuestSaveOwnership.h"
namespace DragonWilds::DialogueSave {
inline std::string Key(const std::string& mod){Quests::ValidateOwner(mod);return mod+":__runeschema_dialogue_state";}
inline std::string PersistenceIdForSeed(const std::string& seed) {
    if(seed.empty() || seed.size()>512)throw std::runtime_error("Invalid persistence identity seed");
    auto words=VendorIdentity::ForOwner("RuneSchema.Persistence/v1/"+seed);words[0]=0x44535352;
    constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string output;uint32_t bits=0;int count=0;
    for(auto word:words)for(int shift=0;shift<32;shift+=8) {
        bits=(bits<<8)|((word>>shift)&255);count+=8;
        while(count>=6){count-=6;output+=alphabet[(bits>>count)&63];}
    }
    if(count)output+=alphabet[(bits<<(6-count))&63];
    return output;
}
inline std::string PersistenceId(const std::string& mod) {
    Quests::ValidateOwner(mod);return PersistenceIdForSeed("DialogueSave/"+mod);
}
}
