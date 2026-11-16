#include <Unreal/Core/HAL/UnrealMemory.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <new>
#include <stdexcept>
#include <utility>
#include "SDK/Structs/Custom/FManagedValue.h"

using namespace RC;
using namespace RC::Unreal;

namespace UECustom {
    void FManagedValue::Cleanup::operator()(void* data) const
    {
        if (ValueProperty)
            ValueProperty->DestroyValue(static_cast<unsigned char*>(data) + ValueOffset);
        if (Property)
            Property->DestroyValue(data);
        FMemory::Free(data);
    }

    void FManagedValue::Initialize(int size, int alignment, FProperty& property,
                                   FProperty* valueProperty, int valueOffset)
    {
        if (Data) throw std::logic_error("Reflected temporary already initialized");
        if (size <= 0 || alignment <= 0 || (alignment & (alignment - 1)))
            throw std::runtime_error("Invalid reflected temporary size or alignment");

        decltype(Data) prepared{FMemory::Malloc(size, alignment), Cleanup{}};
        if (!prepared) throw std::bad_alloc();

        property.InitializeValue(prepared.get());
        prepared.get_deleter().Property = &property;
        if (valueProperty)
        {
            valueProperty->InitializeValue(static_cast<unsigned char*>(prepared.get()) + valueOffset);
            prepared.get_deleter().ValueProperty = valueProperty;
            prepared.get_deleter().ValueOffset = valueOffset;
        }
        Data = std::move(prepared);
    }

    void* FManagedValue::GetData() const
    {
        return Data.get();
    }
}
