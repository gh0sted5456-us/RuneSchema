#pragma once

#include "Unreal/UObject.hpp"
#include "Unreal/UObjectArray.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <mutex>

namespace PS
{
    // RuneSchema-owned weak reference. This deliberately does not contain or
    // convert through UE4SS FWeakObjectPtr: UE4SS 1135 allocates a missing
    // serial through Conv_ObjectToSoftObjectReference, which is incompatible
    // with Dragonwilds' current FSoftObjectPath layout and crashes in memcpy.
    class WeakObjectHandle
    {
    public:
        int32_t ObjectIndex = -1;
        int32_t ObjectSerialNumber = 0;

        WeakObjectHandle() = default;
        explicit WeakObjectHandle(RC::Unreal::UObject* object) { Assign(object); }

        void Reset() noexcept
        {
            ObjectIndex = -1;
            ObjectSerialNumber = 0;
        }

        RC::Unreal::UObject* Get() const noexcept
        {
            using namespace RC::Unreal;
            if (ObjectIndex < 0 || ObjectSerialNumber <= 0) return nullptr;
            auto* item = FUObjectArray::IndexToObject(ObjectIndex);
            if (!item || item->GetSerialNumber() != ObjectSerialNumber) return nullptr;
            return item->GetUObject();
        }

        void Assign(RC::Unreal::UObject* object)
        {
            using namespace RC::Unreal;
            Reset();
            if (!object) return;
            const auto index = object->GetInternalIndex();
            auto* item = FUObjectArray::IndexToObject(index);
            if (!item || item->GetUObject() != object) return;

        auto& serialStorage = item->GetSerialNumber();
        auto serial = serialStorage;
        if (!serial)
        {
            static std::once_flag seedOnce;
            static std::atomic<int32_t> nextSerial{1};
            std::call_once(seedOnce, [] {
                int32_t maximum = 0;
                const auto count = FUObjectArray::GetNumElements();
                for (int32_t candidate = 0; candidate < count; ++candidate)
                    if (auto* existing = FUObjectArray::IndexToObject(candidate))
                        if (existing->GetSerialNumber() > maximum)
                            maximum = existing->GetSerialNumber();
                nextSerial.store(maximum < INT32_MAX ? maximum + 1 : 1,
                    std::memory_order_relaxed);
            });
            auto candidate = nextSerial.fetch_add(1, std::memory_order_relaxed);
            if (candidate <= 0) candidate = nextSerial.fetch_add(1, std::memory_order_relaxed);
            std::atomic_ref<int32_t> serialRef(serialStorage);
            int32_t expected = 0;
            serialRef.compare_exchange_strong(expected, candidate,
                std::memory_order_release, std::memory_order_relaxed);
            serial = expected ? expected : candidate;
        }
            ObjectIndex = index;
            ObjectSerialNumber = serial;
        }
    };

    inline WeakObjectHandle WeakObject(RC::Unreal::UObject* object)
    {
        return WeakObjectHandle(object);
    }
}
