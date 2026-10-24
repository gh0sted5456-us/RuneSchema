#pragma once

#include "Unreal/UObject.hpp"
#include "SDK/Structs/FBox.h"
#include <functional>
#include <cstddef>
#include <optional>

namespace RC::Unreal {class UWorld;}

namespace UECustom {
    class UWorldPartitionRuntimeCellData : public RC::Unreal::UObject {
    public:
        std::optional<FBox> GetContentBounds();
    };

    class UWorldPartitionRuntimeLevelStreamingCell : public RC::Unreal::UObject {
    public:
        bool GetIsHLOD();
        bool IsShown();
        static void VisitShown(RC::Unreal::UWorld* world,size_t& cursor,double& delay,
            float deltaSeconds,const std::function<void(RC::Unreal::UObject*)>& visit);

        UWorldPartitionRuntimeCellData* GetRuntimeCellData();


        std::optional<FBox> GetContentBounds();
    };
}
