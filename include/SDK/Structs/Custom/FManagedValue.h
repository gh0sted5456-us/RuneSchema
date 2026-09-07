#pragma once

namespace UECustom {
    struct FManagedValue {
    public:
        FManagedValue() {};

        ~FManagedValue();
        FManagedValue(const FManagedValue&) = delete;
        FManagedValue& operator=(const FManagedValue&) = delete;

        void Copy(void* InData);

        void* GetData();
    private:
        void* Data = nullptr;
    };
}
