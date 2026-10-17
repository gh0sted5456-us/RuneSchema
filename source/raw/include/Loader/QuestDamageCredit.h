#pragma once
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Loader/SummonAttribution.h"
#include "SDK/Helper/ReflectedContract.h"

namespace DragonWilds::Quests {
struct AttributedDeath {RC::Unreal::UObject* Victim=nullptr;RC::Unreal::UObject* Player=nullptr;};
inline AttributedDeath ReadFatalDamage(RC::Unreal::UObject* source,RC::Unreal::UFunction* fn,void* parameters) {
    using namespace RC::Unreal;
    if(!source || !fn || !parameters)return {};
    const auto path=fn->GetPathName();
    const bool point=path==TEXT("/Script/Dominion.DamageComponent:BP_OnPointDamageReceived");
    if(!point && path!=TEXT("/Script/Dominion.DamageComponent:BP_OnDamageReceived"))return {};
    const auto parameter=[&](const TCHAR* name)->FProperty* {
        const FName parameterName(name,FNAME_Add);
        FProperty* found=nullptr;
        for(auto* field:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(field->GetFName()==parameterName && field->HasAnyPropertyFlags(CPF_Parm)) {
            if(found)throw std::runtime_error("Duplicate kill-credit parameter");found=field;
        }
        return found;
    };
    auto* target=CastField<FObjectPropertyBase>(parameter(TEXT("Target")));
    auto* damage=CastField<FStructProperty>(parameter(point?TEXT("PointDamageEvent"):TEXT("DamageEvent")));
    ReflectedContract::Frame(fn);
    if(!ReflectedContract::Input(target,fn) || !ReflectedContract::Input(damage,fn) || target->GetArrayDim()!=1 || target->GetElementSize()!=sizeof(UObject*)
        || damage->GetArrayDim()!=1
        || !target->HasAnyPropertyFlags(CPF_Parm) || !damage->HasAnyPropertyFlags(CPF_Parm))throw std::runtime_error("Kill-credit damage function layout changed");
    auto* type=damage->GetStruct().Get();
    if(!type || damage->GetElementSize()!=type->GetStructureSize() || type->GetPathName()!=(point?TEXT("/Script/Dominion.DominionPointDamageEvent"):TEXT("/Script/Dominion.DominionDamageEvent")))throw std::runtime_error("Kill-credit damage struct changed");
    auto* fatal=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(type,TEXT("bIsFatalHit")));
    auto* instigator=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(type,TEXT("Instigator")));
    if(!ReflectedContract::Fits(fatal,type->GetStructureSize()) || !fatal->IsNativeBool() || fatal->GetByteOffset()!=0 || fatal->GetElementSize()!=1
        || !ReflectedContract::Fits(instigator,type->GetStructureSize()) || instigator->GetElementSize()!=sizeof(UObject*))throw std::runtime_error("Kill-credit instigator/fatal fields changed");
    auto* value=damage->ContainerPtrToValuePtr<void>(parameters);
    if(!fatal->GetPropertyValue(fatal->ContainerPtrToValuePtr<void>(value)))return {};
    auto* victim=target->GetObjectPropertyValue(target->ContainerPtrToValuePtr<void>(parameters));
    auto* player=Summons::CreditedPlayer(instigator->GetObjectPropertyValue(instigator->ContainerPtrToValuePtr<void>(value)));
    if(!player || !victim || source->GetOuterPrivate()!=victim
        || ActorHelper::GetObjectRef(victim,TEXT("AiDamageComponent"))!=source)return {};
    if(!Summons::DeadVictim(victim,player))return {};
    return {victim,player};
}
}
