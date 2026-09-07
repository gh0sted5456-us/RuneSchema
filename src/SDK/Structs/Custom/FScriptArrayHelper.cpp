#include "SDK/Structs/Custom/FScriptArrayHelper.h"

using namespace RC;
using namespace RC::Unreal;

namespace UECustom {
    FScriptArrayHelper::FScriptArrayHelper(void* InScriptArray, RC::Unreal::FArrayProperty* InArrayProperty)
    {
        ScriptArray = static_cast<FScriptArray*>(InScriptArray);
        ArrayProperty = InArrayProperty;
    }

    FScriptArrayHelper::FScriptArrayHelper(FScriptArray* InScriptArray, FArrayProperty* InArrayProperty)
    {
        ScriptArray = InScriptArray;
        ArrayProperty = InArrayProperty;
    }

    void FScriptArrayHelper::Add(void* Value)
    {
        auto ElementSize = GetElementSize();
        auto ElementAlignment = GetMinAlignment();
        auto InnerProperty = GetInner();

        int32 FirstIndex = ScriptArray->Add(1, ElementSize, ElementAlignment);
        uint8* DataPtr = static_cast<uint8*>(ScriptArray->GetData());
        void* NewElementPtr = DataPtr + FirstIndex * ElementSize;
        InnerProperty->InitializeValue(NewElementPtr);
        FMemory::Memcpy(NewElementPtr, Value, ElementSize);
    }

    void FScriptArrayHelper::Add(UECustom::FManagedValue& ValuePtr)
    {
        Add(ValuePtr.GetData());
    }



    void FScriptArrayHelper::Empty()
    {
        auto ElementSize = GetElementSize();
        auto ElementAlignment = GetMinAlignment();
        auto InnerProperty = GetInner();

        for (int32 Index = 0; Index < ScriptArray->Num(); ++Index)
        {
            void* ElementPtr = static_cast<uint8*>(ScriptArray->GetData()) + Index * ElementSize;
            InnerProperty->DestroyValue(ElementPtr);
        }

        ScriptArray->Empty(0, ElementSize, ElementAlignment);
    }



    void FScriptArrayHelper::InitializeValue(UECustom::FManagedValue& OutValuePtr)
    {
        auto InnerProperty = GetInner();
        void* ValuePtr = FMemory::Malloc(InnerProperty->GetElementSize());
        InnerProperty->InitializeValue(ValuePtr);
        OutValuePtr.Copy(ValuePtr);
    }

    RC::Unreal::int32 FScriptArrayHelper::GetElementSize()
    {
        auto Inner = GetInner();
        return Inner->GetElementSize();
    }

    RC::Unreal::int32 FScriptArrayHelper::GetMinAlignment()
    {
        auto Inner = GetInner();
        return Inner->GetMinAlignment();
    }

    RC::Unreal::FProperty* FScriptArrayHelper::GetInner()
    {
        return ArrayProperty->GetInner();
    }

    void FScriptArrayHelper::ForEachElement(const std::function<void(void*)> Callback)
    {
        auto ElementSize = GetElementSize();
        for (int32 Index = 0; Index < ScriptArray->Num(); ++Index)
        {
            void* ElementPtr = static_cast<uint8*>(ScriptArray->GetData()) + Index * ElementSize;
            Callback(ElementPtr);
        }
    }
}
