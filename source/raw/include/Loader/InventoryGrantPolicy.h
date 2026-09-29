#pragma once

#include "Runtime/Storefront.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/UObject.hpp"

namespace DragonWilds::InventoryGrantPolicy {
    class ScopedBackpackGrant {
    public:
        explicit ScopedBackpackGrant(RC::Unreal::UObject* item)
            : m_item(item)
        {
            using namespace RC::Unreal;
            if (!m_item || PS::Storefront::Current() != PS::Storefront::Kind::GamePass)
                return;
            auto* type = m_item->GetClassPrivate();
            if (!type) return;
            m_property = CastField<FBoolProperty>(
                PropertyHelper::GetPropertyByName(type, TEXT("bGoIntoHotbarOnPickup")));
            if (!m_property || m_property->GetArrayDim() != 1) {
                m_property = nullptr;
                return;
            }
            auto* address = m_property->ContainerPtrToValuePtr<void>(m_item);
            m_original = m_property->GetPropertyValue(address);
            if (!m_original) return;
            m_property->SetPropertyValue(address, false);
            m_suppressed = !m_property->GetPropertyValue(address);
        }

        ScopedBackpackGrant(const ScopedBackpackGrant&) = delete;
        ScopedBackpackGrant& operator=(const ScopedBackpackGrant&) = delete;

        ~ScopedBackpackGrant()
        {
            if (!m_property || !m_item || !m_suppressed) return;
            auto* address = m_property->ContainerPtrToValuePtr<void>(m_item);
            m_property->SetPropertyValue(address, m_original);
        }

        bool SuppressedHotbarRouting() const noexcept { return m_suppressed; }

    private:
        RC::Unreal::UObject* m_item = nullptr;
        RC::Unreal::FBoolProperty* m_property = nullptr;
        bool m_original = false;
        bool m_suppressed = false;
    };
}
