#pragma once

#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include "SDK/Structs/Custom/FManagedValue.h"

namespace UECustom {
    struct FScriptArrayHelper {
    public:
        FScriptArrayHelper(void* InScriptArray, RC::Unreal::FArrayProperty* InArrayProperty);

        FScriptArrayHelper(RC::Unreal::FScriptArray* InScriptArray, RC::Unreal::FArrayProperty* InArrayProperty);

        void Add(void* Value);

        void Add(UECustom::FManagedValue& ValuePtr);

        bool RemoveAtIndex(RC::Unreal::int32 Index);

        void Empty();

        void ExpandForIndex(RC::Unreal::int32 Index);

        void InitializeValue(UECustom::FManagedValue& OutValuePtr);

        RC::Unreal::int32 GetElementSize();

        RC::Unreal::int32 GetMinAlignment();

        RC::Unreal::FProperty* GetInner();

        void ForEachElement(const std::function<void(void*)> Callback);
    private:
        RC::Unreal::FScriptArray* ScriptArray{};
        RC::Unreal::FArrayProperty* ArrayProperty{};
    };
}
