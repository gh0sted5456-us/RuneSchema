#pragma once
// Version 1 of RuneSchema's optional, dedicated RSDiagLog.jsonl protocol.
// No Unreal, UE4SS, file I/O or log output may occur from this contract.
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace PS::StructuredDiagnostic {
using Json=nlohmann::json;
inline constexpr int Version=1;
inline constexpr std::size_t MaxRecord=64*1024;
inline constexpr std::array<std::string_view,3> Severities={"Error","Warning","Info"};
inline constexpr std::array<std::string_view,6> Outcomes={"Succeeded","Failed","Partial","Skipped","Deferred","Blocked"};
inline constexpr std::array<std::string_view,20> Sources={"JsonParser","AssetLoader","BlueprintLoader","RawTableLoader","ImportPlanner","RecipeLoader","LoaderCore","DiagnosticTransport","BuildingLoader","SpawnLoader","EquipmentLoader","GameplayEffectLoader","VendorLoader","NiagaraLoader","EnumLoader","StringLoader","RegistryLoader","JournalLoader","CourseLoader","NpcLoader"};
inline constexpr std::array<std::string_view,8> Phases={"Discovery","Parsing","Validation","Planning","Resolution","Application","Finalization","Reload"};
inline constexpr std::array<std::string_view,3> CauseStatuses={"Confirmed","Suspected","Unknown"};
template<std::size_t N> inline bool Contains(const std::array<std::string_view,N>& values,std::string_view test){return std::find(values.begin(),values.end(),test)!=values.end();}
struct Record {
    std::string EventId,SessionId,Timestamp,Code,Severity,Outcome,Source,Title,Message;
    std::string OperationId;
    Json Context=Json::object(),Cause=Json::object(),Evidence=Json::object();
};
inline bool IsCode(std::string_view v){
    if(v.size()<9||v.size()>40||v.substr(0,3)!="RS-"||v[v.size()-4]!='-')return false;
    for(std::size_t i=3;i+4<v.size();++i)if(v[i]<'A'||v[i]>'Z')return false;
    for(std::size_t i=v.size()-3;i<v.size();++i)if(v[i]<'0'||v[i]>'9')return false;
    return true;
}
inline bool IsUtc(std::string_view v){return v.size()>=20&&v.size()<=36&&v.back()=='Z'&&v[4]=='-'&&v[7]=='-'&&v[10]=='T'&&v[13]==':'&&v[16]==':';}
inline bool StringsOnly(const Json& object,std::initializer_list<std::string_view> names){
    if(!object.is_object())return false;
    for(auto it=object.begin();it!=object.end();++it){
        if(std::find(names.begin(),names.end(),std::string_view(it.key()))==names.end()||!it.value().is_string())return false;
        if(it.value().get_ref<const std::string&>().size()>8192)return false;
    }
    return true;
}
inline bool Validate(const Record& r){
    if(r.EventId.empty()||r.EventId.size()>128||r.SessionId.empty()||r.SessionId.size()>128||
       !IsUtc(r.Timestamp)||!IsCode(r.Code)||!Contains(Severities,r.Severity)||
       !Contains(Outcomes,r.Outcome)||!Contains(Sources,r.Source)||
       r.Title.empty()||r.Title.size()>512||r.Message.empty()||r.Message.size()>16384||
       r.OperationId.size()>128)return false;
    if(!StringsOnly(r.Context,{"ModId","File","Target","Property","Phase"}))return false;
    if(r.Context.contains("Phase")&&!Contains(Phases,r.Context["Phase"].get<std::string>()))return false;
    if(!r.Cause.is_object()||r.Cause.size()>2||!r.Evidence.is_object()||r.Evidence.size()>7)return false;
    if(!r.Cause.empty()){
        if(!r.Cause.contains("Status")||!r.Cause["Status"].is_string()||!Contains(CauseStatuses,r.Cause["Status"].get<std::string>()))return false;
        for(auto it=r.Cause.begin();it!=r.Cause.end();++it){
            if(it.key()!="Status"&&it.key()!="Description")return false;
            if(!it.value().is_string()||it.value().get_ref<const std::string&>().size()>8192)return false;
        }
    }
    for(auto it=r.Evidence.begin();it!=r.Evidence.end();++it){
        const auto& name=it.key();const auto& v=it.value();
        if(name=="RetryCount"||name=="Line"||name=="Column"||name=="RepeatCount"){
            if(!v.is_number_integer()||v.get<std::int64_t>()<(name=="RetryCount"?0:1))return false;
        }else if(name=="FirstSeen"||name=="LastSeen"){
            if(!v.is_string()||!IsUtc(v.get<std::string>()))return false;
        }else if(name=="Truncated"){
            if(!v.is_boolean())return false;
        }else return false;
    }
    return true;
}
inline Json ToJson(const Record& r){
    Json doc={{"Version",Version},{"EventId",r.EventId},{"SessionId",r.SessionId},
        {"Timestamp",r.Timestamp},{"Code",r.Code},{"Severity",r.Severity},
        {"Outcome",r.Outcome},{"Source",r.Source},{"Title",r.Title},{"Message",r.Message}};
    if(!r.OperationId.empty())doc["OperationId"]=r.OperationId;
    if(!r.Context.empty())doc["Context"]=r.Context;
    if(!r.Cause.empty())doc["Cause"]=r.Cause;
    if(!r.Evidence.empty())doc["Evidence"]=r.Evidence;
    return doc;
}
inline std::optional<Record> Parse(std::string_view line) noexcept {
    if(line.empty()||line.size()>MaxRecord)return std::nullopt;
    try {
        const auto doc=Json::parse(line.begin(),line.end());
        if(!doc.is_object()||doc.value("Version",0)!=Version)return std::nullopt;
        Record r;
        r.EventId=doc.at("EventId").get<std::string>();r.SessionId=doc.at("SessionId").get<std::string>();
        r.Timestamp=doc.at("Timestamp").get<std::string>();r.Code=doc.at("Code").get<std::string>();
        r.Severity=doc.at("Severity").get<std::string>();r.Outcome=doc.at("Outcome").get<std::string>();
        r.Source=doc.at("Source").get<std::string>();r.Title=doc.at("Title").get<std::string>();
        r.Message=doc.at("Message").get<std::string>();
        if(doc.contains("OperationId"))r.OperationId=doc.at("OperationId").get<std::string>();
        if(doc.contains("Context"))r.Context=doc.at("Context");
        if(doc.contains("Cause"))r.Cause=doc.at("Cause");
        if(doc.contains("Evidence"))r.Evidence=doc.at("Evidence");
        if(!Validate(r))return std::nullopt;
        return r;
    }catch(...){return std::nullopt;}
}
inline std::optional<std::string> Encode(const Record& r) noexcept {
    if(!Validate(r))return std::nullopt;
    try {
        auto text=ToJson(r).dump(-1,' ',false,Json::error_handler_t::strict);
        if(text.size()+1>MaxRecord)return std::nullopt;
        text+='\n';return text;
    }catch(...){return std::nullopt;}
}
inline std::string IssueKey(const Record& r){
    // Length-framed parts prevent collisions if mod/asset/property contain separators.
    std::string result;
    const auto append=[&](std::string_view v){result+=std::to_string(v.size());result+=':';result+=v;};
    append(r.Code);append(r.Severity);append(r.Source);
    for(const auto* key:{"ModId","File","Target","Property"}){
        const auto it=r.Context.find(key);append(it!=r.Context.end()&&it->is_string()?it->get_ref<const std::string&>():std::string_view{});
    }
    return result;
}
}
