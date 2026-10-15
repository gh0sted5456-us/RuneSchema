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

    FBox* UWorldPartitionRuntimeLevelStreamingCell::GetContentBounds()
    {
        auto* cellData = GetRuntimeCellData();
        return cellData ? cellData->GetContentBounds() : nullptr;
    }
}
