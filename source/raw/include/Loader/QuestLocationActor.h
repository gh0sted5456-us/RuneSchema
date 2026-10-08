#pragma once
#include "Loader/QuestDefinition.h"
#include "Loader/QuestObjectReference.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/UObjectArray.hpp"
#include "Unreal/World.hpp"

namespace DragonWilds::Quests {
class LocationActor {
    using UObject=RC::Unreal::UObject;
    using AActor=RC::Unreal::AActor;
    using UWorld=RC::Unreal::UWorld;
    AActor* actor=nullptr;
    int32_t index=-1,serial=0;
    static void ConfigureArea(AActor* actor,double meters) {
        using namespace RC::Unreal;
        auto* cls=actor->GetClassPrivate();
        auto* radius=CastField<FFloatProperty>(PropertyHelper::GetPropertyByName(cls,TEXT("Radius")));
        auto* type=CastField<FByteProperty>(PropertyHelper::GetPropertyByName(cls,TEXT("Type")));
        auto* enumeration=type?type->GetEnum().Get():nullptr;
        if(!radius || radius->GetArrayDim()!=1 || radius->GetElementSize()!=sizeof(float)
            || !type || type->GetArrayDim()!=1 || type->GetElementSize()!=1 || !enumeration
            || enumeration->GetPathName()!=TEXT("/Script/Dominion.EQuestLocationType"))
            throw std::runtime_error("Quest area type/radius layout changed");
        const auto& names=enumeration->GetEnumNames();
        if(names.Num()<1 || names.Num()>64)throw std::runtime_error("Quest location enum count is invalid");
        std::optional<uint8_t> area;
        for(const auto& entry:names)if(entry.Key.ToString()==TEXT("EQuestLocationType::Area") || entry.Key.ToString()==TEXT("Area")) {
            if(area || entry.Value<0 || entry.Value>255)throw std::runtime_error("Quest area enum value is ambiguous or invalid");
            area=static_cast<uint8_t>(entry.Value);
        }
        if(!area)throw std::runtime_error("Native quest location Area enum value unavailable");
        const float centimeters=RadiusCentimeters(meters);
        radius->CopyCompleteValue(radius->ContainerPtrToValuePtr<void>(actor),&centimeters);
        type->CopyCompleteValue(type->ContainerPtrToValuePtr<void>(actor),&*area);
        if(radius->GetPropertyValue(radius->ContainerPtrToValuePtr<void>(actor))!=centimeters
            || type->GetPropertyValue(type->ContainerPtrToValuePtr<void>(actor))!=*area)
            throw std::runtime_error("Quest area configuration did not round-trip");
    }
    static UObject* Subsystem(UWorld* world) {
        using namespace RC::Unreal;
        auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.QuestWorldSubsystem"));
        if(!type)throw std::runtime_error("Quest location subsystem class unavailable");
        TArray<UObject*> objects;UECustom::UObjectGlobals::GetObjectsOfClass(type,objects,true);
        UObject* result=nullptr;
        for(auto* value:objects)if(value && value->GetOuterPrivate()==world
            && !value->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))) {
            if(result)throw std::runtime_error("Quest location subsystem is ambiguous");
            result=value;
        }
        if(!result)throw std::runtime_error("Quest location subsystem unavailable for this world");
        return result;
    }
    static size_t Membership(UObject* subsystem,AActor* target) {
        using namespace RC::Unreal;
        auto* array=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(subsystem->GetClassPrivate(),TEXT("QuestLocations")));
        auto* inner=array?CastField<FObjectPropertyBase>(array->GetInner()):nullptr;
        if(!array || array->GetArrayDim()!=1 || !inner || inner->GetArrayDim()!=1 || inner->GetElementSize()!=sizeof(UObject*)
            || !inner->GetPropertyClass().Get() || !target->IsA(inner->GetPropertyClass().Get()))
            throw std::runtime_error("Quest location registry layout changed");
        auto* raw=array->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        if(raw->Num()<0 || raw->Num()>65536)throw std::runtime_error("Quest location registry count is invalid");
        UECustom::FScriptArrayHelper view(raw,array);
        size_t found=0;
        view.ForEachElement([&](void* entry){if(inner->GetObjectPropertyValue(entry)==target)++found;});
        return found;
    }
public:
    LocationActor()=default;
    LocationActor(const LocationActor&)=delete;
    LocationActor& operator=(const LocationActor&)=delete;
    AActor* Get() const {
        using namespace RC::Unreal;
        auto* slot=index<0?nullptr:FUObjectArray::IndexToObject(index);
        if(!slot || slot->GetUObject()!=actor || slot->GetSerialNumber()!=serial || !slot->IsValid(false))return nullptr;
        if(actor->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))return nullptr;
        return actor;
    }
    void Create(UWorld* world,UObject* quest,const Location& location) {
        using namespace RC::Unreal;
        if(Get())throw std::runtime_error("Quest location already owned");
        auto* subsystem=Subsystem(world);
        auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.QuestLocation"));
        if(!type || !quest)throw std::runtime_error("Quest location class or asset unavailable");
        auto* spawned=ActorHelper::SpawnActor(world,type,FVector(location.Position[0],location.Position[1],location.Position[2]),FRotator(0,0,0),[&](AActor* value){
            value->SetFlags(RF_Transient);
            ActorHelper::FunctionCall(value,TEXT("/Script/Engine.Actor:SetReplicates"))
                .Arg(TEXT("bInReplicates"),false).Invoke();
            auto* object=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(type,TEXT("Quest")));
            auto* name=CastField<FNameProperty>(PropertyHelper::GetPropertyByName(type,TEXT("QuestLocationName")));
            auto* shown=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(type,TEXT("bShownByDefault")));
            if(!object || object->GetElementSize()!=sizeof(UObject*) || object->GetArrayDim()!=1
                || !object->GetPropertyClass().Get() || !quest->IsA(object->GetPropertyClass().Get())
                || !name || name->GetArrayDim()!=1 || name->GetElementSize()!=sizeof(FName) || !shown || shown->GetArrayDim()!=1)
                throw std::runtime_error("Quest location configuration layout changed");
            QuestRegistry::ValidateObjectReferenceLayout(RC::to_string(object->GetClass().GetName()),object->GetElementSize(),object->GetArrayDim(),sizeof(UObject*));
            object->CopyCompleteValue(object->ContainerPtrToValuePtr<void>(value),&quest);
            if(object->GetObjectPropertyValue(object->ContainerPtrToValuePtr<void>(value))!=quest)
                throw std::runtime_error("Quest location asset copy failed");
            const FName id(RC::to_generic_string(location.Name).c_str(),FNAME_Add);
            name->CopyCompleteValue(name->ContainerPtrToValuePtr<void>(value),&id);
            shown->SetPropertyValue(shown->ContainerPtrToValuePtr<void>(value),true);
            if(location.RadiusMeters)ConfigureArea(value,*location.RadiusMeters);
            ActorHelper::FunctionCall(value,TEXT("/Script/Engine.Actor:SetActorEnableCollision"))
                .Arg(TEXT("bNewActorEnableCollision"),false).Invoke();
        });
        try {
            auto* slot=FUObjectArray::IndexToObject(spawned->GetInternalIndex());
            if(!slot || slot->GetUObject()!=spawned || !slot->IsValid(false))throw std::runtime_error("Quest location object identity unavailable");
            if(Membership(subsystem,spawned)!=1)throw std::runtime_error("Native QuestLocation did not register exactly once; marker removed");
            const auto position=ActorHelper::GetActorLocation(spawned);
            if(std::abs(position.X()-location.Position[0])>1 || std::abs(position.Y()-location.Position[1])>1 || std::abs(position.Z()-location.Position[2])>1)
                throw std::runtime_error("Native quest location did not retain its coordinates; marker removed");
            actor=spawned;index=spawned->GetInternalIndex();serial=slot->GetSerialNumber();
        } catch(...) {ActorHelper::DestroyActor(spawned);throw;}
    }
    void Remove() {
        auto* value=Get();
        if(!value){actor=nullptr;index=-1;return;}
        auto* subsystem=Subsystem(value->GetWorld());
        ActorHelper::DestroyActor(value);
        actor=nullptr;index=-1;
        if(Membership(subsystem,value)!=0)throw std::runtime_error("Destroyed quest location remains in native registry");
    }
};
}
