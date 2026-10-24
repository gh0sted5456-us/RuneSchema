#pragma once

#include "Unreal/UObject.hpp"

namespace UECustom {
    class UObjectWrapper : public RC::Unreal::UObject {
    public:
        void* GetValuePtrByPropertyNameInChain(const RC::Unreal::TCHAR* PropertyName);

        template<RC::Unreal::UObjectPointerDerivativeOrAnyNonUObject ReturnType>
        ReturnType* GetValuePtrByPropertyNameInChain(const TCHAR* PropertyName)
        {
            return static_cast<ReturnType*>(UObjectWrapper::GetValuePtrByPropertyNameInChain(PropertyName));
        }
    };
}
