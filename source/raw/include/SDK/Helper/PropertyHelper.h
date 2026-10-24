#pragma once

#include "nlohmann/json.hpp"
#include "SDK/StaticClassStorage.h"
#include "Utility/Logging.h"

namespace RC::Unreal {
    class UScriptStruct;
    class FProperty;
    class FFieldClass;
    class FText;
    class FNumericProperty;
    class FEnumProperty;
    class FBoolProperty;
    class FNameProperty;
    class FStrProperty;
    class FTextProperty;
    class FClassProperty;
    class FObjectProperty;
    class FSoftClassProperty;
    class FSoftObjectProperty;
    class FStructProperty;
    class FArrayProperty;
    class FMapProperty;
}

namespace DragonWilds::PropertyHelper {
    void CopyJsonValueToContainer(void* Container, RC::Unreal::FProperty* Property, const nlohmann::json& Value);

    nlohmann::json BuildAppendValue(RC::Unreal::FProperty* Property, const nlohmann::json& Items);

    int AppendJsonValuesToContainer(void* Container, RC::Unreal::UClass* Class, const nlohmann::json& AppendData);

    RC::Unreal::int64 ParseEnumFromJsonValue(RC::Unreal::FEnumProperty* Property, const nlohmann::json& Value);

    RC::Unreal::int64 ParseByteFromJsonValue(RC::Unreal::FNumericProperty* Property, const nlohmann::json& Value);

    void SetEnumPropertyValueFromJsonValue(void* Data, RC::Unreal::FEnumProperty* Property, const nlohmann::json& Value);

    void SetNumericPropertyValueFromJsonValue(void* Data, RC::Unreal::FNumericProperty* Property, const nlohmann::json& Value);

    void SetBoolPropertyValueFromJsonValue(void* Data, RC::Unreal::FBoolProperty* Property, const nlohmann::json& Value);

    void SetNamePropertyValueFromJsonValue(void* Data, RC::Unreal::FNameProperty* Property, const nlohmann::json& Value);

    void SetStrPropertyValueFromJsonValue(void* Data, RC::Unreal::FStrProperty* Property, const nlohmann::json& Value);

    void SetTextPropertyValueFromJsonValue(void* Data, RC::Unreal::FTextProperty* Property, const nlohmann::json& Value);

    void SetClassPropertyValueFromJsonValue(void* Data, RC::Unreal::FClassProperty* Property, const nlohmann::json& Value);

    void SetObjectPropertyValueFromJsonValue(void* Data, RC::Unreal::FObjectProperty* Property, const nlohmann::json& Value);

    void SetSoftClassPropertyValueFromJsonValue(void* Data, RC::Unreal::FSoftClassProperty* Property, const nlohmann::json& Value);

    void SetSoftObjectPropertyValueFromJsonValue(void* Data, RC::Unreal::FSoftObjectProperty* Property, const nlohmann::json& Value);

    void SetStructPropertyValueFromJsonValue(void* Data, RC::Unreal::FStructProperty* Property, const nlohmann::json& Value);

    void SetArrayPropertyValueFromJsonValue(void* Data, RC::Unreal::FArrayProperty* Property, const nlohmann::json& Value);

    void SetMapPropertyValueFromJsonValue(void* Data, RC::Unreal::FMapProperty* Property, const nlohmann::json& Value);

    RC::StringType GetTextAsString(const RC::Unreal::FText& Text);

    void ValidateJsonValueType(RC::Unreal::FProperty* Property, const nlohmann::json& Value);

    std::string GetPropertyNameAsUTF8String(RC::Unreal::FProperty* Property);

    std::string GetPropertyTypeAsUTF8String(RC::Unreal::FProperty* Property);

    RC::Unreal::FProperty* GetPropertyByName(RC::Unreal::UClass* Class, const RC::StringType& PropertyName);

    template <RC::Unreal::FFieldDerivative FFieldDerivedType>
    FFieldDerivedType* GetPropertyByName(RC::Unreal::UClass* Class, const RC::StringType& PropertyName)
    {
        auto Property = GetPropertyByName(Class, PropertyName);
        return CastProperty<FFieldDerivedType>(Property);
    }

    RC::Unreal::FProperty* GetPropertyByName(RC::Unreal::UScriptStruct* Struct, const RC::StringType& PropertyName);

    template <RC::Unreal::FFieldDerivative FFieldDerivedType>
    FFieldDerivedType* GetPropertyByName(RC::Unreal::UScriptStruct* Struct, const RC::StringType& PropertyName)
    {
        auto Property = GetPropertyByName(Struct, PropertyName);
        return CastProperty<FFieldDerivedType>(Property);
    }

    void* GetValuePtrByPropertyNameInChain(RC::Unreal::UObject* Instance, const RC::StringType& PropertyName);

    template<typename ReturnType>
    ReturnType* GetValuePtrByPropertyNameInChain(RC::Unreal::UObject* Instance, const RC::StringType& PropertyName)
    {
        auto ValuePtr = PropertyHelper::GetValuePtrByPropertyNameInChain(Instance, PropertyName);
        return static_cast<ReturnType*>(ValuePtr);
    }

    RC::Unreal::FFieldClass* FindFieldClassByName(const RC::Unreal::FName& Name);

    RC::Unreal::FFieldClass* FindFieldClassByName(const RC::StringType& Name);

    RC::Unreal::FField* GetNextField(RC::Unreal::FField* Field);

    RC::Unreal::TMap<RC::Unreal::FName, RC::Unreal::FFieldClass*>* GetNameToFieldClassMap();

    bool IsPropertyA(RC::Unreal::FField* Field, RC::Unreal::FFieldClass* FieldClass);

    template <RC::Unreal::FFieldDerivative FFieldDerivedType>
    bool IsPropertyA(RC::Unreal::FField* Field)
    {
        if constexpr (std::is_same_v<FFieldDerivedType, RC::Unreal::FEnumProperty>)
        {
            return IsPropertyA(Field, DragonWilds::StaticClassStorage::EnumPropertyStaticClass);
        }
        else if constexpr (std::is_same_v<FFieldDerivedType, RC::Unreal::FNumericProperty>)
        {
            return IsPropertyA(Field, DragonWilds::StaticClassStorage::NumericPropertyStaticClass);
        }
        else if constexpr (std::is_same_v<FFieldDerivedType, RC::Unreal::FBoolProperty>)
        {
            return IsPropertyA(Field, DragonWilds::StaticClassStorage::BoolPropertyStaticClass);
        }
        else if constexpr (std::is_same_v<FFieldDerivedType, RC::Unreal::FNameProperty>)
        {
            return IsPropertyA(Field, DragonWilds::StaticClassStorage::NamePropertyStaticClass);
        }
        else if constexpr (std::is_same_v<FFieldDerivedType, RC::Unreal::FStrProperty>)
        {
            return IsPropertyA(Field, DragonWilds::StaticClassStorage::StrPropertyStaticClass);
        }
        else if constexpr (std::is_same_v<FFieldDerivedType, RC::Unreal::FTextProperty>)
        {
            return IsPropertyA(Field, DragonWilds::StaticClassStorage::TextPropertyStaticClass);
        }
        else if constexpr (std::is_same_v<FFieldDerivedType, RC::Unreal::FClassProperty>)
        {
            return IsPropertyA(Field, DragonWilds::StaticClassStorage::ClassPropertyStaticClass);
        }
        else if constexpr (std::is_same_v<FFieldDerivedType, RC::Unreal::FObjectProperty>)
        {
            return IsPropertyA(Field, DragonWilds::StaticClassStorage::ObjectPropertyStaticClass);
        }
        else if constexpr (std::is_same_v<FFieldDerivedType, RC::Unreal::FSoftClassProperty>)
        {
            return IsPropertyA(Field, DragonWilds::StaticClassStorage::SoftClassPropertyStaticClass);
        }
        else if constexpr (std::is_same_v<FFieldDerivedType, RC::Unreal::FSoftObjectProperty>)
        {
            return IsPropertyA(Field, DragonWilds::StaticClassStorage::SoftObjectPropertyStaticClass);
        }
        else if constexpr (std::is_same_v<FFieldDerivedType, RC::Unreal::FStructProperty>)
        {
            return IsPropertyA(Field, DragonWilds::StaticClassStorage::StructPropertyStaticClass);
        }
        else if constexpr (std::is_same_v<FFieldDerivedType, RC::Unreal::FArrayProperty>)
        {
            return IsPropertyA(Field, DragonWilds::StaticClassStorage::ArrayPropertyStaticClass);
        }
        else if constexpr (std::is_same_v<FFieldDerivedType, RC::Unreal::FMapProperty>)
        {
            return IsPropertyA(Field, DragonWilds::StaticClassStorage::MapPropertyStaticClass);
        }
        
        return false;
    }

    template <RC::Unreal::FFieldDerivative FFieldDerivedType>
    FFieldDerivedType* CastProperty(RC::Unreal::FField* Field)
    {
        return Field != nullptr && IsPropertyA<FFieldDerivedType>(Field) ? static_cast<FFieldDerivedType*>(Field) : nullptr;
    }
}
