#pragma once
#include "Unreal/FField.hpp"

namespace DragonWilds {
    class StaticClassStorage {
    public:
        static inline RC::Unreal::FFieldClass* EnumPropertyStaticClass;
        static inline RC::Unreal::FFieldClass* NumericPropertyStaticClass;
        static inline RC::Unreal::FFieldClass* BoolPropertyStaticClass;
        static inline RC::Unreal::FFieldClass* NamePropertyStaticClass;
        static inline RC::Unreal::FFieldClass* StrPropertyStaticClass;
        static inline RC::Unreal::FFieldClass* TextPropertyStaticClass;
        static inline RC::Unreal::FFieldClass* ClassPropertyStaticClass;
        static inline RC::Unreal::FFieldClass* ClassPtrPropertyStaticClass;
        static inline RC::Unreal::FFieldClass* ObjectPropertyStaticClass;
        static inline RC::Unreal::FFieldClass* ObjectPropertyBaseStaticClass;
        static inline RC::Unreal::FFieldClass* ObjectPtrPropertyStaticClass;
        static inline RC::Unreal::FFieldClass* SoftClassPropertyStaticClass;
        static inline RC::Unreal::FFieldClass* SoftObjectPropertyStaticClass;
        static inline RC::Unreal::FFieldClass* StructPropertyStaticClass;
        static inline RC::Unreal::FFieldClass* ArrayPropertyStaticClass;
        static inline RC::Unreal::FFieldClass* MapPropertyStaticClass;

        static void Initialize();
    };
}
