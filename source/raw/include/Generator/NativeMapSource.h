#pragma once
#include "Core/MapProjection.h"
namespace RC::Unreal {class UWorld;class UObject;}
namespace PS::NativeMapSource {
struct Source {MapProjection::Projection Alignment;RC::Unreal::UObject* Texture=nullptr;};
Source Read(RC::Unreal::UWorld* world);
}
