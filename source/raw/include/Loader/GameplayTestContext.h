#pragma once
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/UnrealFlags.hpp"

namespace DragonWilds {
// Quest state belongs only to a fully possessed gameplay controller.  Menu
// controllers and character-preview pawns can be locally controlled briefly
// during map transitions, but they do not own the live quest component.
inline bool IsGameplayQuestController(RC::Unreal::UObject* controller,
    RC::Unreal::AActor* expectedPawn = nullptr) {
    using namespace RC::Unreal;
    const auto invalid = static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject
        | RF_NeedLoad | RF_NeedPostLoad | RF_NeedInitialization
        | RF_BeginDestroyed | RF_FinishDestroyed);
    if (!controller || !controller->GetWorld() || controller->HasAnyFlags(invalid)) return false;
    auto* controllerType = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr,
        TEXT("/Script/Dominion.DominionPlayerController"));
    auto* pawnType = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr,
        TEXT("/Script/Dominion.DominionPlayerCharacter"));
    if (!controllerType || !pawnType || !controller->IsA(controllerType)) return false;
    auto* pawn = ActorHelper::GetObjectRef(controller, TEXT("Pawn"));
    if (!pawn || (expectedPawn && pawn != expectedPawn) || !pawn->IsA(pawnType)
        || pawn->GetWorld() != controller->GetWorld() || pawn->HasAnyFlags(invalid)
        || ActorHelper::GetObjectRef(pawn, TEXT("Controller")) != controller) return false;
    auto* component = ActorHelper::GetObjectRef(controller, TEXT("QuestProgressComponent"));
    return component && component->GetOuterPrivate() == controller
        && component->GetWorld() == controller->GetWorld() && !component->HasAnyFlags(invalid);
}

// Test candidates target the locally possessed gameplay pawn, never the first
// arbitrary actor in the object array. No dedicated-server/client automation.
inline RC::Unreal::AActor* FindLocalGameplayTestPawn() {
    using namespace RC::Unreal;
    auto* type = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr,
        TEXT("/Script/Dominion.DominionPlayerCharacter"));
    if (!type) return nullptr;
    TArray<UObject*> objects;
    UECustom::UObjectGlobals::GetObjectsOfClass(type, objects, true,
        static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject | RF_NeedLoad
            | RF_NeedPostLoad | RF_NeedInitialization | RF_BeginDestroyed | RF_FinishDestroyed));
    for (auto* object : objects) {
        if (!object || !object->GetWorld()) continue;
        ActorHelper::FunctionCall local(object, TEXT("/Script/Engine.Pawn:IsLocallyControlled"));
        local.Invoke();
        if (!local.Result<bool>()) continue;
        auto* controller = ActorHelper::GetObjectRef(object, TEXT("Controller"));
        if (!IsGameplayQuestController(controller, static_cast<AActor*>(object))) continue;
        ActorHelper::FunctionCall authority(object, TEXT("/Script/Engine.Actor:HasAuthority"));
        authority.Invoke();
        if (authority.Result<bool>()) return static_cast<AActor*>(object);
    }
    return nullptr;
}
}
