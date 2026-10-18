#pragma once
#include "SDK/Helper/ReflectedContract.h"
#include "Unreal/UObject.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
namespace DragonWilds::QuestScrolls {
// Interface-inclusive lookup can return the interface declaration even when
// native Execute_UseItem will call C++ directly. Require a real script override.
inline RC::Unreal::UFunction* RequireUseEvent(RC::Unreal::UObject* item) {
    using namespace RC::Unreal;
    UFunction* function=nullptr;
    if(item)for(auto* candidate:TFieldRange<UFunction>(item->GetClassPrivate(),EFieldIterationFlags::IncludeSuper))
        if(candidate->GetFName()==FName(TEXT("UseItem"),FNAME_Add)){function=candidate;break;}
    if(!function||function->HasAnyFunctionFlags(FUNC_Native)||function->HasAnyFunctionFlags(FUNC_BlueprintPure))throw std::runtime_error("$QuestScroll requires an impure cooked Blueprint UseItem override (inert body returning false); native-only C++ and pure reevaluated functions are not a supported action route");
    auto* result=CastField<FBoolProperty>(function->GetReturnProperty());
    auto* controller=CastField<FObjectPropertyBase>(function->FindProperty(FName(TEXT("PlayerController"),FNAME_Find)));
    auto* inventory=CastField<FObjectPropertyBase>(function->FindProperty(FName(TEXT("Inventory"),FNAME_Find)));
    auto* slot=CastField<FIntProperty>(function->FindProperty(FName(TEXT("SlotIndex"),FNAME_Find)));
    const auto object=[&](FObjectPropertyBase* field,const TCHAR* type){return ReflectedContract::Input(field,function)&&field->GetElementSize()==sizeof(UObject*)&&field->GetPropertyClass().Get()&&field->GetPropertyClass()->GetPathName()==type;};
    if(ReflectedContract::Count(function)!=4||!ReflectedContract::Output(result,function)||!result->IsNativeBool()||result->GetElementSize()!=1
        ||!object(controller,TEXT("/Script/Dominion.DominionPlayerController"))||!object(inventory,TEXT("/Script/Dominion.InventoryComponent"))
        ||!ReflectedContract::Input(slot,function)||slot->GetElementSize()!=4)throw std::runtime_error("Quest-scroll Blueprint UseItem does not match UsableItemInterface");
    return function;
}
}
