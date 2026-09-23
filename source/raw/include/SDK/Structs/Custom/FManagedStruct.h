#pragma once

namespace RC::Unreal {
    class UScriptStruct;
}

namespace DragonWilds {
    class FManagedStruct {
    public:
        FManagedStruct(RC::Unreal::UScriptStruct* Struct);

        ~FManagedStruct();
        FManagedStruct(const FManagedStruct&) = delete;
        FManagedStruct& operator=(const FManagedStruct&) = delete;

        void* GetData();
    private:
        void* m_data;
        RC::Unreal::UScriptStruct* m_struct;
    };
}
