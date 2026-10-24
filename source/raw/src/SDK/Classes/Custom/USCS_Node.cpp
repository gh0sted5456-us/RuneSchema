#include "SDK/Classes/Custom/USCS_Node.h"

namespace UECustom {
    RC::Unreal::UObject* UECustom::USCS_Node::GetComponentTemplate()
    {
        auto ComponentTemplate =
            this->GetValuePtrByPropertyNameInChain<RC::Unreal::TObjectPtr<RC::Unreal::UObject>>(STR("ComponentTemplate"));

        if (!ComponentTemplate)
        {
            return nullptr;
        }

        return (*ComponentTemplate).Get();
    }
}
