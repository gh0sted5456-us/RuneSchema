#pragma once

#include "Unreal/UObject.hpp"
#include "SDK/Structs/FBox.h"

namespace UECustom {
    class UWorldPartitionRuntimeCellData : public RC::Unreal::UObject {
    public:
        static constexpr int32_t ContentBoundsOffset = 0x60;

        FBox& GetContentBounds();
    };

    class UWorldPartitionRuntimeLevelStreamingCell : public RC::Unreal::UObject {
    public:
        static constexpr int32_t IsHLODOffset = 0x59;
        static constexpr int32_t RuntimeCellDataOffset = 0xA8;

        bool& GetIsHLOD();

        UWorldPartitionRuntimeCellData* GetRuntimeCellData();


        FBox* GetContentBounds();
    };
}
