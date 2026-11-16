#pragma once
#include "Core/ModTemplateStore.h"
#include "SDK/Helper/ReflectedContract.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "Unreal/FFrame.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"

namespace PS::ModLocalServices {
using namespace RC::Unreal;
enum class Type {Bool,Int64,Int32,Byte,Double,String,Numbers,Strings};
struct Pin {const TCHAR* Name;Type Kind;bool Output=false;};
inline bool Matches(FProperty* property,Type kind){
    switch(kind){case Type::Bool:return CastField<FBoolProperty>(property);case Type::Int64:return CastField<FInt64Property>(property);
        case Type::Int32:return CastField<FIntProperty>(property);case Type::Byte:return CastField<FByteProperty>(property);
        case Type::Double:return CastField<FDoubleProperty>(property);case Type::String:return CastField<FStrProperty>(property)&&property->GetElementSize()==sizeof(FString);
        default:if(auto* array=CastField<FArrayProperty>(property)){auto* inner=array->GetInner();return inner&&inner->GetArrayDim()==1&&(kind==Type::Numbers?(CastField<FDoubleProperty>(inner)&&inner->GetElementSize()==sizeof(double)):(CastField<FStrProperty>(inner)&&inner->GetElementSize()==sizeof(FString)));}return false;
    }
}
inline void Signature(UFunction* function,const std::vector<Pin>& pins){
    namespace C=DragonWilds::ReflectedContract;
    if(!function||function->GetReturnProperty()||function->HasAnyFunctionFlags(FUNC_Net|FUNC_Native|FUNC_Exec|FUNC_BlueprintPure)||C::Count(function)!=pins.size())throw std::runtime_error("Local service function signature changed");
    size_t index=0;for(auto* property:TFieldRange<FProperty>(function,EFieldIterationFlags::IncludeDeprecated))if(property->HasAnyPropertyFlags(CPF_Parm)){
        const auto& pin=pins.at(index++);const bool array=pin.Kind==Type::Numbers||pin.Kind==Type::Strings;
        const bool direction=pin.Output?(property->HasAnyPropertyFlags(CPF_OutParm)&&!property->HasAnyPropertyFlags(CPF_ReferenceParm|CPF_ConstParm)):
            (!property->HasAnyPropertyFlags(CPF_OutParm|CPF_ReferenceParm)||(array&&property->HasAllPropertyFlags(CPF_OutParm|CPF_ReferenceParm|CPF_ConstParm)));
        if(property->GetFName()!=FName(pin.Name,FNAME_Find)||!C::Fits(property,function->GetParmsSize())||!Matches(property,pin.Kind)||!direction)throw std::runtime_error("Local service pin type, order or direction changed");
    }
}
struct Frame {
    UFunction* Function;FFrame& Stack;
    FProperty* Field(const TCHAR* name){auto* field=Function->FindProperty(FName(name,FNAME_Find));if(!DragonWilds::ReflectedContract::Fits(field,Function->GetParmsSize()))throw std::runtime_error("Local service parameter is unavailable");return field;}
    void* Address(FProperty* field,bool output){if(output)if(auto* value=FindOutParamValueAddress(Stack,field))return value;return field->ContainerPtrToValuePtr<void>(Stack.Locals());}
    bool Boolean(const TCHAR* name){auto* field=CastField<FBoolProperty>(Field(name));return field->GetPropertyValue(Address(field,false));}
    int64_t Integer(const TCHAR* name){auto* field=CastField<FNumericProperty>(Field(name));return field->GetSignedIntPropertyValue(Address(field,false));}
    double Number(const TCHAR* name){auto* field=CastField<FNumericProperty>(Field(name));return field->GetFloatingPointPropertyValue(Address(field,false));}
    static std::string StringValue(const FString& value,size_t maximum){
        const auto& chars=value.GetCharArray();if(chars.Num()==0)return {};
        if(chars.Num()<0||static_cast<size_t>(chars.Num())>maximum+1||!chars.GetData()||chars[chars.Num()-1]!=0)throw std::runtime_error("Local service string exceeds its bound");
        const auto bytes=RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));if(bytes.size()>maximum)throw std::runtime_error("Local service string exceeds its UTF-8 bound");(void)ModTemplates::Wide(bytes);return bytes;
    }
    std::string String(const TCHAR* name,size_t maximum=ModTemplates::MaximumBytes){return StringValue(*static_cast<FString*>(Address(Field(name),false)),maximum);}
    static void StringValue(void* output,const std::string& value){
        const auto wide=ModTemplates::Wide(value);FString text;text.AppendChars(wide.data(),static_cast<int32_t>(wide.size()));*static_cast<FString*>(output)=text;
    }
    void String(const TCHAR* name,const std::string& value){StringValue(Address(Field(name),true),value);}
    void Boolean(const TCHAR* name,bool value){auto* field=CastField<FBoolProperty>(Field(name));field->SetPropertyValue(Address(field,true),value);}
    void Integer(const TCHAR* name,int64_t value){auto* field=CastField<FNumericProperty>(Field(name));field->SetIntPropertyValue(Address(field,true),value);}
    void Number(const TCHAR* name,double value){auto* field=CastField<FNumericProperty>(Field(name));field->SetFloatingPointPropertyValue(Address(field,true),value);}
    ModTemplates::Json Array(const TCHAR* name,bool numbers){
        auto* field=CastField<FArrayProperty>(Field(name));FScriptArrayHelper values(field,Address(field,false));if(values.Num()<0||values.Num()>1024)throw std::runtime_error("Local service array exceeds 1024 entries");
        auto result=ModTemplates::Json::array();size_t bytes=0;
        for(int i=0;i<values.Num();++i)if(numbers){const double value=*reinterpret_cast<const double*>(values.GetRawPtr(i));if(!std::isfinite(value))throw std::runtime_error("Array numbers must be finite");result.push_back(value);}
        else{auto value=StringValue(*reinterpret_cast<const FString*>(values.GetRawPtr(i)),ModTemplates::MaximumBytes);bytes+=value.size();if(bytes>ModTemplates::MaximumBytes)throw std::runtime_error("String array exceeds 64 KiB");result.push_back(std::move(value));}return result;
    }
    void Array(const TCHAR* name,const ModTemplates::Json& value,bool numbers){
        ModTemplates::Array(value,numbers);auto* field=CastField<FArrayProperty>(Field(name));UECustom::FScriptArrayHelper values(Address(field,true),field);values.Empty();
        for(const auto& entry:value)if(numbers){double number=entry.get<double>();values.Add(&number);}else{FString text;StringValue(&text,entry.get<std::string>());values.Add(&text);}
    }
};
}
