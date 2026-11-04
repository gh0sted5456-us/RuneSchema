#pragma once
#include "Core/ModDataRelay.h"
#include <limits>

namespace PS::CharacterAppearance {
using Json=nlohmann::json;
inline constexpr size_t MaximumBytes=4096,MaximumRecords=1024,SnapshotBytes=4*1024*1024;
struct Route {std::string Key,Class,Function="OnRuneSchemaCharacterAppearance";size_t Limit=MaximumBytes;};
inline Route Declaration(const std::string& owner,const std::string& id,const Json& value){
    using ModDataRelay::Require;
    Require(ModDataRelay::Token(owner,96)&&owner.find(':')==owner.npos&&ModDataRelay::Token(id,96)&&id.find(':')==id.npos&&value.is_object(),"Character appearance registry identity is invalid");
    for(const auto& [key,_]:value.items())Require(key=="MaxBytes"||key=="Class"||key=="Function","Unknown character appearance declaration field");
    if(value.contains("MaxBytes"))Require(value["MaxBytes"].is_number_unsigned()||(value["MaxBytes"].is_number_integer()&&value["MaxBytes"].get<int64_t>()>0),"Character appearance byte limit must be positive");
    Route route{owner+":"+id};route.Limit=value.value("MaxBytes",MaximumBytes);
    Require(route.Limit>=2&&route.Limit<=MaximumBytes,"Character appearance payload exceeds the RPC limit");
    if(value.contains("Class")){
        route.Class=value.at("Class").get<std::string>();
        Require(route.Class.size()>4&&route.Class.size()<=1024&&route.Class[0]=='/'&&route.Class.ends_with("_C")
            &&!route.Class.starts_with("/Script/")&&!route.Class.starts_with("/Engine/")&&route.Class.find("..") == route.Class.npos
            &&std::none_of(route.Class.begin(),route.Class.end(),[](unsigned char c){return c<=32||c=='\\';}),"Character appearance consumer must be a cooked component class");
    }
    if(value.contains("Function")){Require(!route.Class.empty(),"Character appearance Function requires a Class");route.Function=value.at("Function").get<std::string>();}
    Require(ModDataRelay::Token(route.Function,96)&&route.Function.find(':')==route.Function.npos,"Character appearance callback name is invalid");
    return route;
}
inline std::string Guid(std::string value){
    ModDataRelay::Require(value.size()==32&&std::all_of(value.begin(),value.end(),[](unsigned char c){return (c>='0'&&c<='9')||(c>='A'&&c<='F')||(c>='a'&&c<='f');}),"Character appearance GUID is invalid");
    for(auto& c:value)if(c>='a'&&c<='f')c-=32;
    ModDataRelay::Require(value!=std::string(32,'0'),"Character appearance GUID is unassigned");return value;
}
inline std::string Payload(const Route& route,const std::string& text){
    ModDataRelay::Require(!text.empty()&&text.size()<=route.Limit,"Character appearance payload is too large or empty");
    const auto value=Json::parse(text,[](int depth,Json::parse_event_t,Json&){ModDataRelay::Require(depth<=32,"Character appearance JSON nesting is too deep");return true;});
    ModDataRelay::Require(value.is_object(),"Character appearance payload must be a JSON object");
    auto canonical=value.dump();ModDataRelay::Require(canonical.size()<=route.Limit,"Character appearance canonical payload is too large");return canonical;
}
struct Record {std::string Subject,Key,CharacterGuid,Payload;int64_t Revision=0;bool Removed=false;};
struct Recipient {
    std::set<std::string> Known;
    std::map<std::pair<std::string,std::string>,int64_t> Seen;
    std::map<std::string,int64_t> Published;
    void Subject(const std::string& scope,bool present){
        if(present)Known.insert(scope);else Known.erase(scope);
        std::erase_if(Seen,[&](const auto& row){return row.first.first==scope;});
    }
    bool Wants(const Record& record,const std::string& ownSubject,bool local)const{
        if(!local&&record.Subject!=ownSubject&&!Known.contains(record.Subject))return false;
        const auto at=Seen.find({record.Subject,record.Key});return at==Seen.end()||at->second<record.Revision;
    }
    void CheckPublication(const std::string& key,int64_t revision)const{
        const auto at=Published.find(key);
        ModDataRelay::Require(revision>0&&(at==Published.end()||revision>at->second),"Character appearance publication revision is stale");
    }
};
class Ledger {
    std::map<std::pair<std::string,std::string>,Record> Records;
    size_t Bytes=0;int64_t Revision=0;
public:
    const auto& Snapshot()const{return Records;}
    size_t Size()const{return Records.size();}size_t StoredBytes()const{return Bytes;}
    void Clear(){Records.clear();Bytes=0;Revision=0;}
    const Record& Put(const Route& route,const std::string& subject,const std::string& guid,const std::string& text,bool removed=false){
        ModDataRelay::Require(!subject.empty()&&subject.size()<=96,"Character appearance subject is invalid");
        const auto actualGuid=Guid(guid),payload=removed?std::string("{}"):Payload(route,text);
        const auto key=std::pair(subject,route.Key);const auto old=Records.find(key);
        if(old!=Records.end()&&old->second.CharacterGuid==actualGuid&&old->second.Payload==payload&&old->second.Removed==removed)return old->second;
        const auto previous=old==Records.end()?0:old->second.Payload.size();
        ModDataRelay::Require((old!=Records.end()||Records.size()<MaximumRecords)&&Bytes-previous+payload.size()<=SnapshotBytes,"Character appearance snapshot capacity exceeded");
        ModDataRelay::Require(Revision<std::numeric_limits<int64_t>::max(),"Character appearance revision exhausted");
        auto& record=Records[key];record={subject,route.Key,actualGuid,payload,++Revision,removed};Bytes=Bytes-previous+payload.size();return record;
    }
    void Remove(const std::string& subject){for(auto at=Records.begin();at!=Records.end();)if(at->second.Subject==subject){Bytes-=at->second.Payload.size();at=Records.erase(at);}else ++at;}
};
}
