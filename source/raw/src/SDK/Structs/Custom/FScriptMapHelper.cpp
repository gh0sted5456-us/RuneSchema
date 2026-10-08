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
        void* KeyPtrToAdd = PairPtrToAdd;
        void* ValuePtrToAdd = static_cast<uint8*>(PairPtrToAdd) + MapLayout.ValueOffset;
        const auto construct = [](FProperty* property, const void* source,
                                   void* destination) {
            property->InitializeValue(destination);
            property->CopySingleValue(destination, source);
        };
        const auto destruct = [](FProperty* property, void* value) {
            property->DestroyValue(value);
        };
        ScriptMap->Add(KeyPtrToAdd, ValuePtrToAdd, MapLayout,
            [this](const void* source) {
                return KeyProperty->GetValueTypeHash(source);
            },
            [this](const void* left, const void* right) {
                return KeyProperty->Identical(left, right);
            },
            [this, KeyPtrToAdd, construct](void* destination) {
                construct(KeyProperty, KeyPtrToAdd, destination);
            },
            [this, ValuePtrToAdd, construct](void* destination) {
                construct(ValueProperty, ValuePtrToAdd, destination);
            },
            [this, ValuePtrToAdd](void* destination) {
                ValueProperty->CopySingleValue(destination, ValuePtrToAdd);
            },
            [this, destruct](void* value) { destruct(KeyProperty, value); },
            [this, destruct](void* value) { destruct(ValueProperty, value); });
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

        void* value = FindValue(PairPtrToUpdate);
        if (!value) return false;
        ValueProperty->CopySingleValue(value,
            static_cast<uint8*>(PairPtrToUpdate) + MapLayout.ValueOffset);
        return true;
    }

    void* FScriptMapHelper::FindValue(const void* KeyToFind) const
    {
        return ScriptMap->FindValue(KeyToFind, MapLayout,
            [this](const void* source) {
                return KeyProperty->GetValueTypeHash(source);
            },
            [this](const void* left, const void* right) {
                return KeyProperty->Identical(left, right);
            });
    }

    bool FScriptMapHelper::Remove(void* KeyToRemove)
    {
        auto Num = ScriptMap->Num();

        if (Num < 0)
        {
            throw std::runtime_error("Failed to remove TMap entry due to invalid ScriptMap.");
        }

        const auto index = ScriptMap->FindPairIndex(KeyToRemove, MapLayout,
            [this](const void* source) {
                return KeyProperty->GetValueTypeHash(source);
            },
            [this](const void* left, const void* right) {
                return KeyProperty->Identical(left, right);
            });
        if (index == INDEX_NONE) return false;
        auto* pair = static_cast<uint8*>(ScriptMap->GetData(index, MapLayout));
        ValueProperty->DestroyValue(pair + MapLayout.ValueOffset);
        KeyProperty->DestroyValue(pair);
        ScriptMap->RemoveAt(index, MapLayout);
        return true;
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
