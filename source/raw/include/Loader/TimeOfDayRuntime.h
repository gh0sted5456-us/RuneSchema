#pragma once
#include "Loader/TimeOfDay.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/Property/FEnumProperty.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>

namespace DragonWilds::TimeOfDay {
inline std::optional<Requirement> ReflectedState(RC::Unreal::UObject* actor) {
    using namespace RC::Unreal;
    for(const auto* name:{TEXT("CachedTimeOfDayState"),TEXT("TimeOfDayState")}) {
        auto* property=PropertyHelper::GetPropertyByName(actor->GetClassPrivate(),name);
        UEnum* enumeration=nullptr;
        int64 value=0;
        if(auto* field=CastField<FEnumProperty>(property);field && field->GetArrayDim()==1 && field->GetUnderlyingProperty()) {
            enumeration=field->GetEnum();
            value=field->GetUnderlyingProperty()->GetSignedIntPropertyValue(field->ContainerPtrToValuePtr<void>(actor));
        } else if(auto* field=CastField<FByteProperty>(property);field && field->GetArrayDim()==1) {
            enumeration=field->GetEnum().Get();
            value=field->GetUnsignedIntPropertyValue(field->ContainerPtrToValuePtr<void>(actor));
        }
        if(!enumeration)continue;
        auto state=RC::to_string(enumeration->GetNameByValue(value).ToString());
        std::transform(state.begin(),state.end(),state.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        if(const auto separator=state.rfind("::");separator!=state.npos)state=state.substr(separator+2);
        if(state.find("night")!=state.npos || state.find("dusk")!=state.npos || state.find("evening")!=state.npos)
            return Requirement::Night;
        if(state.find("day")!=state.npos || state.find("dawn")!=state.npos || state.find("morning")!=state.npos)
            return Requirement::Day;
    }
    return std::nullopt;
}
inline double NumericField(RC::Unreal::UObject* actor,const RC::CharType* name) {
    using namespace RC::Unreal;
    auto* result=actor?CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(actor->GetClassPrivate(),name)):nullptr;
    if(!result || result->GetArrayDim()!=1 || !result->IsFloatingPoint())
        throw std::runtime_error("Native time-of-day field layout changed: "+RC::to_string(name));
    const auto value=result->GetFloatingPointPropertyValue(result->ContainerPtrToValuePtr<void>(actor));
    if(!std::isfinite(value))throw std::runtime_error("Native time-of-day field is non-finite: "+RC::to_string(name));
    return value;
}
inline double NumericGetter(RC::Unreal::UObject* actor,const RC::CharType* path) {
    using namespace RC::Unreal;
    auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
    auto* result=function?CastField<FNumericProperty>(function->GetReturnProperty()):nullptr;
    size_t inputs=0;if(function)for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))
        if(field->HasAnyPropertyFlags(CPF_Parm)&&!field->HasAnyPropertyFlags(CPF_ReturnParm))++inputs;
    if(!actor||!function||inputs||!result||result->GetArrayDim()!=1)
        throw std::runtime_error("Native time-of-day getter layout changed: "+RC::to_string(path));
    ActorHelper::FunctionCall call(actor,function);call.Invoke();const auto value=call.NumericResult();
    if(!std::isfinite(value))throw std::runtime_error("Native time-of-day getter returned a non-finite value");
    return value;
}
inline Requirement Current(RC::Unreal::UObject* context) {
    using namespace RC::Unreal;
    if(!context || !context->GetWorld())throw std::runtime_error("Time-of-day world unavailable");
    auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.InGameTimeActor"));
    if(!type)throw std::runtime_error("InGameTimeActor class unavailable");
    TArray<UObject*> values;UECustom::UObjectGlobals::GetObjectsOfClass(type,values,true);
    UObject* actor=nullptr;
    for(auto* value:values)if(value && value->GetWorld()==context->GetWorld()
        && !value->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))) {
        if(actor)throw std::runtime_error("Live InGameTimeActor is ambiguous");actor=value;
    }
    if(!actor)throw std::runtime_error("Live InGameTimeActor unavailable");
    // Prefer the replicated native phase when present. It is the state used by
    // gameplay and avoids a boundary disagreement while the clock fields are
    // advancing or being corrected by the server.
    if(const auto state=ReflectedState(actor);state.has_value())return *state;
    // The road-torch system observes this actor's native phase transition.
    // Read the same public getters first; reflected fields are a compatibility
    // fallback for builds that do not expose the getter contract.
    try {
        double hour=0,dawn=0,dusk=0;
        try {
            hour=NumericGetter(actor,TEXT("/Script/Dominion.InGameTimeActor:GetTimeOfDay"));
            dawn=NumericGetter(actor,TEXT("/Script/Dominion.InGameTimeActor:GetTimeOfDawn"));
            dusk=NumericGetter(actor,TEXT("/Script/Dominion.InGameTimeActor:GetTimeOfDusk"));
        }catch(...) {
            hour=NumericField(actor,TEXT("StoredTime"));dawn=NumericField(actor,TEXT("TimeOfDawn"));dusk=NumericField(actor,TEXT("TimeOfDusk"));
        }
        const auto normalized=[](double value){value=std::fmod(value,24.0);return value<0?value+24.0:value;};
        const auto h=normalized(hour),a=normalized(dawn),b=normalized(dusk);
        const bool day=a<=b?(h>=a && h<b):(h>=a || h<b);
        return day?Requirement::Day:Requirement::Night;
    } catch(...) {
        throw;
    }
}
inline bool Allows(RC::Unreal::UObject* context,Requirement requirement) {
    return requirement==Requirement::Any || Current(context)==requirement;
}
}
