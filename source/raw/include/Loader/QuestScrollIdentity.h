#pragma once
#include "Loader/ItemIdentity.h"
#include <cstdint>
#include <string>

namespace DragonWilds::QuestScrolls {
// Borrowed FString storage only: validate its header and bounded contents
// before allocating any copy or asking a C-string constructor to find a NUL.
struct IdentityRead {
    int StorageCount=0,Capacity=0;
    bool Empty=false;
    std::string Value,Error;
};
template<class Char> IdentityRead ReadIdentity(const Char* data,int count,int capacity) {
    IdentityRead read{count,capacity};
    if(count<0 || capacity<0 || count>capacity || (capacity>0 && !data))read.Error="invalid string header";
    else if(count==0)read.Empty=true;
    else if(count==1) {
        if(data[0]!=0)read.Error="empty string lacks terminator";
        else read.Empty=true;
    } else if(count!=23)read.Error="expected 22 characters plus terminator";
    else if(data[22]!=0)read.Error="missing string terminator";
    else {
        read.Value.reserve(22);
        for(int i=0;i<22;++i) {
            const auto c=static_cast<uint32_t>(data[i]);
            if(c>127 || c==0){read.Error="non-ASCII or embedded terminator";break;}
            if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-' || c=='_')) {
                read.Error="invalid PersistenceID character";break;
            }
            read.Value.push_back(static_cast<char>(c));
        }
        if(read.Error.empty() && !IsCanonicalPersistenceId(read.Value))read.Error="noncanonical PersistenceID";
    }
    return read;
}
inline std::string IdentityMismatch(const IdentityRead& read,const std::string& expected,bool allowEmptySaveCache=false) {
    if(!IsCanonicalPersistenceId(expected))return "invalid bound PersistenceID";
    if(!read.Error.empty())return read.Error;
    if(read.Empty)return allowEmptySaveCache?std::string{}:"required identity is empty";
    return read.Value==expected?std::string{}:"identity differs from bound PersistenceID";
}
inline bool AllowEmptySaveIdentity(bool saveGame,bool transient,bool replicated,bool ownedSlotMatches,bool nativeItemDataMatches) {
    // Native ItemDataPersistenceId is a nonreplicated save cache, not the live
    // ItemData identity. Fresh native SetItemData does not populate that cache.
    return saveGame && transient && !replicated && ownedSlotMatches && nativeItemDataMatches;
}
}
