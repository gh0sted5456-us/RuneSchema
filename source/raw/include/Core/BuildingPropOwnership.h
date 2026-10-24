#pragma once
#include "Loader/QuestSaveOwnership.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <map>
#include <set>

namespace PS::BuildingPropOwnership {
using Json=nlohmann::json;
using Guid=std::array<uint32_t,4>;
inline Guid ActorGuid(const std::string& identity,bool legacy=false){
    Guid words={2166136261u,2166136261u^0x9E3779B9u,2166136261u^0x85EBCA6Bu,2166136261u^0xC2B2AE35u};
    for(unsigned char byte:identity)for(auto& word:words)word=(word^byte)*16777619u;
    if(!legacy)words[0]=0x48435352;return words;
}
inline void Require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Record {
    std::string Owner,Id,Class,Building,Binding;
    bool operator==(const Record&)const=default;
    std::string Identity()const{return Owner+"|actor|"+Id;}
};
inline void Validate(const Record& record){
    DragonWilds::Quests::ValidateOwner(record.Owner);
    Require(record.Owner.find('|')==record.Owner.npos&&!record.Id.empty()&&record.Id.size()<=256
        &&record.Id.find_first_of("|\r\n\t")==record.Id.npos,"Invalid managed building placement identity");
    for(const auto* path:{&record.Class,&record.Building})Require(!path->empty()&&path->size()<=1024&&path->front()=='/'
        &&path->find('.')!=path->npos&&path->find_first_of("\\\r\n\t:")==path->npos&&path->find("..") ==path->npos,"Invalid managed building native path");
    Require(record.Binding=="BuildingPieceData"||record.Binding=="BuildingData"||record.Binding=="BuildingPiece"
        ||record.Binding.empty(),"Invalid managed building binding");
}
inline Json Encode(const std::vector<Record>& records){
    Require(records.size()<=4096,"Managed building history exceeds 4096 definitions");
    Json rows=Json::array();std::set<std::string> seen;std::map<Guid,std::string> identities;
    for(const auto& record:records){
        Validate(record);const auto identity=record.Identity();
        for(bool legacy:{false,true}){
            const auto [found,added]=identities.emplace(ActorGuid(identity,legacy),identity);
            Require(added||found->second==identity,"Managed building stable identity collision; cleanup refused");
        }
        Json row={{"Owner",record.Owner},{"Id",record.Id},{"Class",record.Class},{"Building",record.Building},{"Binding",record.Binding}};
        Require(seen.insert(row.dump()).second,"Duplicate managed building history record");rows.push_back(std::move(row));
    }
    return {{"Version",1},{"Records",std::move(rows)}};
}
inline std::vector<Record> Decode(const Json& document){
    Require(document.is_object()&&document.size()==2&&document.value("Version",Json{})==1&&document.contains("Records")
        &&document.at("Records").is_array()&&document.at("Records").size()<=4096,"Unsupported managed building ownership history");
    std::vector<Record> result;
    for(const auto& row:document.at("Records")){
        Require(row.is_object()&&row.size()==5,"Malformed managed building history");
        result.push_back({row.at("Owner").get<std::string>(),row.at("Id").get<std::string>(),row.at("Class").get<std::string>(),
            row.at("Building").get<std::string>(),row.at("Binding").get<std::string>()});
    }
    (void)Encode(result);return result;
}
inline std::vector<Record> Remember(std::vector<Record> history,const std::vector<Record>& current,bool complete){
    Require(complete,"Incomplete placement discovery; managed building cleanup refused");
    for(const auto& record:current)if(std::find(history.begin(),history.end(),record)==history.end())history.push_back(record);
    (void)Encode(history);return history;
}
enum class Action { Preserve,Keep,Retire };
struct Proof {bool Authority=false,RegistryReady=false,SameWorld=false,ExactClass=false,ExactBinding=false,UniqueIdentity=false;};
inline Action Decide(const Proof& proof,bool active,bool canonical,bool transient,bool excluded){
    if(!proof.Authority||!proof.RegistryReady||!proof.SameWorld||!proof.ExactClass||!proof.ExactBinding||!proof.UniqueIdentity)return Action::Preserve;
    if(!active||!canonical||!transient||!excluded)return Action::Retire;
    return Action::Keep;
}
}
