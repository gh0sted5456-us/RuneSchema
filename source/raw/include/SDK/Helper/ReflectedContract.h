#pragma once
#include "Core/ReflectedLayout.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/UFunctionStructs.hpp"
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
