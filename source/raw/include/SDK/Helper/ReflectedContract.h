#pragma once
#include "Core/ReflectedLayout.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/FText.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include <string>
#include <vector>

namespace DragonWilds::ReflectedContract {
using namespace RC::Unreal;
struct Extent { std::string Name; int Offset,Size; };
inline void Frame(UFunction* function) {
    if(!function)throw std::runtime_error("Reflected function is unavailable");
    std::vector<Extent> fields;
    // A UFunction override owns its parameter layout. Iterating the superclass
    // as well can falsely duplicate every argument on Blueprint overrides.
    for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::IncludeDeprecated))if(field->HasAnyPropertyFlags(CPF_Parm)) {
        if(field->GetArrayDim()<1||field->GetArrayDim()>1024)throw std::runtime_error("Reflected parameter dimension is invalid");
        fields.push_back({RC::to_string(field->GetName()),field->GetOffset_Internal(),field->GetSize()});
    }
    PS::ReflectedLayout::Validate(function->GetParmsSize(),fields);
}
inline bool Fits(FProperty* property,int ownerSize) {
    return property&&property->GetArrayDim()==1&&property->GetOffset_Internal()>=0&&property->GetSize()>0
        &&ownerSize>=property->GetOffset_Internal()&&property->GetSize()<=ownerSize-property->GetOffset_Internal();
}
// FText is an SDK storage wrapper, not the live engine field size. In UE 5.6
// the native value is 16 bytes while the wrapper reserves 48 bytes. Value
// initialization/copy/destruction must remain with the reflected property.
inline bool Text(FProperty* property) {
    const auto liveSize=FText::StaticSize();
    return CastField<FTextProperty>(property)&&property->GetArrayDim()==1
        &&liveSize>0&&liveSize<=sizeof(FText)&&property->GetElementSize()==liveSize;
}
inline bool Input(FProperty* property,UFunction* function) {
    return function&&Fits(property,function->GetParmsSize())&&property->HasAnyPropertyFlags(CPF_Parm)
        &&!property->HasAnyPropertyFlags(CPF_ReturnParm)
        &&(!property->HasAnyPropertyFlags(CPF_OutParm)||property->HasAnyPropertyFlags(CPF_ReferenceParm));
}
inline bool Output(FProperty* property,UFunction* function) {
    return function&&property==function->GetReturnProperty()&&Fits(property,function->GetParmsSize())
        &&property->HasAnyPropertyFlags(CPF_Parm)&&property->HasAnyPropertyFlags(CPF_ReturnParm);
}
inline size_t Count(UFunction* function) {
    Frame(function);size_t count=0;
    for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::IncludeDeprecated))if(field->HasAnyPropertyFlags(CPF_Parm))++count;
    return count;
}
}
