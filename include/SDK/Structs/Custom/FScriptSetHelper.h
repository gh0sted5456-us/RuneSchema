#pragma once

#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace UECustom {
    struct FScriptSetHelper {
    public:
        FScriptSetHelper(RC::Unreal::FSetProperty* InProperty, void* InSet);

        bool Contains(const void* ElementToFind);

        void Add(const void* ElementToAdd);
    private:
        RC::Unreal::FScriptSet* ScriptSet{};
        RC::Unreal::FScriptSetLayout SetLayout{};
        RC::Unreal::FProperty* ElementProp{};
    };
}
