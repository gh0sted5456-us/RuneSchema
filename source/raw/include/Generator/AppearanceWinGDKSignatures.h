#pragma once
#include "Generator/AppearanceSignatures.h"
namespace PS::AppearanceWinGDKSignatures {
inline constexpr AppearanceSignatures::Target WearableTargets[]{{10,4,true},{34,4,true},{57,4,true}};
inline constexpr AppearanceSignatures::Signature WearableMeshRoutineReturn{
    "WearableMeshRoutineReturnWinGDK",61,"488d542448410fb6cce83b9f18008b4508c1e81ef6d0a8017423488b8dd8140000e8637bffff4885c07412488d97c00000004c8bc3488bc8e81c9e18004883c468415e415d415c5f5d5bc3","ffffffffffffffffffff00000000ffffffffffffffffffffffffffffffffffffffff00000000ffffffffffffffffffffffffffffffffffffff00000000ffffffffffffffffffffffffffff",WearableTargets,std::size(WearableTargets)};
}
