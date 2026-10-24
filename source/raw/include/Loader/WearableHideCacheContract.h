#pragma once
#include <stdexcept>
#include "Unreal/UObject.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Helpers/Casting.hpp"

namespace DragonWilds::WearableHideCacheContract {
// The native builder inserts an EquipmentMeshDataToMaterialsToHide key only
// for a nonempty hide-section list. A mesh with no sections to hide does not
// need an opaque cache entry; item enumeration is validated separately.
inline bool RequiresEntry(RC::Unreal::UObject* mesh) {
    using namespace RC::Unreal;
    if (!mesh) throw std::runtime_error("wearable mesh descriptor unavailable");
    auto* property=CastField<FArrayProperty>(mesh->GetClassPrivate()->FindProperty(
        FName(L"MaterialSectionsToHide",FNAME_Find)));
    auto* inner=property?CastField<FStructProperty>(property->GetInner()):nullptr;
    if (!inner || !inner->GetStruct() || inner->GetStruct()->GetName()!=L"HideMaterialSectionData"
        || property->GetOffset_Internal()<0
        || property->GetOffset_Internal()+property->GetSize()>mesh->GetClassPrivate()->GetPropertiesSize())
        throw std::runtime_error("wearable hide-section descriptor layout changed");
    auto* array=property->ContainerPtrToValuePtr<FScriptArray>(mesh);
    if (array->Num()<0 || array->Num()>4096)
        throw std::runtime_error("wearable hide-section list exceeds diagnostic bounds");
    return array->Num()!=0;
}
}
