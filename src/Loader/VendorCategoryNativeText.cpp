#include "Loader/VendorCategoryText.h"
#include "Loader/VendorCategoryLabel.h"

#include <cstddef>
#include <stdexcept>
#include <vector>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/FString.hpp"
#include "Unreal/NameTypes.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UObject.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds::VendorCategoryText {
namespace {
// Do not materialize a C++ FText or assume its size. The engine owns the text
// representation; reflected properties own parameter initialization/copy/cleanup.
class TextCall {
public:
    explicit TextCall(bool toText)
    {
        self = UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr,
            TEXT("/Script/Engine.Default__KismetTextLibrary"));
        function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr,
            toText ? TEXT("/Script/Engine.KismetTextLibrary:Conv_StringToText")
                   : TEXT("/Script/Engine.KismetTextLibrary:Conv_TextToString"));
        if (!self || !function)
            throw std::runtime_error("Vendor category: native text conversion is unavailable");

        input = function->FindProperty(FName(toText ? TEXT("InString") : TEXT("InText"), FNAME_Find));
        output = function->GetReturnProperty();
        const size_t size = function->GetParmsSize();
        const auto valid = [size](FProperty* field) {
            return field && field->GetArrayDim() == 1
                && field->GetOffset_Internal() >= 0 && field->GetElementSize() > 0
                && static_cast<size_t>(field->GetOffset_Internal()) <= size
                && static_cast<size_t>(field->GetElementSize())
                    <= size - static_cast<size_t>(field->GetOffset_Internal());
        };
        if (!size || size > 4096 || !valid(input) || !valid(output) || input == output)
            throw std::runtime_error("Vendor category: invalid native text parameter layout");
        const auto inputEnd = input->GetOffset_Internal() + input->GetElementSize();
        const auto outputEnd = output->GetOffset_Internal() + output->GetElementSize();
        if (input->GetOffset_Internal() < outputEnd && output->GetOffset_Internal() < inputEnd)
            throw std::runtime_error("Vendor category: overlapping native text parameters");
        for (auto* field : TFieldRange<FProperty>(function, EFieldIterationFlags::Default))
            if (field != input && field != output)
                throw std::runtime_error("Vendor category: unexpected native text parameter");

        auto* stringField = CastField<FStrProperty>(toText ? input : output);
        auto* textField = CastField<FTextProperty>(toText ? output : input);
        if (!stringField || !textField || stringField->GetElementSize() != static_cast<int>(sizeof(FString)))
            throw std::runtime_error("Vendor category: native string/text type mismatch");

        // max_align_t also supplies alignment for reflected parameter storage.
        storage.resize((size + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
        initialized.reserve(2);
        try {
            input->InitializeValue_InContainer(storage.data());
            initialized.push_back(input);
            output->InitializeValue_InContainer(storage.data());
            initialized.push_back(output);
        } catch (...) {
            Cleanup();
            throw;
        }
    }
    ~TextCall() { Cleanup(); }
    TextCall(const TextCall&) = delete;
    TextCall& operator=(const TextCall&) = delete;

    void* Input() { return input->ContainerPtrToValuePtr<void>(storage.data()); }
    void* Output() { return output->ContainerPtrToValuePtr<void>(storage.data()); }
    FProperty* InputProperty() { return input; }
    FProperty* OutputProperty() { return output; }
    void Invoke() { self->ProcessEvent(function, storage.data()); }

private:
    void Cleanup() noexcept
    {
        for (auto it = initialized.rbegin(); it != initialized.rend(); ++it) {
            try { (*it)->DestroyValue_InContainer(storage.data()); }
            catch (...) {} // Destructors must not throw during error unwinding.
        }
        initialized.clear();
    }
    UObject* self = nullptr;
    UFunction* function = nullptr;
    FProperty* input = nullptr;
    FProperty* output = nullptr;
    std::vector<std::max_align_t> storage;
    std::vector<FProperty*> initialized;
};

void RequireCompatible(FTextProperty* destination, FProperty* source)
{
    if (!destination || destination->GetArrayDim() != 1
        || destination->GetOffset_Internal() < 0 || destination->GetElementSize() <= 0
        || !CastField<FTextProperty>(source)
        || destination->GetElementSize() != source->GetElementSize())
        throw std::runtime_error("Vendor category: incompatible reflected FText storage");
}

}

std::string Read(const void* container, FTextProperty* property)
{
    if (!container) throw std::runtime_error("Vendor category: null text container");
    TextCall call(false);
    RequireCompatible(property, call.InputProperty());
    // Deep copy, so ProcessEvent and parameter cleanup never borrow a live row's
    // text allocation. No word splitting or token interpretation takes place.
    call.InputProperty()->CopyCompleteValue(call.Input(),
        property->ContainerPtrToValuePtr<void>(const_cast<void*>(container)));
    call.Invoke();
    const auto& value = *static_cast<const FString*>(call.Output());
    return RC::to_string(*value);
}

void Write(void* container, FTextProperty* property, const std::string& label)
{
    if (!container) throw std::runtime_error("Vendor category: null text container");
    TextCall call(true);
    RequireCompatible(property, call.OutputProperty());
    const auto wide = RC::to_generic_string(label);
    auto string = FString(wide.c_str());
    static_cast<FStrProperty*>(call.InputProperty())->SetPropertyValue(call.Input(), string);
    call.Invoke();
    property->CopyCompleteValue(property->ContainerPtrToValuePtr<void>(container), call.Output());
    // Validate the destination, not just the temporary native conversion result.
    VendorCategoryLabel::RequireExact(label, Read(container, property));
}

}
