#pragma once

#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include "SDK/Structs/Custom/FManagedValue.h"

namespace UECustom {
    struct FScriptMapHelper {
    public:
        FScriptMapHelper(RC::Unreal::FMapProperty* InProperty, void* InMap);

        FScriptMapHelper(RC::Unreal::FScriptMap* InScriptMap, RC::Unreal::FScriptMapLayout InMapLayout, RC::Unreal::FProperty* InKeyProperty, RC::Unreal::FProperty* InValueProperty);

        void Add(void* PairPtrToAdd);

        void Add(UECustom::FManagedValue& PairPtr);

        bool Update(void* PairPtrToUpdate);

        bool Remove(void* KeyToRemove);

        void InitializePair(UECustom::FManagedValue& PairPtr);

        void ForEachPair(const std::function<void(void*, void*)> Callback);

        void* GetKeyPtr(void* PairPtr) const;
        void* GetValuePtr(void* PairPtr) const;

        void Rehash();
    private:
        RC::Unreal::FScriptMap* ScriptMap{};
        RC::Unreal::FScriptMapLayout MapLayout{};
        RC::Unreal::FProperty* KeyProperty{};
        RC::Unreal::FProperty* ValueProperty{};
    };
}
