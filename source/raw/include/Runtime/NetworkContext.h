#pragma once
#include "Runtime/NetworkRole.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Unreal/World.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include <stdexcept>
namespace PS::Network {
struct Context {
    RC::Unreal::UObject* World=nullptr;
    Role Mode=Role::Unknown;
};
// Call only on the game thread; the returned pointer is not a persistent lease.
inline Context Detect(RC::Unreal::UObject* preferred=nullptr) {
    using namespace RC::Unreal;
    using namespace DragonWilds;
    Context result;
    if(preferred)result.World=preferred->GetWorld();
    if(!result.World) {
        auto* type=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.World"));
        if(!type)return result;
        TArray<UObject*> worlds;
        UECustom::UObjectGlobals::GetObjectsOfClass(type,worlds,true,
            static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization|RF_BeginDestroyed|RF_FinishDestroyed));
        for(auto* world:worlds) {
            if(!world)continue;
            if(!ActorHelper::GetObjectRef(world,TEXT("OwningGameInstance"))
                || !ActorHelper::GetObjectRef(world,TEXT("PersistentLevel")))continue;
            if(result.World) return {}; // Travel/ambiguous worlds must not select the first.
            result.World=world;
        }
    }
    if(!result.World)return result;
    auto* library=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Script/Engine.Default__KismetSystemLibrary"));
    if(!library)return result;
    const auto query=[&](const TCHAR* path) {
        auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path);
        auto* input=function?CastField<FObjectPropertyBase>(function->FindProperty(FName(TEXT("WorldContextObject"),FNAME_Find))):nullptr;
        auto* output=function?CastField<FBoolProperty>(function->GetReturnProperty()):nullptr;
        if(!function || function->GetParmsSize()!=9 || !input || input->GetOffset_Internal()!=0
            || input->GetSize()!=8 || !output || !output->IsNativeBool()
            || output->GetOffset_Internal()!=8 || output->GetSize()!=1)
            throw std::runtime_error("Network-mode query does not match captured layout");
        ActorHelper::FunctionCall call(library,path);
        call.Arg(TEXT("WorldContextObject"),result.World).Invoke();
        return call.Result<bool>();
    };
    const bool standalone=query(TEXT("/Script/Engine.KismetSystemLibrary:IsStandalone"));
    const bool server=query(TEXT("/Script/Engine.KismetSystemLibrary:IsServer"));
    const bool dedicated=query(TEXT("/Script/Engine.KismetSystemLibrary:IsDedicatedServer"));
    result.Mode=Classify(standalone,server,dedicated);
    return result;
}
}
