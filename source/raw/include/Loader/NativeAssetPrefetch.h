#pragma once
#include "Core/AssetPreloadPlan.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Utility/Logging.h"
#include <cstddef>
#include <cstdint>

namespace DragonWilds {
// Optional overlap of cooked-package I/O. All Unreal calls stay on the calling
// game thread; loading/registration below still verifies every required object.
inline void PrefetchCookedAssets(const PS::AssetPreload::Plan<RC::StringType>& plan) {
    using namespace RC;using namespace RC::Unreal;
    if(plan.Paths().empty())return;
    auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Engine.KismetSystemLibrary:LoadAsset"),false);
    auto* library=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Script/Engine.Default__KismetSystemLibrary"),false);
    auto* contextField=function?CastField<FObjectPropertyBase>(function->FindProperty(FName(TEXT("WorldContextObject"),FNAME_Find))):nullptr;
    auto* assetField=function?CastField<FSoftObjectProperty>(function->FindProperty(FName(TEXT("Asset"),FNAME_Find))):nullptr;
    auto* latentField=function?CastField<FStructProperty>(function->FindProperty(FName(TEXT("LatentInfo"),FNAME_Find))):nullptr;
    auto* delegateField=function?CastField<FDelegateProperty>(function->FindProperty(FName(TEXT("OnLoaded"),FNAME_Find))):nullptr;
    if(!function || !library || !contextField || !assetField || !latentField || !delegateField
        || function->GetReturnProperty() || !function->HasAnyFunctionFlags(FUNC_Static)
        || function->GetParmsSize()>256 || !latentField->GetStruct().Get()
        || latentField->GetStruct()->GetPathName()!=TEXT("/Script/Engine.LatentActionInfo"))return;
    std::size_t count=0;
    for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm)){
        ++count;
        if(field->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm|CPF_ReferenceParm) || field->GetArrayDim()!=1
            || field->GetOffset_Internal()<0 || field->GetElementSize()<=0
            || field->GetOffset_Internal()+field->GetElementSize()>function->GetParmsSize())return;
    }
    if(count!=4)return;
    struct LatentArgs {std::int32_t Linkage=-1,Uuid=0;FName ExecutionFunction{};UObject* CallbackTarget=nullptr;};
    auto* latentType=latentField->GetStruct().Get();
    auto* linkage=CastField<FIntProperty>(PropertyHelper::GetPropertyByName(latentType,TEXT("Linkage")));
    auto* uuid=CastField<FIntProperty>(PropertyHelper::GetPropertyByName(latentType,TEXT("UUID")));
    auto* execution=CastField<FNameProperty>(PropertyHelper::GetPropertyByName(latentType,TEXT("ExecutionFunction")));
    auto* callback=CastField<FObjectProperty>(PropertyHelper::GetPropertyByName(latentType,TEXT("CallbackTarget")));
    if(!linkage || !uuid || !execution || !callback || latentField->GetElementSize()!=sizeof(LatentArgs)
        || contextField->GetElementSize()!=sizeof(UObject*)
        || linkage->GetOffset_Internal()!=offsetof(LatentArgs,Linkage) || linkage->GetElementSize()!=4
        || uuid->GetOffset_Internal()!=offsetof(LatentArgs,Uuid) || uuid->GetElementSize()!=4
        || execution->GetOffset_Internal()!=offsetof(LatentArgs,ExecutionFunction) || execution->GetElementSize()!=sizeof(FName)
        || callback->GetOffset_Internal()!=offsetof(LatentArgs,CallbackTarget) || callback->GetElementSize()!=sizeof(UObject*))return;
    auto* instanceType=ActorHelper::ResolveClass(TEXT("/Script/Engine.GameInstance"));
    if(!instanceType)return;
    TArray<UObject*> instances;UECustom::UObjectGlobals::GetObjectsOfClass(instanceType,instances,true);
    UObject* context=nullptr;
    for(auto* instance:instances)if(instance && !instance->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization)) && instance->GetWorld()){context=instance;break;}
    if(!context)return;
    // A unique latent slot per asset, unbound delegate and INDEX_NONE linkage:
    // nothing is dispatched to a fabricated Blueprint completion function.
    static std::int32_t nextUuid=0x52530000;
    std::size_t queued=0;
    // Bound outstanding latent handles during startup. This is an overlap hint,
    // not a replacement for the required loader or a reason to skip any asset.
    constexpr std::size_t MaxQueuedAssets=512;
    for(const auto& path:plan.Paths()){
        if(queued==MaxQueuedAssets)break;
        if(UECustom::UObjectGlobals::StaticFindObject(nullptr,nullptr,path.c_str(),false))continue;
        if(nextUuid==0x7ffffffe)break;
        try {
            ActorHelper::FunctionCall request(library,function);
            const LatentArgs latent{-1,++nextUuid,FName{},context};
            request.Arg(TEXT("WorldContextObject"),context).JsonArg(TEXT("Asset"),RC::to_string(path))
                .Arg(TEXT("LatentInfo"),latent).Invoke();
            ++queued;
        }catch(const std::exception& error){
            PS::Log<LogLevel::Normal>(STR("[PERF][ASSETS] Async prefetch stopped: {}. Normal required-asset loading remains active.\n"),PS::ToWideSafe(error.what()));break;
        }
    }
    if(queued)PS::Log<LogLevel::Normal>(STR("[PERF][ASSETS] Queued {} cooked assets for native async prefetch; consumers still validate required assets before use.\n"),queued);
}
}
