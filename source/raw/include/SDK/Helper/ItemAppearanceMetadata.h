#pragma once
// Read-only compatibility metadata; does not mutate ItemData or a shared asset.
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "SDK/Helper/PropertyHelper.h"
#include "Helpers/String.hpp"
#include "Helpers/Casting.hpp"
#include <string>

namespace PS::ItemAppearanceMetadata {
inline std::string Group(RC::Unreal::UObject* item) {
    using namespace RC::Unreal;
    if(!item||!item->GetClassPrivate())return {};
    auto* type=item->GetClassPrivate();
    if(!DragonWilds::PropertyHelper::GetPropertyByName(type,TEXT("HeldEquipmentActorClass")))return {};
    auto* category=CastField<FStructProperty>(DragonWilds::PropertyHelper::GetPropertyByName(type,TEXT("Category")));
    if(!category||category->GetArrayDim()!=1||!category->GetStruct())return "held:unresolved";
    const auto offset=category->GetOffset_Internal(),size=category->GetElementSize();
    if(offset<0||size<=0||offset>type->GetPropertiesSize()||size>type->GetPropertiesSize()-offset)return "held:unresolved";
    auto* tag=CastField<FNameProperty>(DragonWilds::PropertyHelper::GetPropertyByName(category->GetStruct().Get(),TEXT("TagName")));
    if(!tag||tag->GetArrayDim()!=1||tag->GetElementSize()!=sizeof(FName)||tag->GetOffset_Internal()<0
        ||tag->GetOffset_Internal()>size||tag->GetElementSize()>size-tag->GetOffset_Internal())return "held:unresolved";
    auto* data=category->ContainerPtrToValuePtr<void>(item);
    const auto name=RC::to_string(tag->ContainerPtrToValuePtr<FName>(data)->ToString());
    return name.empty()||name=="None"?"held:unresolved":"held:"+name;
}
} // namespace PS::ItemAppearanceMetadata
