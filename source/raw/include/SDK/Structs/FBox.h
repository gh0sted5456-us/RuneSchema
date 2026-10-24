#pragma once

#include "Unreal/UnrealCoreStructs.hpp"

namespace UECustom {
    struct FBox {
        RC::Unreal::FVector Min;
        RC::Unreal::FVector Max;
        bool bIsValid;

        bool IsInside(const RC::Unreal::FVector& point) const
        {
            return point.X() > Min.X() && point.X() < Max.X()
                && point.Y() > Min.Y() && point.Y() < Max.Y()
                && point.Z() > Min.Z() && point.Z() < Max.Z();
        }

        double LargestAxis() const
        {
            auto x = Max.X() - Min.X();
            auto y = Max.Y() - Min.Y();
            auto z = Max.Z() - Min.Z();
            return x > y ? (x > z ? x : z) : (y > z ? y : z);
        }
    };
}
