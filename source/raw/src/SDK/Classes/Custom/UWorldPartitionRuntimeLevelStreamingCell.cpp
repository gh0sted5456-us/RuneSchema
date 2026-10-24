#include "SDK/Classes/Custom/UWorldPartitionRuntimeLevelStreamingCell.h"
#include "Helpers/Casting.hpp"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/ReflectedContract.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/UObjectArray.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

using namespace RC;
using namespace RC::Unreal;

namespace UECustom {
    std::optional<FBox> UWorldPartitionRuntimeCellData::GetContentBounds()
    {
        auto* property=CastField<FStructProperty>(DragonWilds::PropertyHelper::GetPropertyByName(GetClassPrivate(),TEXT("ContentBounds")));
        if(!DragonWilds::ReflectedContract::Fits(property,GetClassPrivate()->GetPropertiesSize())
            ||!property->GetStruct()||property->GetStruct()->GetPathName()!=TEXT("/Script/CoreUObject.Box"))return {};
        // FVector's SDK wrapper has byte alignment. A C++ FBox therefore need
        // not have the engine's padding/stride; never alias a native Box with it.
        auto* structure=property->GetStruct().Get();
        const auto extent=structure->GetPropertiesSize();
        if(extent<=0||extent>property->GetElementSize())return {};
        auto* minimum=CastField<FStructProperty>(DragonWilds::PropertyHelper::GetPropertyByName(structure,TEXT("Min")));
        auto* maximum=CastField<FStructProperty>(DragonWilds::PropertyHelper::GetPropertyByName(structure,TEXT("Max")));
        auto* valid=CastField<FBoolProperty>(DragonWilds::PropertyHelper::GetPropertyByName(structure,TEXT("IsValid")));
        for(auto* vector:{minimum,maximum})
            if(!DragonWilds::ReflectedContract::Fits(vector,extent)||!vector->GetStruct()
                ||vector->GetStruct()->GetPathName()!=TEXT("/Script/CoreUObject.Vector")
                ||vector->GetElementSize()!=sizeof(FVector))return {};
        if(!DragonWilds::ReflectedContract::Fits(valid,extent))return {};
        auto* native=property->ContainerPtrToValuePtr<void>(this);
        if(!valid->GetPropertyValue(valid->ContainerPtrToValuePtr<void>(native)))return {};
        FBox bounds{};
        std::memcpy(&bounds.Min,minimum->ContainerPtrToValuePtr<void>(native),sizeof(FVector));
        std::memcpy(&bounds.Max,maximum->ContainerPtrToValuePtr<void>(native),sizeof(FVector));
        bounds.bIsValid=true;
        for(const auto value:{bounds.Min.X(),bounds.Min.Y(),bounds.Min.Z(),bounds.Max.X(),bounds.Max.Y(),bounds.Max.Z()})
            if(!std::isfinite(value))return {};
        if(bounds.Min.X()>bounds.Max.X()||bounds.Min.Y()>bounds.Max.Y()||bounds.Min.Z()>bounds.Max.Z())return {};
        return bounds;
    }

    bool UWorldPartitionRuntimeLevelStreamingCell::GetIsHLOD()
    {
        auto* property=CastField<FBoolProperty>(DragonWilds::PropertyHelper::GetPropertyByName(GetClassPrivate(),TEXT("bIsHLOD")));
        // An unrecognised cell is not a placement-ready gameplay cell.
        if(!DragonWilds::ReflectedContract::Fits(property,GetClassPrivate()->GetPropertiesSize()))return true;
        return property->GetPropertyValue(property->ContainerPtrToValuePtr<void>(this));
    }

    UWorldPartitionRuntimeCellData* UWorldPartitionRuntimeLevelStreamingCell::GetRuntimeCellData()
    {
        auto* property=CastField<FObjectPropertyBase>(DragonWilds::PropertyHelper::GetPropertyByName(GetClassPrivate(),TEXT("RuntimeCellData")));
        if(!DragonWilds::ReflectedContract::Fits(property,GetClassPrivate()->GetPropertiesSize())
            ||!property->GetPropertyClass())return nullptr;
        auto* data=property->GetObjectPropertyValue(property->ContainerPtrToValuePtr<void>(this));
        if(!data||!data->IsA(property->GetPropertyClass().Get())
            ||data->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))return nullptr;
        return static_cast<UWorldPartitionRuntimeCellData*>(data);
    }

    bool UWorldPartitionRuntimeLevelStreamingCell::IsShown()
    {
        // Read current visibility, not the requested bShouldBeVisible state.
        // This catches cells shown before a loader finished initializing without
        // activating unloaded cells or depending on executable-specific offsets.
        const auto objectField=[](UObject* owner,const TCHAR* name)->UObject* {
            if(!owner)return nullptr;
            auto* property=CastField<FObjectPropertyBase>(DragonWilds::PropertyHelper::GetPropertyByName(owner->GetClassPrivate(),name));
            if(!DragonWilds::ReflectedContract::Fits(property,owner->GetClassPrivate()->GetPropertiesSize())||!property->GetPropertyClass())return nullptr;
            auto* value=property->GetObjectPropertyValue(property->ContainerPtrToValuePtr<void>(owner));
            if(!value||!value->IsA(property->GetPropertyClass().Get())
                ||value->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))return nullptr;
            return value;
        };
        auto* streaming=objectField(this,TEXT("LevelStreaming"));
        auto* level=objectField(streaming,TEXT("LoadedLevel"));
        if(!level||level->GetWorld()!=GetWorld())return false;
        auto* visible=CastField<FBoolProperty>(DragonWilds::PropertyHelper::GetPropertyByName(level->GetClassPrivate(),TEXT("bIsVisible")));
        return DragonWilds::ReflectedContract::Fits(visible,level->GetClassPrivate()->GetPropertiesSize())
            &&visible->GetPropertyValue(visible->ContainerPtrToValuePtr<void>(level));
    }

    std::optional<FBox> UWorldPartitionRuntimeLevelStreamingCell::GetContentBounds()
    {
        auto* cellData = GetRuntimeCellData();
        return cellData ? cellData->GetContentBounds() : std::nullopt;
    }

    void UWorldPartitionRuntimeLevelStreamingCell::VisitShown(UWorld* world,size_t& cursor,double& delay,
        float deltaSeconds,const std::function<void(UObject*)>& visit)
    {
        if(!world||!visit||!std::isfinite(deltaSeconds)||deltaSeconds<=0)return;
        delay=std::max(0.0,delay-std::min(1.0,static_cast<double>(deltaSeconds)));
        if(delay>0)return;
        auto* type=UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.WorldPartitionRuntimeLevelStreamingCell"));
        if(!type)return;
        const auto count=FUObjectArray::GetNumElements();if(count<0||count>4'000'000)return;
        const auto begin=std::chrono::steady_clock::now();size_t scanned=0;
        while(cursor<static_cast<size_t>(count)&&scanned++<2048
            &&std::chrono::steady_clock::now()-begin<std::chrono::microseconds(500)) {
            auto* slot=FUObjectArray::IndexToObject(static_cast<int32>(cursor++));auto* object=slot?slot->GetUObject():nullptr;
            if(!object||!object->IsA(type)||object->GetWorld()!=world
                ||object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))continue;
            auto* cell=static_cast<UWorldPartitionRuntimeLevelStreamingCell*>(object);
            if(!cell->GetIsHLOD()&&cell->IsShown())visit(cell);
        }
        if(cursor>=static_cast<size_t>(count)){cursor=0;delay=2;}
    }
}
