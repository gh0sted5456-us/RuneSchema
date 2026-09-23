#include "Loader/VendorCategoryText.h"
#include "Loader/VendorCategoryGroups.h"
#include "Loader/VendorCategoryLabel.h"

#include <cstdint>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Structs/Custom/FManagedValue.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds::VendorCategoryText {
namespace {
struct GroupFields {
    FStructProperty* element;
    FTextProperty* label;
    FArrayProperty* collection;
};
GroupFields RequireGroups(FArrayProperty* groups)
{
    auto* element = groups ? CastField<FStructProperty>(groups->GetInner()) : nullptr;
    auto* type = element ? element->GetStruct().Get() : nullptr;
    auto* label = type ? CastField<FTextProperty>(
        PropertyHelper::GetPropertyByName(type, TEXT("Label"))) : nullptr;
    auto* collection = type ? CastField<FArrayProperty>(
        PropertyHelper::GetPropertyByName(type, TEXT("Collection"))) : nullptr;
    if (!groups || groups->GetArrayDim() != 1 || !element || element->GetArrayDim() != 1
        || !label || label->GetArrayDim() != 1 || !collection
        || collection->GetArrayDim() != 1 || !CastField<FSoftObjectProperty>(collection->GetInner()))
        throw std::runtime_error("Vendor category: unsupported LabeledRecipes layout");
    const auto size = element->GetElementSize();
    for (auto* field : {static_cast<FProperty*>(label), static_cast<FProperty*>(collection)})
        if (size <= 0 || field->GetOffset_Internal() < 0 || field->GetElementSize() <= 0
            || field->GetOffset_Internal() > size
            || field->GetElementSize() > size - field->GetOffset_Internal())
            throw std::runtime_error("Vendor category: group field is outside its reflected struct");
    return {element, label, collection};
}

void RequireExpected(const nlohmann::json& expected)
{
    if (!expected.is_array() || expected.size() > 128)
        throw std::runtime_error("Vendor category: expected at most 128 groups");
    for (const auto& group : expected) {
        if (!group.is_object() || !group.contains("Label") || !group.at("Label").is_string()
            || !group.contains("Collection") || !group.at("Collection").is_array())
            throw std::runtime_error("Vendor category: expected Label and Collection");
        (void)VendorCategoryLabel::Validate(group.at("Label").get<std::string>());
    }
}
}

void WriteGroups(void* row, FArrayProperty* groups, const nlohmann::json& expected)
{
    if (!row) throw std::runtime_error("Vendor category: null merchant row");
    RequireExpected(expected);
    const auto fields = RequireGroups(groups);
    auto* array = groups->ContainerPtrToValuePtr<FScriptArray>(row);
    UECustom::FScriptArrayHelper helper(array, groups);
    helper.Empty();
    for (const auto& group : expected) {
        UECustom::FManagedValue value;
        helper.InitializeValue(value);
        PropertyHelper::CopyJsonValueToContainer(value.GetData(), fields.collection, group.at("Collection"));
        Write(value.GetData(), fields.label, group.at("Label").get<std::string>());
        helper.Add(value);
    }
    VerifyGroups(row, groups, expected);
}

void VerifyGroups(const void* row, FArrayProperty* groups, const nlohmann::json& expected)
{
    if (!row) throw std::runtime_error("Vendor category: merchant row was not created");
    RequireExpected(expected);
    const auto fields = RequireGroups(groups);
    auto* array = groups->ContainerPtrToValuePtr<FScriptArray>(const_cast<void*>(row));
    if (array->Num() != static_cast<int>(expected.size()) || (array->Num() && !array->GetData()))
        throw std::runtime_error("Vendor category: merchant group count changed during copying");
    for (int i = 0; i < array->Num(); ++i) {
        const auto& wanted = expected.at(static_cast<size_t>(i));
        auto* entry = static_cast<uint8_t*>(array->GetData()) + i * fields.element->GetElementSize();
        const auto actual = Read(entry, fields.label);
        VendorCategoryLabel::RequireExact(wanted.at("Label").get<std::string>(), actual);
        auto* collection = fields.collection->ContainerPtrToValuePtr<FScriptArray>(entry);
        if (collection->Num() != static_cast<int>(wanted.at("Collection").size())
            || (collection->Num() && !collection->GetData()))
            throw std::runtime_error("Vendor category: recipe count changed in '" + actual + "'");
    }
}
}
