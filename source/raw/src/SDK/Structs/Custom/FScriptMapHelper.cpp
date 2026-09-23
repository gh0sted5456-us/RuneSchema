#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <algorithm>
#include <cstdint>
#include <stdexcept>

using namespace RC;
using namespace RC::Unreal;

namespace UECustom {
    FScriptMapHelper::FScriptMapHelper(RC::Unreal::FMapProperty* InProperty, void* InMap)
    {
        KeyProperty = InProperty->GetKeyProp();
        ValueProperty = InProperty->GetValueProp();

        MapLayout = FScriptMap::GetScriptLayout(
            KeyProperty->GetSize(),
            KeyProperty->GetMinAlignment(),
            ValueProperty->GetSize(),
            ValueProperty->GetMinAlignment()
        );

        ScriptMap = static_cast<FScriptMap*>(InMap);
    }

    FScriptMapHelper::FScriptMapHelper(FScriptMap* InScriptMap, FScriptMapLayout InMapLayout, FProperty* InKeyProperty, FProperty* InValueProperty)
    {
        ScriptMap = InScriptMap;
        MapLayout = InMapLayout;
        KeyProperty = InKeyProperty;
        ValueProperty = InValueProperty;
    }

    void FScriptMapHelper::Add(void* PairPtrToAdd)
    {
        if (Update(PairPtrToAdd)) return;

        auto Index = ScriptMap->AddUninitialized(MapLayout);
        uint8* PairPtr = static_cast<uint8*>(ScriptMap->GetData(Index, MapLayout));

        void* KeyPtr = PairPtr;
        void* ValuePtr = PairPtr + MapLayout.ValueOffset;

        KeyProperty->InitializeValue(KeyPtr);
        ValueProperty->InitializeValue(ValuePtr);

        void* KeyPtrToAdd = PairPtrToAdd;
        void* ValuePtrToAdd = static_cast<uint8*>(PairPtrToAdd) + MapLayout.ValueOffset;

        FMemory::Memcpy(KeyPtr, KeyPtrToAdd, KeyProperty->GetElementSize());
        FMemory::Memcpy(ValuePtr, ValuePtrToAdd, ValueProperty->GetElementSize());
    }

    void FScriptMapHelper::Add(UECustom::FManagedValue& PairPtr)
    {
        Add(PairPtr.GetData());
    }

    bool FScriptMapHelper::Update(void* PairPtrToUpdate)
    {
        auto Num = ScriptMap->Num();

        if (Num < 0)
        {
            throw std::runtime_error("Failed to update TMap entry due to invalid ScriptMap.");
        }

        void* KeyPtrToUpdate = PairPtrToUpdate;
        void* ValuePtrToUpdate = static_cast<uint8*>(PairPtrToUpdate) + MapLayout.ValueOffset;

        for (auto Index = 0; Index < ScriptMap->GetMaxIndex(); ++Index)
        {
            if (!ScriptMap->IsValidIndex(Index)) {
                continue;
            }

            uint8* PairPtr = (uint8*)ScriptMap->GetData(Index, MapLayout);
            void* KeyPtr = PairPtr;
            void* ValuePtr = PairPtr + MapLayout.ValueOffset;

            if (KeyProperty->Identical(KeyPtr, KeyPtrToUpdate))
            {
                FMemory::Memcpy(KeyPtr, KeyPtrToUpdate, KeyProperty->GetElementSize());
                FMemory::Memcpy(ValuePtr, ValuePtrToUpdate, ValueProperty->GetElementSize());
                return true;
            }
        }

        return false;
    }

    bool FScriptMapHelper::Remove(void* KeyToRemove)
    {
        auto Num = ScriptMap->Num();

        if (Num < 0)
        {
            throw std::runtime_error("Failed to remove TMap entry due to invalid ScriptMap.");
        }

        for (auto Index = 0; Index < ScriptMap->GetMaxIndex(); ++Index)
        {
            if (!ScriptMap->IsValidIndex(Index)) {
                continue;
            }

            uint8* PairPtr = (uint8*)ScriptMap->GetData(Index, MapLayout);
            void* KeyPtr = PairPtr;

            if (KeyProperty->Identical(KeyPtr, KeyToRemove))
            {
                ScriptMap->RemoveAt(Index, MapLayout);
                return true;
            }
        }

        return false;
    }

    void FScriptMapHelper::InitializePair(UECustom::FManagedValue& PairPtr)
    {
        // Allocate the padded map layout, not key size + value size.
        const auto size = MapLayout.SetLayout.Size;
        const auto alignment = std::max(KeyProperty->GetMinAlignment(), ValueProperty->GetMinAlignment());
        if (KeyProperty->GetSize() <= 0 || ValueProperty->GetSize() <= 0
            || MapLayout.ValueOffset < KeyProperty->GetSize()
            || static_cast<int64_t>(MapLayout.ValueOffset) + ValueProperty->GetSize() > size
            || alignment <= 0 || (alignment & (alignment - 1)))
            throw std::runtime_error("Invalid reflected map pair layout");
        uint8* Pair = static_cast<uint8*>(FMemory::Malloc(size, alignment));
        if (!Pair) throw std::bad_alloc();
        PairPtr.Copy(Pair);

        void* KeyPtr = Pair;
        KeyProperty->InitializeValue(KeyPtr);

        void* ValuePtr = Pair + MapLayout.ValueOffset;
        ValueProperty->InitializeValue(ValuePtr);

    }

    void FScriptMapHelper::ForEachPair(const std::function<void(void*, void*)> Callback)
    {
        for (int32 Index = 0; Index < ScriptMap->GetMaxIndex(); ++Index)
        {
            if (!ScriptMap->IsValidIndex(Index)) continue;

            uint8* PairPtr = (uint8*)ScriptMap->GetData(Index, MapLayout);
            void* ValuePtr = PairPtr + MapLayout.ValueOffset;

            Callback(PairPtr, ValuePtr);
        }
    }

    void* FScriptMapHelper::GetKeyPtr(void* PairPtr) const
    {
        return PairPtr;
    }

    void* FScriptMapHelper::GetValuePtr(void* PairPtr) const
    {
        return static_cast<uint8*>(PairPtr) + MapLayout.ValueOffset;
    }

    void FScriptMapHelper::Rehash()
    {
        ScriptMap->Rehash(MapLayout, [this](const void* Src) {
            return KeyProperty->GetValueTypeHash(Src);
        });
    }
}
