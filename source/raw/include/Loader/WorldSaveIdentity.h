#pragma once
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/ReflectedContract.h"
#include "Loader/Spawn/RuntimeSupport.h"
#include "Generator/SaveReport.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include <cstring>
#include <format>
#include <stdexcept>
#include <string>
// The loaded world's save GUID (Dominion PersistenceSubsystem.WorldSaveSettings.WorldSaveGuid),
// read through the verified reflected layout. Available where the world save is loaded, which is
// the authority (single player, listen host, dedicated server). purpose prefixes the errors.
namespace DragonWilds::WorldSaveIdentity {
inline std::string Guid(RC::Unreal::UObject* context,const std::string& purpose){
    using namespace RC::Unreal;
    if(!context||!context->GetWorld())throw std::runtime_error(purpose+" world is unavailable");
    auto* instance=SpawnRuntime::CallWorldContextGetter(TEXT("/Script/Engine.GameplayStatics:GetGameInstance"),TEXT("/Script/Engine.Default__GameplayStatics"),context);
    auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.PersistenceSubsystem"));
    if(!instance||!type)throw std::runtime_error(purpose+" persistence subsystem unavailable");
    TArray<UObject*> candidates;UECustom::UObjectGlobals::GetObjectsOfClass(type,candidates,true);
    if(candidates.Num()<0||candidates.Num()>64)throw std::runtime_error(purpose+" persistence roster is invalid");
    UObject* subsystem=nullptr;
    for(auto* candidate:candidates)if(candidate&&candidate->GetOuterPrivate()==instance
        &&!candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))){
        if(subsystem)throw std::runtime_error(purpose+" persistence subsystem is ambiguous");
        subsystem=candidate;
    }
    auto* settings=subsystem?CastField<FStructProperty>(PropertyHelper::GetPropertyByName(type,TEXT("WorldSaveSettings"))):nullptr;
    auto* schema=settings?settings->GetStruct().Get():nullptr;
    auto* guid=schema?CastField<FStructProperty>(PropertyHelper::GetPropertyByName(schema,TEXT("WorldSaveGuid"))):nullptr;
    if(!schema||schema->GetPathName()!=TEXT("/Script/Dominion.DomGameWorldSettings")
        ||!ReflectedContract::Fits(settings,type->GetPropertiesSize())||settings->GetArrayDim()!=1
        ||settings->GetElementSize()!=schema->GetStructureSize()||!guid||guid->GetArrayDim()!=1
        ||!guid->GetStruct().Get()||guid->GetStruct()->GetPathName()!=TEXT("/Script/CoreUObject.Guid")
        ||guid->GetElementSize()!=16||!ReflectedContract::Fits(guid,schema->GetStructureSize()))
        throw std::runtime_error(purpose+" world-save GUID contract changed");
    uint32_t words[4]{};
    std::memcpy(words,guid->ContainerPtrToValuePtr<void>(settings->ContainerPtrToValuePtr<void>(subsystem)),sizeof(words));
    const auto world=PS::SaveReport::Guid(std::format("{:08X}{:08X}{:08X}{:08X}",words[0],words[1],words[2],words[3]));
    if(world.empty())throw std::runtime_error(purpose+" is waiting for the loaded world-save GUID");
    return world;
}
}
