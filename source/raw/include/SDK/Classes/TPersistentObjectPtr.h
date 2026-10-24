#pragma once

#include "Unreal/FWeakObjectPtr.hpp"

namespace UECustom {
    template<typename TObjectID>
    class TPersistentObjectPtr
    {
    public:
        explicit TPersistentObjectPtr()
            : WeakPtr()
            , ObjectID()
        {
        }

        explicit TPersistentObjectPtr(const TObjectID& InObjectID)
            : WeakPtr()
            , ObjectID(InObjectID)
        {
        }
    public:
        RC::Unreal::FWeakObjectPtr WeakPtr;
        TObjectID ObjectID;
    public:
        class RC::Unreal::UObject* Get() const
        {
            return WeakPtr.Get();
        }
        class RC::Unreal::UObject* operator->() const
        {
            return WeakPtr.Get();
        }
    };
}
