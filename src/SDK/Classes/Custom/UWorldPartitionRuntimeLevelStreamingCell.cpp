#include "SDK/Classes/Custom/UWorldPartitionRuntimeLevelStreamingCell.h"
#include "Helpers/Casting.hpp"

using namespace RC;
using namespace RC::Unreal;

namespace UECustom {
    FBox& UWorldPartitionRuntimeCellData::GetContentBounds()
    {
        return *Helper::Casting::ptr_cast<FBox*>(this, ContentBoundsOffset);
    }

    bool& UWorldPartitionRuntimeLevelStreamingCell::GetIsHLOD()
    {
        return *Helper::Casting::ptr_cast<bool*>(this, IsHLODOffset);
    }

    UWorldPartitionRuntimeCellData* UWorldPartitionRuntimeLevelStreamingCell::GetRuntimeCellData()
    {
        return *Helper::Casting::ptr_cast<UWorldPartitionRuntimeCellData**>(this, RuntimeCellDataOffset);
    }

    FBox* UWorldPartitionRuntimeLevelStreamingCell::GetContentBounds()
    {
        auto* cellData = GetRuntimeCellData();
        return cellData ? &cellData->GetContentBounds() : nullptr;
    }
}
