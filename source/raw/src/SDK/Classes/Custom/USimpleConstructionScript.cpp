#include "SDK/Classes/Custom/USimpleConstructionScript.h"

namespace UECustom {
    RC::Unreal::TArray<USCS_Node*>& USimpleConstructionScript::GetAllNodes()
    {
        auto AllNodes = this->GetValuePtrByPropertyNameInChain<RC::Unreal::TArray<USCS_Node*>>(STR("AllNodes"));
        return *AllNodes;
    }
}
