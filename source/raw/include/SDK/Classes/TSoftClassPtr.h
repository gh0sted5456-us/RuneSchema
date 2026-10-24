#pragma once

#include "SDK/Classes/TPersistentObjectPtr.h"
#include "SDK/Structs/FSoftObjectPtr.h"

namespace UECustom {
    template<typename UEType>
    class TSoftClassPtr
    {
    public:
        explicit FORCEINLINE TSoftClassPtr(FSoftObjectPath ObjectPath) : SoftObjectPtr(ObjectPath)
        {
        }

        FORCEINLINE const FSoftObjectPath& ToSoftObjectPath() const
        {
            return SoftObjectPtr.ObjectID;
        }
    private:
        FSoftObjectPtr SoftObjectPtr;
    };
}
