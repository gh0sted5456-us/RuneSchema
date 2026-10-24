#pragma once
#include <nlohmann/json.hpp>
#include <bit>
#include <cstdint>
#include <stdexcept>
namespace DragonWilds::Quests::DefinitionIdentity {
using Json=nlohmann::json;
inline uint64_t Hash(const Json& value){uint64_t hash=14695981039346656037ULL;for(unsigned char b:value.dump()){hash^=b;hash*=1099511628211ULL;}return hash;}
inline Json Progress(Json value){
    for(const char* key:{"Title","Description","Icon","CompletedDescription","Region","HideInQuestList","ShowInCompletedQuestList"})value.erase(key);
    const auto objective=[](Json& entry){if(entry.is_object())for(const char* key:{"Text","SubobjectiveDescription","Hint","QuestDescriptionOverride","ProgressText","CompleteText","AnnounceProgress"})entry.erase(key);};
    if(value.contains("Objective"))objective(value["Objective"]);
    if(value.contains("Stages")&&value["Stages"].is_array())for(auto& stage:value["Stages"])
        if(stage.contains("Objectives")&&stage["Objectives"].is_array())for(auto& entry:stage["Objectives"])objective(entry);
    return value;
}
template<class Read>bool PairMatches(uint64_t hash,const char* lo,const char* hi,Read&& read){
    return read(lo)==std::bit_cast<int32_t>(uint32_t(hash))&&read(hi)==std::bit_cast<int32_t>(uint32_t(hash>>32));
}
enum class Match {Exact,Presentation,Changed};
template<class Read> Match Compare(const Json& value,Read&& read){
    const auto version=read("RuneSchema.LogicVersion");
    if(version<0||version>1)throw std::runtime_error("Unsupported quest progress fingerprint version");
    if(version==1){
        if(!PairMatches(Hash(Progress(value)),"RuneSchema.LogicLo","RuneSchema.LogicHi",read))return Match::Changed;
        return PairMatches(Hash(value),"RuneSchema.DefLo","RuneSchema.DefHi",read)?Match::Exact:Match::Presentation;
    }
    if(PairMatches(Hash(value),"RuneSchema.DefLo","RuneSchema.DefHi",read))return Match::Exact;
    // Old receipts store only the full document hash. We can prove additions
    // of these optional fields by reconstructing the old document exactly;
    // arbitrary old display text cannot safely be guessed from a hash.
    for(unsigned mask=1;mask<8;++mask){auto prior=value;if(mask&1)prior.erase("Region");if(mask&2)prior.erase("CompletedDescription");if(mask&4)prior.erase("Icon");
        if(PairMatches(Hash(prior),"RuneSchema.DefLo","RuneSchema.DefHi",read))return Match::Presentation;
    }
    return Match::Changed;
}
}
