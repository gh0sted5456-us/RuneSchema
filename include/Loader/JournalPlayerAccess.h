#pragma once
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"

namespace DragonWilds::JournalPlayerAccess {
using namespace RC::Unreal;
inline UFunction* Function(const TCHAR* path,bool unlock,UObject* entry) {
    auto* fn=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
    auto* input=fn?CastField<FObjectPropertyBase>(fn->FindProperty(FName(TEXT("InJournalEntry"),FNAME_Find))):nullptr;
    auto* result=fn?CastField<FBoolProperty>(fn->GetReturnProperty()):nullptr;
    size_t count=0;
    if(fn)for(auto* p:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm))++count;
    if(!fn || fn->GetParmsSize()!=(unlock?10:9) || count!=(unlock?3:2)
        || !input || input->GetArrayDim()!=1 || input->GetElementSize()!=sizeof(UObject*)
        || input->GetOffset_Internal()!=0 || !input->HasAnyPropertyFlags(CPF_Parm)
        || input->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm)
        || !entry || !input->GetPropertyClass().Get() || !entry->IsA(input->GetPropertyClass().Get())
        || !result || !result->IsNativeBool() || result->GetByteOffset()!=0
        || result->GetArrayDim()!=1 || result->GetElementSize()!=1 || result->GetOffset_Internal()!=(unlock?9:8))
        throw std::runtime_error("Journal player-access signature changed");
    if(unlock) {
        auto* unread=CastField<FBoolProperty>(fn->FindProperty(FName(TEXT("bInMarkUnread"),FNAME_Find)));
        if(!unread || !unread->IsNativeBool() || unread->GetByteOffset()!=0
            || unread->GetArrayDim()!=1 || unread->GetElementSize()!=1 || unread->GetOffset_Internal()!=8
            || !unread->HasAnyPropertyFlags(CPF_Parm) || unread->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm))
            throw std::runtime_error("Journal unread-input signature changed");
    }
    return fn;
}
inline bool EnsureUnlocked(UObject* component,UObject* entry) {
    const auto* queryPath=TEXT("/Script/Dominion.JournalComponent:IsJournalEntryUnlocked");
    const auto* unlockPath=TEXT("/Script/Dominion.JournalComponent:UnlockJournalEntry");
    Function(queryPath,false,entry);
    auto* unlock=Function(unlockPath,true,entry);
    auto* componentClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.JournalComponent"));
    if(!component || !componentClass || !component->IsA(componentClass) || !component->GetWorld())
        throw std::runtime_error("Journal player component is not ready");
    const auto isUnlocked=[&] {
        ActorHelper::FunctionCall call(component,queryPath);
        call.Arg(TEXT("InJournalEntry"),entry).Invoke();return call.Result<bool>();
    };
    // Never re-mark an already unlocked/read entry as unread during restoration.
    if(isUnlocked())return false;
    ActorHelper::FunctionCall call(component,unlock->GetPathName());
    call.Arg(TEXT("InJournalEntry"),entry).Arg(TEXT("bInMarkUnread"),true).Invoke();
    if(!isUnlocked())throw std::runtime_error("Native journal unlock was not confirmed");
    return true;
}
}
