#pragma once
#include "Loader/SummonAttribution.h"
#include <vector>

namespace DragonWilds::Quests::Summons {
// Optional, generic cooked-mod observer. This is not a reward dispatcher or RPC.
// Only components already attached to the credited authoritative player can opt in.
inline void NotifyConfirmedKill(RC::Unreal::UObject* player,RC::Unreal::UObject* victim) {
    using namespace RC::Unreal;
    if(!PlayerController(player) || !Live(victim) || victim->GetWorld()!=player->GetWorld())return;
    auto* componentType=ActorHelper::ResolveClass(TEXT("/Script/Engine.ActorComponent"));
    auto* actorType=ActorHelper::ResolveClass(TEXT("/Script/Engine.Actor"));
    if(!componentType || !actorType)return;
    const PS::WeakObjectHandle playerHandle(player),victimHandle(victim);
    std::vector<PS::WeakObjectHandle> listeners;
    for(auto* component:static_cast<AActor*>(player)->GetComponentsByClass(componentType)) {
        if(!Live(component) || component->GetOuterPrivate()!=player)continue;
        auto* enabled=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(component->GetClassPrivate(),TEXT("bRuneSchemaConfirmedKillListener")));
        if(!enabled || enabled->GetArrayDim()!=1 || !enabled->GetPropertyValue(enabled->ContainerPtrToValuePtr<void>(component)))continue;
        if(listeners.size()>=64)break;
        listeners.emplace_back(component);
    }
    // Snapshot weak identities: one listener may destroy itself or another listener.
    for(const auto& handle:listeners) {
        auto* component=handle.Get();player=playerHandle.Get();victim=victimHandle.Get();
        if(!PlayerController(player) || !Live(victim) || victim->GetWorld()!=player->GetWorld())break;
        if(!Live(component) || component->GetOuterPrivate()!=player || component->GetWorld()!=player->GetWorld())continue;
        auto* function=component->GetFunctionByNameInChain(TEXT("RuneSchema_OnConfirmedKill"));
        if(!function || function->HasAnyFunctionFlags(FUNC_Net) || function->GetParmsSize()!=16 || function->GetReturnProperty())continue;
        size_t count=0;bool valid=true;
        for(auto* property:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(property->HasAnyPropertyFlags(CPF_Parm)) {
            ++count;auto* object=CastField<FObjectPropertyBase>(property);
            const bool killed=property->GetFName()==FName(TEXT("Victim"),FNAME_Add);
            const bool credited=property->GetFName()==FName(TEXT("CreditedPlayer"),FNAME_Add);
            valid&=(killed || credited) && object && !property->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm|CPF_ReferenceParm)
                && property->GetArrayDim()==1 && property->GetElementSize()==8
                && property->GetOffset_Internal()==(killed?0:8)
                && object->GetPropertyClass().Get()==actorType;
        }
        if(!valid || count!=2)continue;
        try {
            ActorHelper::FunctionCall(component,function).Arg(TEXT("Victim"),victim).Arg(TEXT("CreditedPlayer"),player).Invoke();
        } catch(const std::exception& error) {
            PS::Log<RC::LogLevel::Warning>(STR("Confirmed-kill listener failed: {}\n"),PS::ToWideSafe(error.what()));
        }
    }
}
}
