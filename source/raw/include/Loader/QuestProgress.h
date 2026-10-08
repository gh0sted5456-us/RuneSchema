#pragma once
#include "Loader/QuestDefinition.h"
#include "Core/ConfigFiles.h"
#include "Generator/SaveReport.h"
#include <mutex>

namespace DragonWilds::QuestProgress {
using Json=nlohmann::json;
enum class State { Ungiven, Active, Pending, Complete };
enum class Result { Accepted, Completed, AlreadyActive, AlreadyComplete, NotReady, Uncertain };
class OperationGuard {
    inline static std::mutex mutex;
    inline static std::set<std::filesystem::path> active;
    std::filesystem::path key;
public:
    explicit OperationGuard(const std::filesystem::path& path):key(std::filesystem::absolute(path).lexically_normal()) {
        std::lock_guard lock(mutex);
        if(!active.insert(key).second)throw std::runtime_error("Quest progress operation already in progress");
    }
    OperationGuard(const OperationGuard&)=delete;
    OperationGuard& operator=(const OperationGuard&)=delete;
    ~OperationGuard(){std::lock_guard lock(mutex);active.erase(key);}
};
inline void Write(const std::filesystem::path& path,const Json& data) {
    const auto text=data.dump(2);
    if(text.size()>512*1024)throw std::runtime_error("Quest progress exceeds byte limit");
    PS::ConfigFiles::Write(path,text);
}
inline Json Read(const std::filesystem::path& path,const std::string& character) {
    if(character.empty() || PS::SaveReport::Guid(character)!=character)
        throw std::runtime_error("Quest progress requires a canonical character GUID");
    if(!std::filesystem::exists(path))return {{"Version",1},{"CharacterGuid",character},{"Quests",Json::object()}};
    auto data=Json::parse(PS::ConfigFiles::Read(path,512*1024));
    Dialogue::Fields(data,{"Version","CharacterGuid","Quests"});
    if(!data.at("Version").is_number_integer() || data.at("Version")!=1 || data.at("CharacterGuid")!=character || !data.at("Quests").is_object() || data["Quests"].size()>512)
        throw std::runtime_error("Invalid quest progress document; original preserved");
    for(const auto& [key,record]:data["Quests"].items()) {
        if(key.find(':')==key.npos || Dialogue::Reference("_",key)!=key)
            throw std::runtime_error("Quest progress key must be qualified");
        Dialogue::Fields(record,{"Definition","State","Operation"});
        const auto definition=Quests::Parse(key.substr(0,key.find(':')),record.at("Definition"));
        if(definition.Key!=key)throw std::runtime_error("Quest progress identity mismatch");
        const auto state=record.at("State").get<std::string>();
        const auto operation=record.at("Operation").get<std::string>();
        if((state!="active" && state!="pending" && state!="complete")
            || (state=="pending" ? (operation!="accept" && operation!="turn_in") : !operation.empty()))
            throw std::runtime_error("Invalid quest progress state");
    }
    return data;
}
inline State Status(const Json& data,const Quests::Definition& quest,const Json& definition) {
    const auto& records=data.at("Quests");
    if(!records.contains(quest.Key))return State::Ungiven;
    const auto& record=records.at(quest.Key);
    if(record.at("Definition")!=definition)
        throw std::runtime_error("Accepted quest definition changed; explicit migration required");
    const auto state=record.at("State").get<std::string>();
    return state=="complete"?State::Complete:state=="active"?State::Active:State::Pending;
}
// One authoritative caller using a canonical path per character is required. Callbacks must
// return true only after observing the expected native postcondition.
template<class AcceptNative>
Result Accept(const std::filesystem::path& path,const std::string& character,
    const std::string& mod,const Json& definition,AcceptNative acceptNative) {
    OperationGuard operation(path);
    const auto quest=Quests::Parse(mod,definition);
    auto data=Read(path,character);
    switch(Status(data,quest,definition)) {
        case State::Complete:return Result::AlreadyComplete;
        case State::Active:return Result::AlreadyActive;
        case State::Pending:return Result::Uncertain;
        case State::Ungiven:break;
    }
    if(data["Quests"].size()>=512)throw std::runtime_error("Quest progress limit reached");
    auto& record=data["Quests"][quest.Key];
    record={{"Definition",definition},{"State","pending"},{"Operation","accept"}};
    Write(path,data);
    if(!acceptNative())return Result::Uncertain;
    record["State"]="active";record["Operation"]="";
    Write(path,data);
    return Result::Accepted;
}
template<class Ready,class Take,class Give,class CompleteNative>
Result TurnIn(const std::filesystem::path& path,const std::string& character,
    const std::string& mod,const Json& definition,Ready ready,Take take,Give give,CompleteNative completeNative) {
    OperationGuard operation(path);
    const auto quest=Quests::Parse(mod,definition);
    auto data=Read(path,character);
    const auto state=Status(data,quest,definition);
    if(state==State::Complete)return Result::AlreadyComplete;
    if(state==State::Pending)return Result::Uncertain;
    if(state!=State::Active || !ready())return Result::NotReady;
    auto& record=data["Quests"][quest.Key];
    record["State"]="pending";record["Operation"]="turn_in";
    Write(path,data);
    if(!take() || !give() || !completeNative())return Result::Uncertain;
    record["State"]="complete";record["Operation"]="";
    Write(path,data);
    return Result::Completed;
}
}
