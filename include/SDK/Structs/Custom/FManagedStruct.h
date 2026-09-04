#pragma once

namespace RC::Unreal {
    class UScriptStruct;
}

namespace DragonWilds {
    class FManagedStruct {
    public:
        FManagedStruct(RC::Unreal::UScriptStruct* Struct);

        ~FManagedStruct();

        void* GetData();
    private:
        void* m_data;
        RC::Unreal::UScriptStruct* m_struct;
    };
}
