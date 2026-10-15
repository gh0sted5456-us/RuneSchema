#include "SDK/Classes/Custom/UWorldPartitionRuntimeLevelStreamingCell.h"
#include "Helpers/Casting.hpp"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/ReflectedContract.h"
#include <cmath>

using namespace RC;
using namespace RC::Unreal;

namespace UECustom {
    FBox* UWorldPartitionRuntimeCellData::GetContentBounds()
    {
        auto* property=CastField<FStructProperty>(DragonWilds::PropertyHelper::GetPropertyByName(GetClassPrivate(),TEXT("ContentBounds")));
        if(!DragonWilds::ReflectedContract::Fits(property,GetClassPrivate()->GetPropertiesSize())
            ||!property->GetStruct()||property->GetStruct()->GetPathName()!=TEXT("/Script/CoreUObject.Box")
            ||property->GetElementSize()!=sizeof(FBox))return nullptr;
        auto* bounds=property->ContainerPtrToValuePtr<FBox>(this);
        if(!bounds->bIsValid)return nullptr;
        for(const auto value:{bounds->Min.X(),bounds->Min.Y(),bounds->Min.Z(),bounds->Max.X(),bounds->Max.Y(),bounds->Max.Z()})
            if(!std::isfinite(value))return nullptr;
        if(bounds->Min.X()>bounds->Max.X()||bounds->Min.Y()>bounds->Max.Y()||bounds->Min.Z()>bounds->Max.Z())return nullptr;
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

    FBox* UWorldPartitionRuntimeLevelStreamingCell::GetContentBounds()
    {
        auto* cellData = GetRuntimeCellData();
        return cellData ? cellData->GetContentBounds() : nullptr;
    }
}
