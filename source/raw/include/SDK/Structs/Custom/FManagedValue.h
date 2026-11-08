#pragma once

#include <memory>

namespace RC::Unreal { class FProperty; }

namespace UECustom {
    struct FManagedValue {
    public:
        FManagedValue() = default;
        FManagedValue(const FManagedValue&) = delete;
        FManagedValue& operator=(const FManagedValue&) = delete;

        void Initialize(int size, int alignment, RC::Unreal::FProperty& property,
                        RC::Unreal::FProperty* valueProperty = nullptr, int valueOffset = 0);

        void* GetData() const;
    private:
        struct Cleanup {
            RC::Unreal::FProperty* Property = nullptr;
            RC::Unreal::FProperty* ValueProperty = nullptr;
            int ValueOffset = 0;
            void operator()(void* data) const;
        };
        std::unique_ptr<void, Cleanup> Data{nullptr, Cleanup{}};
    };
}
