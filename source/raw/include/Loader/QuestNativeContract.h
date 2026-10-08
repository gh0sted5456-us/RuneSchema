#pragma once
#include <array>
#include <span>
#include <string_view>
#include <stdexcept>

namespace DragonWilds::QuestNative {
enum class Method { Give, Complete, Initialize, IsInitialized, State, Objective, GetInt, SetInt };
enum class Kind { Quest, Boolean, Name, State, Integer };
struct Field { std::string_view Name; int Offset,Size; Kind Type; bool Return=false; };
struct Contract { std::string_view Name; int Size; std::span<const Field> Fields; };
inline constexpr std::array<Field,2> Mutate{{{"QuestData",0,8,Kind::Quest},{"bSilent",8,1,Kind::Boolean}}};
inline constexpr std::array<Field,1> Init{{{"QuestData",0,8,Kind::Quest}}};
inline constexpr std::array<Field,2> Initialized{{{"QuestData",0,8,Kind::Quest},{"ReturnValue",8,1,Kind::Boolean,true}}};
inline constexpr std::array<Field,2> State{{{"QuestData",0,8,Kind::Quest},{"ReturnValue",8,1,Kind::State,true}}};
inline constexpr std::array<Field,2> Objective{{{"QuestData",0,8,Kind::Quest},{"ObjectiveName",8,8,Kind::Name}}};
inline constexpr std::array<Field,3> ReadInt{{{"QuestData",0,8,Kind::Quest},{"QuestIntName",8,8,Kind::Name},{"ReturnValue",16,4,Kind::Integer,true}}};
inline constexpr std::array<Field,3> WriteInt{{{"QuestData",0,8,Kind::Quest},{"QuestIntName",8,8,Kind::Name},{"IntValue",16,4,Kind::Integer}}};
inline Contract Get(Method method) {
    switch(method) {
        case Method::Give:return {"GiveQuest",9,Mutate};
        case Method::Complete:return {"CompleteQuest",9,Mutate};
        case Method::Initialize:return {"InitQuest",8,Init};
        case Method::IsInitialized:return {"IsQuestInitialized",9,Initialized};
        case Method::State:return {"GetQuestState",9,State};
        case Method::Objective:return {"SetQuestObjective",16,Objective};
        case Method::GetInt:return {"GetQuestInt",20,ReadInt};
        case Method::SetInt:return {"SetQuestInt",20,WriteInt};
    }
    throw std::runtime_error("Unknown quest method");
}
inline void Validate(const Contract& expected,int size,std::span<const Field> actual) {
    if(expected.Size!=size || expected.Fields.size()!=actual.size())throw std::runtime_error("Quest function frame changed");
    for(const auto& wanted:expected.Fields) {
        int matches=0;
        for(const auto& field:actual)if(field.Name==wanted.Name) {
            ++matches;
            if(field.Offset!=wanted.Offset || field.Size!=wanted.Size || field.Type!=wanted.Type || field.Return!=wanted.Return)
                throw std::runtime_error("Quest function parameter changed");
        }
        if(matches!=1)throw std::runtime_error("Quest function parameter missing or duplicated");
    }
}
}
