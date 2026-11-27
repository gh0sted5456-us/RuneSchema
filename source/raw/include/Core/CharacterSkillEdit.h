#pragma once
#include "Core/CharacterSaveEditor.h"
namespace PS::CharacterSkillEdit {
using Json=nlohmann::json;
struct Request {std::string Character,Skill;int Level;};
inline Request Parse(const Json& value){
    const auto guid=CharacterSaveEditor::Guid(value.value("CharacterGuid",Json{}));
    if(guid.empty()||!value.contains("SkillId")||!value["SkillId"].is_string()||!value.contains("Level")||!value["Level"].is_number_integer()||value["Level"]<1||value["Level"]>200)
        throw std::runtime_error("A character identity, native skill and level 1..200 are required");
    const auto id=value["SkillId"].get<std::string>();if(id.empty()||id.size()>128||std::any_of(id.begin(),id.end(),[](unsigned char c){return c<32||c==127;}))throw std::runtime_error("Invalid skill identity");
    return {guid,id,value["Level"].get<int>()};
}
inline void RequireOwner(const Request& edit,const std::string& current,bool allowed){
    if(!allowed||edit.Character!=CharacterSaveEditor::Guid(current))throw std::runtime_error("Skill changes require access to the selected active character on its own platform");
}
inline bool Receipt(const Json& value)noexcept{try{
    (void)Parse(value);
    return value.contains("XP")&&value["XP"].is_number_integer()&&value["XP"]>=0&&value["XP"]<=2147483647
        &&value.contains("Applied")&&value["Applied"].is_boolean()&&value.contains("SaveRequested")&&value["SaveRequested"].is_boolean()
        &&(!value["SaveRequested"].get<bool>()||value["Applied"].get<bool>());
}catch(...){return false;}}

}
