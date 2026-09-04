#pragma once

#include "SDK/Classes/TPersistentObjectPtr.h"
#include "SDK/Structs/FSoftObjectPtr.h"

namespace UECustom {
    template<typename UEType>
    class TSoftObjectPtr
    {
    public:
        FORCEINLINE TSoftObjectPtr() {};

        explicit FORCEINLINE TSoftObjectPtr(FSoftObjectPath ObjectPath) : SoftObjectPtr(ObjectPath)
        {
        }

        template <
            class U
            UE_REQUIRES(std::is_convertible_v<U*, UEType*>)
        >
        FORCEINLINE TSoftObjectPtr(const TSoftObjectPtr<U>& Other)
            : SoftObjectPtr(Other.SoftObjectPtr)
        {
        }

        const FSoftObjectPtr Get() const
        {
            return SoftObjectPtr;
        }

        void SetResolvedObject(RC::Unreal::UObject* Object)
        {
            SoftObjectPtr.WeakPtr = RC::Unreal::FWeakObjectPtr(Object);
        }
    private:
        FSoftObjectPtr SoftObjectPtr;
    };
}
