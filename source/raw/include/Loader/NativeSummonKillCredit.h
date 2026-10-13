#pragma once
#include "Loader/SummonAttribution.h"
#include "Unreal/Hooks.hpp"
#include <vector>

namespace DragonWilds::Quests::Summons {
// Native progress remains the sole dispatcher. This adapter only changes the
// credited instigator on the caster's existing callback, then restores the
// borrowed parameter buffer before the caller can reuse it for another player.
class NativeKillCredit {
    struct Frame {
        RC::Unreal::UObject* Source;
        RC::Unreal::UFunction* Function;
        void* Parameters;
        RC::Unreal::FObjectPropertyBase* Instigator=nullptr;
        RC::Unreal::FObjectPropertyBase* DamageInstigator=nullptr;
        void* Damage=nullptr;
        RC::Unreal::UObject* Original=nullptr;
        uint64_t Epoch=0;
        ObjectIdentity World,Victim,Player;
        bool Reserved=false,Changed=false;
    };
    NativeDispatches m_dispatches;
    std::vector<Frame> m_frames;
    unsigned m_checks=0;
    static ObjectIdentity Identity(RC::Unreal::UObject* object) {
        if(!Live(object))return {};
        const auto weak=PS::WeakObject(object);return {weak.ObjectIndex,weak.ObjectSerialNumber};
    }
    static bool IsLive(ObjectIdentity id) {
        auto* slot=RC::Unreal::FUObjectArray::IndexToObject(id.Index);
        return slot && slot->GetSerialNumber()==id.Serial && Live(slot->GetUObject());
    }
public:
    void Before(RC::Unreal::Hook::TCallbackIterationData<void>& dispatch,RC::Unreal::UObject* source,
        RC::Unreal::UFunction* function,void* parameters,uint64_t epoch) {
        using namespace RC::Unreal;
        if(!source || !function || !parameters
            || function->GetPathName()!=TEXT("/Script/Dominion.ProgressComponent:OnAIKilled"))return;
        // Even an ignored or suppressed nested call needs its own frame. It
        // must not restore its parent's borrowed buffer prematurely.
        m_frames.push_back({source,function,parameters});
        const auto frameIndex=m_frames.size()-1;
        if(dispatch.OriginalFunctionCallPrevented())return;
        auto* victimField=CastField<FObjectPropertyBase>(function->FindProperty(FName(TEXT("KilledCharacter"),FNAME_Find)));
        auto* instigator=CastField<FObjectPropertyBase>(function->FindProperty(FName(TEXT("Instigator"),FNAME_Find)));
        auto* event=CastField<FStructProperty>(function->FindProperty(FName(TEXT("DamageEvent"),FNAME_Find)));
        auto* type=event?event->GetStruct().Get():nullptr;
        if(function->GetParmsSize()!=256 || function->GetReturnProperty() || !victimField || !instigator || !event
            || victimField->GetArrayDim()!=1 || victimField->GetElementSize()!=8 || victimField->GetOffset_Internal()!=0
            || instigator->GetArrayDim()!=1 || instigator->GetElementSize()!=8 || instigator->GetOffset_Internal()!=8
            || event->GetArrayDim()!=1 || event->GetElementSize()!=240 || event->GetOffset_Internal()!=16
            || !type || type->GetPathName()!=TEXT("/Script/Dominion.DominionDamageEvent"))
            throw std::runtime_error("Native summon kill-credit function layout changed");
        auto* damageInstigator=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(type,TEXT("Instigator")));
        auto* fatal=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(type,TEXT("bIsFatalHit")));
        if(!damageInstigator || damageInstigator->GetArrayDim()!=1 || damageInstigator->GetElementSize()!=8 || damageInstigator->GetOffset_Internal()!=64
            || !fatal || !fatal->IsNativeBool() || fatal->GetArrayDim()!=1 || fatal->GetElementSize()!=1 || fatal->GetOffset_Internal()!=224)
            throw std::runtime_error("Native summon damage-event fields changed");
        auto* damage=event->ContainerPtrToValuePtr<void>(parameters);
        if(!fatal->GetPropertyValue(fatal->ContainerPtrToValuePtr<void>(damage)))return;
        auto* attacker=instigator->GetObjectPropertyValue(instigator->ContainerPtrToValuePtr<void>(parameters));
        if(attacker!=damageInstigator->GetObjectPropertyValue(damageInstigator->ContainerPtrToValuePtr<void>(damage)))return;
        auto* player=CreditedPlayer(attacker);
        auto* controller=player?PlayerController(player):nullptr;
        auto* progressType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ProgressComponent"));
        if(!controller || !Live(source) || !progressType || !source->IsA(progressType) || source->GetOuterPrivate()!=controller
            || ActorHelper::GetObjectRef(controller,TEXT("ProgressComponent"))!=source)return;
        auto* victim=victimField->GetObjectPropertyValue(victimField->ContainerPtrToValuePtr<void>(parameters));
        if(victim==attacker || !DeadVictim(victim,player))return;
        const bool summon=attacker!=player;
        if(++m_checks%256==0)m_dispatches.Prune(IsLive);
        const auto world=Identity(player->GetWorld()),victimId=Identity(victim),playerId=Identity(player);
        const auto decision=m_dispatches.Reserve(epoch,world,victimId,playerId,summon);
        if(decision==NativeDispatches::Decision::Rejected)return;
        if(decision==NativeDispatches::Decision::Duplicate){dispatch.PreventOriginalFunctionCall();return;}
        auto& frame=m_frames[frameIndex];
        frame.Epoch=epoch;frame.World=world;frame.Victim=victimId;frame.Player=playerId;
        frame.Reserved=decision==NativeDispatches::Decision::First;
        if(!summon)return;
        frame.Instigator=instigator;frame.DamageInstigator=damageInstigator;frame.Damage=damage;frame.Original=attacker;frame.Changed=true;
        instigator->SetObjectPropertyValue(instigator->ContainerPtrToValuePtr<void>(parameters),player);
        damageInstigator->SetObjectPropertyValue(damageInstigator->ContainerPtrToValuePtr<void>(damage),player);
    }
    void After(RC::Unreal::Hook::TCallbackIterationData<void>& dispatch,RC::Unreal::UObject* source,RC::Unreal::UFunction* function,void* parameters) {
        if(m_frames.empty() || !function || function->GetPathName()!=TEXT("/Script/Dominion.ProgressComponent:OnAIKilled"))return;
        for(size_t index=m_frames.size();index;--index) {
            const auto frame=m_frames[index-1];
            if(frame.Source!=source || frame.Function!=function || frame.Parameters!=parameters)continue;
            if(frame.Changed) {
                frame.Instigator->SetObjectPropertyValue(frame.Instigator->ContainerPtrToValuePtr<void>(parameters),frame.Original);
                frame.DamageInstigator->SetObjectPropertyValue(frame.DamageInstigator->ContainerPtrToValuePtr<void>(frame.Damage),frame.Original);
            }
            // Another mod may veto the original after our pre-hook. Permit a
            // later real dispatch rather than recording an award that never ran.
            if(frame.Reserved && dispatch.OriginalFunctionCallPrevented())
                m_dispatches.Cancel(frame.Epoch,frame.World,frame.Victim,frame.Player);
            m_frames.erase(m_frames.begin()+index-1);return;
        }
    }
};
}
