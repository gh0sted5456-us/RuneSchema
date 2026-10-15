#pragma once
#include <array>
#include <span>
#include <string_view>
#include <stdexcept>
#include "Core/ReflectedLayout.h"

namespace DragonWilds::QuestNative {
enum class Method { Give, Complete, Initialize, IsInitialized, State, Objective, GetInt, SetInt };
enum class Kind { Quest, Boolean, Name, State, Integer, Progress, ProgressArray, QuestType };
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
    PS::ReflectedLayout::Validate(size,actual);
    if(expected.Fields.size()!=actual.size())throw std::runtime_error("Quest function parameter count changed");
    for(const auto& wanted:expected.Fields) {
        int matches=0;
        for(const auto& field:actual)if(field.Name==wanted.Name) {
            ++matches;
            if(field.Size!=wanted.Size || field.Type!=wanted.Type || field.Return!=wanted.Return)
                throw std::runtime_error("Quest function parameter changed");
        }
        if(matches!=1)throw std::runtime_error("Quest function parameter missing or duplicated");
    }
}
// These hooks observe delivery without reading or constructing its parameter
// buffer. Validate the reflected payload, including the 1.0.1 QuestType input,
// rather than coupling NPC initialization to an old frame-size constant.
inline void ValidateDelivery(std::string_view name,int size,std::span<const Field> actual) {
    PS::ReflectedLayout::Validate(size,actual);
    const auto checkDeliveryField=[&](std::string_view fieldName,int offset,int width,Kind kind) {
        int matches=0;
        for(const auto& field:actual)if(field.Name==fieldName) {
            ++matches;
            if(field.Size!=width || field.Type!=kind || field.Return)
                throw std::runtime_error("Quest delivery parameter changed");
        }
        if(matches!=1)throw std::runtime_error("Quest delivery parameter missing or duplicated");
    };
    if(name=="Client_OnQuestUpdated") {
        if(actual.size()!=4)throw std::runtime_error("Quest delivery parameter count changed");
        int progressSize=0;
        for(const auto& field:actual)if(field.Name=="OldQuest")progressSize=field.Size;
        if(progressSize<8 || progressSize>32766)
            throw std::runtime_error("Quest delivery progress frame changed");
        checkDeliveryField("OldQuest",0,progressSize,Kind::Progress);
        checkDeliveryField("UpdatedQuest",progressSize,progressSize,Kind::Progress);
        checkDeliveryField("bSendNotification",2*progressSize,1,Kind::Boolean);
        checkDeliveryField("bNewlyGiven",2*progressSize+1,1,Kind::Boolean);
    } else if(name=="Client_OnQuestsUpdated") {
        if(actual.size()!=2)throw std::runtime_error("Quest array delivery frame changed");
        checkDeliveryField("UpdatedQuests",0,16,Kind::ProgressArray);
        checkDeliveryField("bFromLoadedState",16,1,Kind::Boolean);
    } else if(name=="Client_NotifyTrackedQuestLoaded") {
        if(actual.size()!=1 && actual.size()!=2)
            throw std::runtime_error("Tracked quest delivery frame changed");
        checkDeliveryField("QuestData",0,8,Kind::Quest);
        if(actual.size()==2)checkDeliveryField("QuestType",8,1,Kind::QuestType);
    } else throw std::runtime_error("Unknown quest delivery callback");
}
}
