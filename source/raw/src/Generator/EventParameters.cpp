#include "Generator/EventParameters.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Helpers/Casting.hpp"
#include "Helpers/String.hpp"
#include <cmath>
#include <cstddef>

using namespace RC;
using namespace RC::Unreal;
using nlohmann::json;
namespace PS::InspectionTools {
namespace {
json Omit(const char* reason) { return {{"omitted",reason}}; }
json ReadInline(FProperty* field, std::byte* container, size_t available,
    unsigned depth, unsigned& budget) {
    if (!field || !container || depth > 4 || !budget) return Omit("capture limit");
    --budget;
    const auto offset=field->GetOffset_Internal(), size=field->GetElementSize();
    if (field->GetArrayDim()!=1 || offset<0 || size<=0
        || static_cast<size_t>(offset)>available
        || static_cast<size_t>(size)>available-static_cast<size_t>(offset))
        return Omit("invalid or unsupported layout");
    auto* address=container+offset;
    if (auto* value=CastField<FBoolProperty>(field)) {
        // Packed bool layouts can access a byte outside ElementSize; skip them.
        if (!value->IsNativeBool() || size!=sizeof(bool) || value->GetByteOffset()!=0) return Omit("packed bool");
        return value->GetPropertyValue(address);
    }
    auto* numeric=CastField<FNumericProperty>(field);
    if (auto* value=CastField<FEnumProperty>(field)) numeric=value->GetUnderlyingProp();
    if (numeric) {
        if (numeric->GetElementSize()!=size || (size!=1 && size!=2 && size!=4 && size!=8))
            return Omit("numeric size mismatch");
        if (numeric->IsInteger()) {
            if (CastField<FByteProperty>(numeric) || CastField<FUInt16Property>(numeric)
                || CastField<FUInt32Property>(numeric) || CastField<FUInt64Property>(numeric))
                return numeric->GetUnsignedIntPropertyValue(address);
            return numeric->GetSignedIntPropertyValue(address);
        }
        if (size!=4 && size!=8) return Omit("unsupported floating size");
        const auto number=numeric->GetFloatingPointPropertyValue(address);
        return std::isfinite(number)?json(number):Omit("non-finite number");
    }
    if (auto* value=CastField<FObjectPropertyBase>(field)) {
        if (size!=sizeof(UObject*)) return Omit("object reference size mismatch");
        auto* object=value->GetObjectPropertyValue(address);
        return object ? json(to_string(object->GetPathName())) : json(nullptr);
    }
    if (CastField<FArrayProperty>(field)) {
        if (size!=sizeof(FScriptArray)) return Omit("array header size mismatch");
        auto* array=static_cast<FScriptArray*>(static_cast<void*>(address));
        const auto count=array->Num();
        if (count<0 || count>4096 || (count && !array->GetData()))
            return Omit("invalid array header");
        // Event rules only need stable container metadata. Do not retain or
        // traverse elements from the transient ProcessEvent parameter buffer.
        return {{"Count",count}};
    }
    if (auto* value=CastField<FStructProperty>(field)) {
        auto* type=value->GetStruct().Get();
        if (!type || type->GetPropertiesSize()!=size) return Omit("struct size mismatch");
        json result=json::object();
        unsigned count=0;
        for (auto* child:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
            if (++count>32 || !budget) {result["$truncated"]=true;break;}
            result[to_string(child->GetName())]=ReadInline(child,address,size,depth+1,budget);
        }
        return result;
    }
    return Omit("type not supported for parameter capture");
}
}
json CaptureEventParameters(UFunction* function,void* parameters) {
    if (!function) return Omit("missing function");
    const auto size=function->GetParmsSize();
    if (size>4096 || (size && !parameters)) return Omit("parameter buffer unavailable or too large");
    json result=json::object();
    unsigned budget=128,count=0;
    for (auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::None)) {
        if (++count>64 || !budget) {result["$truncated"]=true;break;}
        if (!field->HasAnyPropertyFlags(CPF_Parm) || field->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
        result[to_string(field->GetName())]=ReadInline(field,static_cast<std::byte*>(parameters),size,0,budget);
    }
    return result;
}
}
