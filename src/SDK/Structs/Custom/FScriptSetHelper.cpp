#include "SDK/Structs/Custom/FScriptSetHelper.h"

using namespace RC;
using namespace RC::Unreal;

namespace UECustom {
    FScriptSetHelper::FScriptSetHelper(RC::Unreal::FSetProperty* InProperty, void* InSet)
    {
        ElementProp = InProperty->GetElementProp();

        SetLayout = FScriptSet::GetScriptLayout(
            ElementProp->GetSize(),
            ElementProp->GetMinAlignment()
        );

        ScriptSet = static_cast<FScriptSet*>(InSet);
    }

    bool FScriptSetHelper::Contains(const void* ElementToFind)
    {
        auto Num = ScriptSet->Num();

        if (Num < 0)
        {
            throw std::runtime_error("Failed to read TSet entry due to invalid ScriptSet.");
        }

        for (auto Index = 0; Index < ScriptSet->GetMaxIndex(); ++Index)
        {
            if (!ScriptSet->IsValidIndex(Index)) {
                continue;
            }

            void* ElementPtr = ScriptSet->GetData(Index, SetLayout);
            if (ElementProp->Identical(ElementPtr, ElementToFind))
            {
                return true;
            }
        }

        return false;
    }

    void FScriptSetHelper::Add(const void* ElementToAdd)
    {
        if (Contains(ElementToAdd))
        {
            return;
        }

        auto Index = ScriptSet->AddUninitialized(SetLayout);
        void* ElementPtr = ScriptSet->GetData(Index, SetLayout);

        ElementProp->InitializeValue(ElementPtr);

        ElementProp->CopySingleValue(ElementPtr, ElementToAdd);

        ScriptSet->Rehash(SetLayout, [this](const void* Src) {
            return ElementProp->GetValueTypeHash(Src);
        });
    }
}
