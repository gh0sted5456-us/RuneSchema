#pragma once
// Native gates for Helpy-owned NPC instances. These checks never modify a class default.
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/AActor.hpp"
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <array>
#include <cstring>
namespace DragonWilds::HelpyNpcGuards {
using namespace RC::Unreal;
inline bool IsNpc(UClass* type) {
    auto* base=ActorHelper::ResolveClass(TEXT("/Script/Dominion.InteractableNPC"));
    return type&&base&&type->IsChildOf(base);
}
inline bool Bounded(UClass* type,FProperty* field) {
    return type&&field&&field->GetArrayDim()==1&&field->GetOffset_Internal()>=0&&field->GetSize()>0
        &&field->GetOffset_Internal()<=type->GetPropertiesSize()
        &&field->GetSize()<=type->GetPropertiesSize()-field->GetOffset_Internal();
}
inline FStructProperty* Guid(UClass* type) {
    auto* p=PropertyHelper::CastProperty<FStructProperty>(PropertyHelper::GetPropertyByName(type,TEXT("SpudGuid")));
    if(!Bounded(type,p)||p->GetSize()!=16||!p->GetStruct()
        ||p->GetStruct()->GetPathName()!=TEXT("/Script/CoreUObject.Guid"))return nullptr;
    return p;
}
inline FBoolProperty* Skip(UClass* type) {
    auto* p=PropertyHelper::CastProperty<FBoolProperty>(PropertyHelper::GetPropertyByName(type,TEXT("bSkipSpudStore")));
    return Bounded(type,p)?p:nullptr;
}
inline void TemporaryClass(UClass* type) {
    if(!IsNpc(type)||ActorHelper::IsAbstract(type)||!Guid(type)||!Skip(type))
        throw std::runtime_error("NPC lacks a verified SpudGuid / bSkipSpudStore contract; timed spawning is blocked.");
}
inline void Exclude(AActor* actor) {
    if(!actor)throw std::runtime_error("No NPC instance to exclude from persistence.");
    auto* type=actor->GetClassPrivate();TemporaryClass(type);
    if(actor->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject)))
        throw std::runtime_error("Never modify an NPC class default or archetype.");
    auto* skip=Skip(type);auto* guid=Guid(type);
    actor->SetFlags(RF_Transient);
    skip->SetPropertyValue(skip->ContainerPtrToValuePtr<void>(actor),true);
    const std::array<uint32_t,4> empty{};
    std::memcpy(guid->ContainerPtrToValuePtr<void>(actor),empty.data(),16);
    if(!skip->GetPropertyValue(skip->ContainerPtrToValuePtr<void>(actor)))
        throw std::runtime_error("NPC persistence exclusion did not apply.");
}
inline void VerifyExcluded(AActor* actor) {
    if(!actor)throw std::runtime_error("NPC instance missing after construction.");
    auto* type=actor->GetClassPrivate();TemporaryClass(type);
    const std::array<uint32_t,4> empty{};
    if(!actor->HasAnyFlags(RF_Transient)||!Skip(type)->GetPropertyValue(Skip(type)->ContainerPtrToValuePtr<void>(actor))
        ||std::memcmp(Guid(type)->ContainerPtrToValuePtr<void>(actor),empty.data(),16)!=0)
        throw std::runtime_error("NPC construction changed its no-save identity; timed spawn rolled back.");
}
} // namespace DragonWilds::HelpyNpcGuards
