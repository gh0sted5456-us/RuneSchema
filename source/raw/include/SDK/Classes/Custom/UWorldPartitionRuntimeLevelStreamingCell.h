#pragma once

#include "Unreal/UObject.hpp"
#include "SDK/Structs/FBox.h"

namespace UECustom {
    class UWorldPartitionRuntimeCellData : public RC::Unreal::UObject {
    public:
        FBox* GetContentBounds();
    };

    class UWorldPartitionRuntimeLevelStreamingCell : public RC::Unreal::UObject {
    public:
        bool GetIsHLOD();
        bool IsShown();

        UWorldPartitionRuntimeCellData* GetRuntimeCellData();


        FBox* GetContentBounds();
    };
}
