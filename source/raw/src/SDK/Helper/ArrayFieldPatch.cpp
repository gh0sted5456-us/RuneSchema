#include "SDK/Helper/ArrayFieldPatch.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Structs/Custom/FManagedStruct.h"
#include "Core/JsonArrayPatch.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Core/HAL/UnrealMemory.hpp"
#include <stdexcept>

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds::PropertyHelper {
    namespace {
        struct ArrayCopy {
            FArrayProperty* Property;
            void* Data;
            ArrayCopy(FArrayProperty* property, void* source) : Property(property),
                Data(FMemory::Malloc(property->GetElementSize())) {
                Property->InitializeValue(Data);
                try { Property->CopySingleValue(Data, source); }
                catch (...) { Property->DestroyValue(Data); FMemory::Free(Data); throw; }
            }
            ~ArrayCopy() { Property->DestroyValue(Data); FMemory::Free(Data); }
            ArrayCopy(const ArrayCopy&) = delete;
            ArrayCopy& operator=(const ArrayCopy&) = delete;
        };

        FProperty* RequireField(UScriptStruct* type, const std::string& key) {
            if (key.starts_with("$")) throw std::runtime_error("Directive is not a reflected array-entry field: " + key);
            auto* field = GetPropertyByName(type, to_generic_string(key));
            if (!field || field->GetArrayDim() != 1)
                throw std::runtime_error("Unknown or unsupported array-entry field: " + key);
            return field;
        }

        void CheckFields(UScriptStruct* type, const nlohmann::json& fields, bool matching) {
            if (!fields.is_object() || fields.empty()) throw std::runtime_error("Entry fields must be a non-empty object");
            for (const auto& [key, value] : fields.items()) {
                auto* field = RequireField(type, key);
                if (auto* nested = CastField<FStructProperty>(field); nested && value.is_object()) {
                    CheckFields(nested->GetStruct().Get(), value, matching);
                } else if (auto* array = CastField<FArrayProperty>(field)) {
                    if (matching) throw std::runtime_error("$Match does not support array fields");
                    auto* element = CastField<FStructProperty>(array->GetInner());
                    if (!element) throw std::runtime_error("Nested array patch requires struct elements");
                    for (const auto& edit : JsonArrayPatch::Parse(value)) {
                        CheckFields(element->GetStruct().Get(), edit.Target, false);
                        if (!edit.Index) CheckFields(element->GetStruct().Get(), edit.Match, true);
                    }
                } else {
                    // Do not traverse shared UObjects while staging a struct.
                    if ((CastField<FObjectProperty>(field) || CastField<FSoftObjectProperty>(field))
                        && !value.is_string() && !value.is_null())
                        throw std::runtime_error("Array reference fields require an object-path string");
                    ValidateJsonValueType(field, value);
                }
            }
        }

        bool Matches(void* data, UScriptStruct* type, const nlohmann::json& fields) {
            FManagedStruct expected(type);
            for (const auto& [key, value] : fields.items()) {
                auto* field = RequireField(type, key);
                auto* actual = field->ContainerPtrToValuePtr<void>(data);
                if (auto* nested = CastField<FStructProperty>(field); nested && value.is_object()) {
                    if (!Matches(actual, nested->GetStruct().Get(), value)) return false;
                } else {
                    CopyJsonValueToContainer(expected.GetData(), field, value);
                    if (!field->Identical(actual, field->ContainerPtrToValuePtr<void>(expected.GetData()))) return false;
                }
            }
            return true;
        }
    }

    void PatchArrayFields(void* data, FArrayProperty* property, const nlohmann::json& value) {
        const auto edits = JsonArrayPatch::Parse(value);
        auto* inner = CastField<FStructProperty>(property->GetInner());
        if (!inner || property->GetArrayDim() != 1)
            throw std::runtime_error("Array $Patch supports arrays of structs only");
        auto* type = inner->GetStruct().Get();
        for (const auto& edit : edits) {
            CheckFields(type, edit.Target, false);
            if (!edit.Index) CheckFields(type, edit.Match, true);
        }
        ArrayCopy copy(property, data);
        auto* array = static_cast<FScriptArray*>(copy.Data);
        for (const auto& edit : edits) {
            const auto selected = JsonArrayPatch::Select(edit, array->Num(), [&](int index) {
                    auto* entry = static_cast<uint8*>(array->GetData()) + index * inner->GetElementSize();
                    return Matches(entry, type, edit.Match);
            });
            auto* entry = static_cast<uint8*>(array->GetData()) + selected * inner->GetElementSize();
            for (const auto& [key, target] : edit.Target.items())
                CopyJsonValueToContainer(entry, RequireField(type, key), target);
        }
        property->CopySingleValue(data, copy.Data);
    }
}
