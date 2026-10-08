#pragma once
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"

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
    if(fn->GetParmsSize()!=(point?464:248) || !target || !damage || target->GetArrayDim()!=1 || target->GetElementSize()!=8 || target->GetOffset_Internal()!=0
        || damage->GetArrayDim()!=1 || damage->GetOffset_Internal()!=(point?16:8) || damage->GetElementSize()!=(point?448:240)
        || !target->HasAnyPropertyFlags(CPF_Parm) || !damage->HasAnyPropertyFlags(CPF_Parm))throw std::runtime_error("Kill-credit damage function layout changed");
    auto* type=damage->GetStruct().Get();
    if(!type || type->GetPathName()!=(point?TEXT("/Script/Dominion.DominionPointDamageEvent"):TEXT("/Script/Dominion.DominionDamageEvent")))throw std::runtime_error("Kill-credit damage struct changed");
    auto* fatal=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(type,TEXT("bIsFatalHit")));
    auto* instigator=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(type,TEXT("Instigator")));
    if(!fatal || !fatal->IsNativeBool() || fatal->GetByteOffset()!=0 || fatal->GetArrayDim()!=1 || fatal->GetElementSize()!=1 || fatal->GetOffset_Internal()!=224
        || !instigator || instigator->GetArrayDim()!=1 || instigator->GetElementSize()!=8 || instigator->GetOffset_Internal()!=64)throw std::runtime_error("Kill-credit instigator/fatal fields changed");
    auto* value=damage->ContainerPtrToValuePtr<void>(parameters);
    if(!fatal->GetPropertyValue(fatal->ContainerPtrToValuePtr<void>(value)))return {};
    auto* victim=target->GetObjectPropertyValue(target->ContainerPtrToValuePtr<void>(parameters));
    auto* player=instigator->GetObjectPropertyValue(instigator->ContainerPtrToValuePtr<void>(value));
    auto* aiClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionAICharacter"));
    auto* playerClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));
    if(!victim || !player || !aiClass || !playerClass || !victim->IsA(aiClass) || !player->IsA(playerClass)
        || !victim->GetWorld() || victim->GetWorld()!=player->GetWorld() || source->GetOuterPrivate()!=victim
        || ActorHelper::GetObjectRef(victim,TEXT("AiDamageComponent"))!=source)return {};
    auto* health=ActorHelper::GetObjectRef(victim,TEXT("HealthComponent"));
    if(!health || health->GetOuterPrivate()!=victim)return {};
    auto* isDead=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Dominion.HealthComponent:IsDead"),false);
    auto* result=isDead?CastField<FBoolProperty>(isDead->GetReturnProperty()):nullptr;
    if(!isDead || isDead->GetParmsSize()!=1 || !result || !result->IsNativeBool() || result->GetArrayDim()!=1 || result->GetOffset_Internal()!=0 || result->GetElementSize()!=1)throw std::runtime_error("Kill-credit IsDead layout changed");
    ActorHelper::FunctionCall dead(health,isDead->GetPathName());dead.Invoke();if(!dead.Result<bool>())return {};
    ActorHelper::FunctionCall authority(player,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    if(!authority.Result<bool>())return {};
    auto* controller=ActorHelper::GetObjectRef(player,TEXT("Controller"));
    auto* controllerType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerController"));
    if(!controller || !controllerType || !controller->IsA(controllerType) || controller->GetWorld()!=player->GetWorld()
        || ActorHelper::GetObjectRef(controller,TEXT("Pawn"))!=player)return {};
    return {victim,player};
}
}
