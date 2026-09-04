#include "Unreal/Core/HAL/UnrealMemory.hpp"
#include "SDK/Structs/Custom/FManagedValue.h"

using namespace RC;
using namespace RC::Unreal;

namespace UECustom {
    FManagedValue::FManagedValue(void* InData) : Data(InData)
    {
    }

    FManagedValue::~FManagedValue()
    {
        FMemory::Free(Data);
    }

    void FManagedValue::Copy(void* InData)
    {
        Data = InData;
    }

    void* FManagedValue::GetData()
    {
        return Data;
    }
}
