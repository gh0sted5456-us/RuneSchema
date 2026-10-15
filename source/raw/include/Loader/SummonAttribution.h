#pragma once
#include "SDK/Helper/ReflectedContract.h"
#include "Loader/SummonKillCreditPolicy.h"
#include "Runtime/NetworkContext.h"
#include "SDK/WeakObjectHandle.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"

namespace DragonWilds::Quests::Summons {
inline bool Live(RC::Unreal::UObject* object) {
    using namespace RC::Unreal;
    if(!object || object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization|RF_BeginDestroyed|RF_FinishDestroyed)))return false;
    auto* slot=FUObjectArray::IndexToObject(object->GetInternalIndex());
    return slot && slot->GetUObject()==object && slot->IsValid(false);
}
inline bool Authority(RC::Unreal::UObject* actor) {
    ActorHelper::FunctionCall call(actor,TEXT("/Script/Engine.Actor:HasAuthority"));call.Invoke();return call.Result<bool>();
}
inline RC::Unreal::UObject* PlayerController(RC::Unreal::UObject* player) {
    using namespace RC::Unreal;
    auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));
    if(!Live(player) || !type || !player->IsA(type) || !player->GetWorld() || !Authority(player)
        || !PS::Network::OwnsGameplay(PS::Network::Detect(player).Mode))return nullptr;
    auto* controller=ActorHelper::GetObjectRef(player,TEXT("Controller"));
    auto* controllerType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerController"));
    if(!Live(controller) || !controllerType || !controller->IsA(controllerType) || controller->GetWorld()!=player->GetWorld()
        || ActorHelper::GetObjectRef(controller,TEXT("Pawn"))!=player || !Authority(controller))return nullptr;
    return controller;
}
inline bool NativeBool(RC::Unreal::UObject* actor,const TCHAR* path) {
    using namespace RC::Unreal;
    auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
    auto* result=function?CastField<FBoolProperty>(function->GetReturnProperty()):nullptr;
    ReflectedContract::Frame(function);
    if(!ReflectedContract::Output(result,function) || !result->IsNativeBool()
        || result->GetArrayDim()!=1 || result->GetElementSize()!=1 || ReflectedContract::Count(function)!=1)
        throw std::runtime_error("Summon native allegiance function changed");
    ActorHelper::FunctionCall query(actor,function);query.Invoke();return query.Result<bool>();
}
inline bool CertifiedAllegiance(RC::Unreal::UObject* attacker,RC::Unreal::UObject* caster) {
    using namespace RC::Unreal;
    // RuneSchema certifies the game's current allegiance itself; no named mod,
    // custom certificate property, AI name or cosmetic identity is trusted.
    auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Dominion.DominionAiFunctionLibrary:GetAttitude"),false);
    auto* library=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Script/Dominion.Default__DominionAiFunctionLibrary"),false);
    auto* ai=function?CastField<FObjectPropertyBase>(function->FindProperty(FName(TEXT("AI"),FNAME_Find))):nullptr;
    auto* target=function?CastField<FObjectPropertyBase>(function->FindProperty(FName(TEXT("Target"),FNAME_Find))):nullptr;
    auto* result=function?CastField<FByteProperty>(function->GetReturnProperty()):nullptr;
    ReflectedContract::Frame(function);
    if(!library || ReflectedContract::Count(function)!=3 || !ReflectedContract::Input(ai,function) || !ReflectedContract::Input(target,function)
        || ai->GetElementSize()!=sizeof(UObject*) || target->GetElementSize()!=sizeof(UObject*)
        || !ReflectedContract::Output(result,function) || result->GetElementSize()!=1)
        throw std::runtime_error("Summon native attitude function changed");
    ActorHelper::FunctionCall attitude(library,function);attitude.Arg(TEXT("AI"),attacker).Arg(TEXT("Target"),caster).Invoke();
    const NativeAllegiance state{attitude.Result<uint8_t>()==0,
        NativeBool(attacker,TEXT("/Script/Dominion.DominionAICharacter:IsConfused")),
        NativeBool(attacker,TEXT("/Script/Dominion.DominionAICharacter:ShouldFollowConfuser"))};
    return state.Certified();
}
inline RC::Unreal::UObject* ResolveCaster(RC::Unreal::UObject* attacker) {
    using namespace RC::Unreal;
    auto* ai=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionAICharacter"));
    if(!Live(attacker) || !ai || !attacker->IsA(ai) || !Authority(attacker))return nullptr;
    auto* caster=ActorHelper::GetObjectRef(attacker,TEXT("Owner"));
    if(!PlayerController(caster) || caster->GetWorld()!=attacker->GetWorld()
        || ActorHelper::GetObjectRef(attacker,TEXT("Instigator"))!=caster)return nullptr;
    // Native follower enrollment supports the base component and any subclass.
    // A subordinate must have its own slot attached directly to the caster.
    auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.FollowerComponent"));
    if(!type)return nullptr;
    auto* follower=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(type,TEXT("Follower")));
    if(!follower || follower->GetArrayDim()!=1 || follower->GetElementSize()!=sizeof(UObject*))
        throw std::runtime_error("Summon follower enrollment fields changed");
    size_t matches=0;
    for(auto* component:static_cast<AActor*>(caster)->GetComponentsByClass(type)) {
        if(!Live(component) || !component->IsA(type) || component->GetOuterPrivate()!=caster)continue;
        ActorHelper::FunctionCall owner(component,TEXT("/Script/Engine.ActorComponent:GetOwner"));owner.Invoke();
        const auto enrolled=follower->GetObjectPropertyValue(follower->ContainerPtrToValuePtr<void>(component));
        if(MatchesEnrollment(attacker,caster,ActorHelper::GetObjectRef(attacker,TEXT("Owner")),
            ActorHelper::GetObjectRef(attacker,TEXT("Instigator")),owner.Result<UObject*>(),enrolled,true))++matches;
    }
    return matches==1 && CertifiedAllegiance(attacker,caster)?caster:nullptr;
}
inline RC::Unreal::UObject* CreditedPlayer(RC::Unreal::UObject* instigator) {
    return PlayerController(instigator)?instigator:ResolveCaster(instigator);
}
inline bool DeadVictim(RC::Unreal::UObject* victim,RC::Unreal::UObject* player) {
    using namespace RC::Unreal;
    auto* ai=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionAICharacter"));
    if(!Live(victim) || !ai || !victim->IsA(ai) || !Live(player) || !victim->GetWorld()
        || victim->GetWorld()!=player->GetWorld() || !Authority(victim))return false;
    auto* health=ActorHelper::GetObjectRef(victim,TEXT("HealthComponent"));
    if(!Live(health) || health->GetOuterPrivate()!=victim)return false;
    auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Dominion.HealthComponent:IsDead"),false);
    auto* result=function?CastField<FBoolProperty>(function->GetReturnProperty()):nullptr;
    ReflectedContract::Frame(function);
    if(ReflectedContract::Count(function)!=1 || !ReflectedContract::Output(result,function) || !result->IsNativeBool() || result->GetArrayDim()!=1 || result->GetElementSize()!=1)
        throw std::runtime_error("Kill-credit IsDead layout changed");
    ActorHelper::FunctionCall dead(health,function);dead.Invoke();return dead.Result<bool>();
}
}
