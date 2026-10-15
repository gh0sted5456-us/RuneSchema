#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// Author-facing recipe discovery compiled into the game's existing
// ProgressionBundleRow / UnlockMatchQuery, not a second per-tick rule engine.
namespace PS::RecipeDiscovery {
using Json=nlohmann::json;
struct Rule {Json Query;std::string Description;};
inline void Require(bool value,const std::string& message){if(!value)throw std::runtime_error("Recipe Discovery: "+message);}
inline std::string Path(const Json& value) {
    Require(value.is_string(),"references must be cooked object paths");
    auto text=value.get<std::string>();
    const auto slash=text.rfind('/'),dot=text.rfind('.');
    Require(text.size()<=1024&&!text.empty()&&text.front()=='/'&&slash>0&&slash!=std::string::npos
        &&dot!=std::string::npos&&dot>slash&&dot+1<text.size()&&text.find("..") == std::string::npos
        &&!text.starts_with("/Script/")&&!text.starts_with("/Engine/")
        &&std::none_of(text.begin(),text.end(),[](unsigned char c){return c<=32||c==127||c=='\\'||c==','||c=='['||c==']';}),
        "use an exact cooked object path, including the object name");
    return text;
}
inline Json Operand(const Json& value,std::string& description) {
    Require(value.is_object()&&value.size()==1,"each operand requires exactly one supported condition");
    const auto& key=value.begin().key();const auto& input=value.begin().value();
    Json result={{"UnlockType","EProgressUnlockType::ItemsPickedUp"},{"Condition","EUnlockMatchCondition::AllMatch"},
        {"Items",Json::array()},{"Actors",Json::array()}};
    if(key=="SkillLevel") {
        Require(input.is_object()&&input.size()==2&&input.contains("Skill")&&input.contains("Level"),"SkillLevel requires Skill and Level");
        Require(input.at("Level").is_number_integer(),"skill level must be an integer");
        const auto level=input.at("Level").get<std::int64_t>();
        Require(level>=1&&level<=100000,"skill level must be between 1 and 100000");
        result["UnlockType"]="EProgressUnlockType::SkillLevelReached";
        result["Skill"]=Path(input.at("Skill"));result["RequiredSkillLevel"]=level;
        description="SKILL LEVEL "+std::to_string(level)+"[ "+result.at("Skill").get<std::string>()+" ]";
        return result;
    }
    const bool items=key=="AllItems"||key=="AnyItems",actors=key=="AllActors"||key=="AnyActors";
    Require(items||actors,"supported operands are AllItems, AnyItems, AllActors, AnyActors and SkillLevel");
    Require(input.is_array()&&!input.empty()&&input.size()<=64,"item/actor conditions require 1 to 64 unique paths");
    const bool all=key.starts_with("All");
    result["UnlockType"]=items?"EProgressUnlockType::ItemsPickedUp":"EProgressUnlockType::ActorsInteractedWith";
    result["Condition"]=all?"EUnlockMatchCondition::AllMatch":"EUnlockMatchCondition::AnyMatch";
    auto& paths=result[items?"Items":"Actors"];std::set<std::string> unique;
    description=std::string(all?"ALL ":"ANY ")+(items?"ITEMS[ ":"ACTORS[ ");
    for(const auto& entry:input){auto path=Path(entry);Require(unique.insert(path).second,"duplicate prerequisite path");
        if(paths.size())description+=", ";description+=path;paths.push_back(std::move(path));}
    description+=" ]";
    return result;
}
inline std::optional<Rule> Parse(const Json& body) {
    if(!body.contains("Discovery"))return std::nullopt;
    Require(!body.value("Unlock",false),"Discovery and Unlock:true are mutually exclusive; use one policy");
    const auto& value=body.at("Discovery");
    if(value.is_string()){Require(value=="Native","the only string mode is Native");return std::nullopt;}
    Require(value.is_object()&&value.size()==1,"use a single condition, All or Any");
    Json operands=Json::array();bool andOperator=true;
    if(value.contains("All")||value.contains("Any")){
        andOperator=value.contains("All");operands=value.at(andOperator?"All":"Any");
        Require(operands.is_array()&&!operands.empty()&&operands.size()<=2,
            "native queries support one or two operands; put multiple items in AllItems/AnyItems, not nested groups");
    }else operands.push_back(value);
    Rule result;std::string first;
    result.Query={{"FirstOperand",Operand(operands.at(0),first)},{"bUseSecondOperand",operands.size()==2},
        {"Operator",andOperator?"EUnlockMatchOperator::AND":"EUnlockMatchOperator::OR"}};
    result.Description=first;
    if(operands.size()==2){std::string second;result.Query["SecondOperand"]=Operand(operands.at(1),second);
        result.Description+=(andOperator?" AND ":" OR ")+second;}
    return result;
}
inline std::vector<std::string> RetainOtherRecipes(const std::vector<std::string>& outputs,const std::set<std::string>& replaced) {
    std::vector<std::string> result;result.reserve(outputs.size());
    for(const auto& path:outputs)if(!replaced.contains(path))result.push_back(path);
    return result;
}
}
