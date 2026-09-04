#pragma once

namespace UECustom {
    struct FManagedValue {
    public:
        FManagedValue() {};

        FManagedValue(void* InData);

        ~FManagedValue();

        void Copy(void* InData);

        void* GetData();
    private:
        void* Data = nullptr;
    };
}
