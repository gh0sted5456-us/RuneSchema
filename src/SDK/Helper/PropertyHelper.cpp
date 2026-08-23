#include <algorithm>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/CoreUObject/UObject/FStrProperty.hpp>
#include <Unreal/Property/FEnumProperty.hpp>
#include <Unreal/Property/FTextProperty.hpp>
#include <Unreal/FText.hpp>
#include "Helpers/Casting.hpp"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "SDK/Classes/TSoftClassPtr.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Structs/Custom/FManagedValue.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/DragonWildsSignatures.h"
#include "Utility/Logging.h"
#include "Utility/ObjectPath.h"

using namespace RC;
using namespace RC::Unreal;

namespace {
    RC::StringType GetReferencePath(const nlohmann::json& value, bool classReference)
    {
        RC::StringType objectPath;
        RC::StringType objectName;

        if (value.is_string())
        {
            objectPath = RC::to_generic_string(value.get<std::string>());
        }
        else if (value.is_object())
        {
            if (value.contains("ObjectPath") && value.at("ObjectPath").is_string())
            {
                objectPath = RC::to_generic_string(value.at("ObjectPath").get<std::string>());
            }
            else if (value.contains("AssetPathName") && value.at("AssetPathName").is_string())
            {
                objectPath = RC::to_generic_string(value.at("AssetPathName").get<std::string>());
            }

            if (value.contains("ObjectName") && value.at("ObjectName").is_string())
            {
                auto exportText = RC::to_generic_string(value.at("ObjectName").get<std::string>());
                auto firstQuote = exportText.find(TEXT('\''));
                auto lastQuote = exportText.find_last_of(TEXT('\''));
                objectName = firstQuote != RC::StringType::npos && lastQuote > firstQuote
                    ? exportText.substr(firstQuote + 1, lastQuote - firstQuote - 1)
                    : exportText;
            }

            if (value.contains("SubPathString") && value.at("SubPathString").is_string())
            {
                auto subPath = RC::to_generic_string(value.at("SubPathString").get<std::string>());
                if (!objectPath.empty() && !subPath.empty() && objectPath.find(TEXT(':')) == RC::StringType::npos)
                {
                    objectPath += TEXT(":") + subPath;
                }
            }
        }

        return objectPath.empty()
            ? objectPath
            : PS::ObjectPath::Normalize(objectPath, objectName, classReference);
    }
}

namespace DragonWilds {
    nlohmann::json PropertyHelper::BuildAppendValue(FProperty* Property, const nlohmann::json& Items)
    {
        if (auto StructProperty = CastProperty<FStructProperty>(Property))
        {
            auto StructType = StructProperty->GetStruct();
            if (StructType && GetPropertyByName(StructType.Get(), TEXT("GameplayTags")))
            {
                auto Tags = nlohmann::json::array();
                for (auto& Item : Items)
                {
                    Tags.push_back(Item.is_string() ? nlohmann::json{ { "TagName", Item } } : Item);
                }

                return { { "GameplayTags", { { "Items", std::move(Tags) } } } };
            }
        }

        return { { "Items", Items } };
    }

    int PropertyHelper::AppendJsonValuesToContainer(void* Container, UClass* Class, const nlohmann::json& AppendData)
    {
        if (!AppendData.is_object())
        {
            throw std::runtime_error("$Append must be an object");
        }

        int Written = 0;
        for (auto& [PropertyName, Items] : AppendData.items())
        {
            auto PropertyNameWide = RC::to_generic_string(PropertyName);
            if (!Items.is_array())
            {
                throw std::runtime_error(std::format("$Append value for '{}' must be an array", PropertyName));
            }

            auto Property = GetPropertyByName(Class, PropertyNameWide);
            if (!Property)
            {
                throw std::runtime_error(std::format("Append target '{}' was not found", PropertyName));
            }

            CopyJsonValueToContainer(Container, Property, BuildAppendValue(Property, Items));
            Written++;
        }

        return Written;
    }

    void PropertyHelper::CopyJsonValueToContainer(void* Container, FProperty* Property, const nlohmann::json& Value)
    {
        auto PropertyName = Property->GetName();
        auto Type = Property->GetCPPType();
        auto Class = Property->GetClass();
        auto ClassName = Class.GetName();
        auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);

        if (auto EnumProperty = CastProperty<FEnumProperty>(Property))
        {
            SetEnumPropertyValueFromJsonValue(ValuePtr, EnumProperty, Value);
        }
        else if (auto NumProperty = CastProperty<FNumericProperty>(Property))
        {
            SetNumericPropertyValueFromJsonValue(ValuePtr, NumProperty, Value);
        }
        else if (auto BoolProperty = CastProperty<FBoolProperty>(Property))
        {
            SetBoolPropertyValueFromJsonValue(ValuePtr, BoolProperty, Value);
        }
        else if (auto NameProperty = CastProperty<FNameProperty>(Property))
        {
            SetNamePropertyValueFromJsonValue(ValuePtr, NameProperty, Value);
        }
        else if (auto StrProperty = CastProperty<FStrProperty>(Property))
        {
            SetStrPropertyValueFromJsonValue(ValuePtr, StrProperty, Value);
        }
        else if (auto TextProperty = CastProperty<FTextProperty>(Property))
        {
            SetTextPropertyValueFromJsonValue(ValuePtr, TextProperty, Value);
        }
        else if (auto ClassProperty = CastProperty<FClassProperty>(Property))
        {
            SetClassPropertyValueFromJsonValue(ValuePtr, ClassProperty, Value);
        }
        else if (CastProperty<FObjectProperty>(Property) && ClassName == STR("ObjectProperty"))
        {
            auto ObjectProperty = CastProperty<FObjectProperty>(Property);
            SetObjectPropertyValueFromJsonValue(Container, ObjectProperty, Value);
        }
        else if (CastProperty<FSoftObjectProperty>(Property) && ClassName == STR("SoftObjectProperty"))
        {
            auto SoftObjectProperty = CastProperty<FSoftObjectProperty>(Property);
            SetSoftObjectPropertyValueFromJsonValue(ValuePtr, SoftObjectProperty, Value);
        }
        else if (CastProperty<FSoftClassProperty>(Property) && ClassName == STR("SoftClassProperty"))
        {
            auto SoftClassProperty = CastProperty<FSoftClassProperty>(Property);
            SetSoftClassPropertyValueFromJsonValue(ValuePtr, SoftClassProperty, Value);
        }
        else if (auto StructProperty = CastProperty<FStructProperty>(Property))
        {
            SetStructPropertyValueFromJsonValue(ValuePtr, StructProperty, Value);
        }
        else if (auto ArrayProperty = CastProperty<FArrayProperty>(Property))
        {
            SetArrayPropertyValueFromJsonValue(ValuePtr, ArrayProperty, Value);
        }
        else if (auto MapProperty = CastProperty<FMapProperty>(Property))
        {
            SetMapPropertyValueFromJsonValue(ValuePtr, MapProperty, Value);
        }
        else
        {
            PS::Log<RC::LogLevel::Warning>(STR("Unhandled property '{}' with class of {} and type of {}\n"), PropertyName, ClassName, Type.GetCharArray());
        }
    }

    static bool FindEnumValueByName(auto Enum, const std::string& Input, int64& OutValue)
    {
        auto Short = Input;
        if (auto Separator = Short.rfind("::"); Separator != std::string::npos)
        {
            Short = Short.substr(Separator + 2);
        }

        auto ShortName = FName(RC::to_generic_string(Short));
        auto QualifiedName = FName(RC::to_generic_string(std::format("{}::{}", RC::to_string(Enum->GetName()), Short)));

        for (const auto& EnumPair : Enum->GetEnumNames())
        {
            if (EnumPair.Key == ShortName || EnumPair.Key == QualifiedName)
            {
                OutValue = EnumPair.Value;
                return true;
            }
        }

        return false;
    }

    int64 PropertyHelper::ParseEnumFromJsonValue(FEnumProperty* Property, const nlohmann::json& Value)
    {
        auto PropertyName = GetPropertyNameAsUTF8String(Property);

        ValidateJsonValueType(Property, Value);

        auto Enum = Property->GetEnum();
        if (!Enum)
        {
            throw std::runtime_error(std::format("EnumProperty {} had an invalid Enum value", PropertyName));
        }

        auto ParsedValue = Value.get<std::string>();

        int64 EnumValue = 0;
        if (!FindEnumValueByName(Enum, ParsedValue, EnumValue))
        {
            throw std::runtime_error(std::format("Enum '{}' doesn't exist", ParsedValue));
        }

        return EnumValue;
    }

    int64 PropertyHelper::ParseByteFromJsonValue(FNumericProperty* Property, const nlohmann::json& Value)
    {
        auto PropertyName = GetPropertyNameAsUTF8String(Property);

        auto Enum = Property->GetIntPropertyEnum();
        if (!Enum)
        {
            throw std::runtime_error(std::format("EnumProperty {} had an invalid Enum value", PropertyName));
        }

        auto ParsedValue = Value.get<std::string>();

        int64 EnumValue = 0;
        if (!FindEnumValueByName(Enum, ParsedValue, EnumValue))
        {
            throw std::runtime_error(std::format("Enum '{}' doesn't exist", ParsedValue));
        }

        return EnumValue;
    }

    void PropertyHelper::SetEnumPropertyValueFromJsonValue(void* Data, FEnumProperty* Property, const nlohmann::json& Value)
    {
        auto EnumValue = ParseEnumFromJsonValue(Property, Value);
        FMemory::Memcpy(Data, &EnumValue, Property->GetElementSize());
    }

    void PropertyHelper::SetNumericPropertyValueFromJsonValue(void* Data, RC::Unreal::FNumericProperty* Property, const nlohmann::json& Value)
    {
        auto PropertyName = GetPropertyNameAsUTF8String(Property);
        if (!Property->IsEnum())
        {
            ValidateJsonValueType(Property, Value);
        }

        if (Property->IsEnum())
        {
            auto EnumValue = ParseByteFromJsonValue(Property, Value);
            Property->SetIntPropertyValue(Data, EnumValue);
        }
        else
        {
            if (Property->IsInteger())
            {
                Property->SetIntPropertyValue(Data, Value.get<int64>());
            }
            else if (Property->IsFloatingPoint())
            {
                Property->SetFloatingPointPropertyValue(Data, Value.get<double>());
            }
            else
            {
                PS::Log<RC::LogLevel::Warning>(STR("Unhandled Numeric Type: {}\n"), Property->GetName());
            }
        }
    }

    void PropertyHelper::SetBoolPropertyValueFromJsonValue(void* Data, RC::Unreal::FBoolProperty* Property, const nlohmann::json& Value)
    {
        ValidateJsonValueType(Property, Value);

        Property->SetPropertyValue(Data, Value.get<bool>());
    }

    void PropertyHelper::SetNamePropertyValueFromJsonValue(void* Data, RC::Unreal::FNameProperty* Property, const nlohmann::json& Value)
    {
        ValidateJsonValueType(Property, Value);

        auto ParsedValue = Value.get<std::string>();
        auto Name = FName(RC::to_generic_string(ParsedValue), FNAME_Add);
        Property->SetPropertyValue(Data, Name);
    }

    void PropertyHelper::SetStrPropertyValueFromJsonValue(void* Data, RC::Unreal::FStrProperty* Property, const nlohmann::json& Value)
    {
        ValidateJsonValueType(Property, Value);

        auto ParsedValue = Value.get<std::string>();
        auto String = FString(RC::to_generic_string(ParsedValue).c_str());
        Property->SetPropertyValue(Data, String);
    }

    void PropertyHelper::SetTextPropertyValueFromJsonValue(void* Data, RC::Unreal::FTextProperty* Property, const nlohmann::json& Value)
    {
        ValidateJsonValueType(Property, Value);

        auto StringValue = Value.get<std::string>();
        auto Text = FText(RC::to_generic_string(StringValue).c_str());
        Property->SetPropertyValue(Data, Text);
    }

    RC::StringType PropertyHelper::GetTextAsString(const RC::Unreal::FText& Text)
    {
        if (!Text.Data)
        {
            return {};
        }

        return RC::StringType(*Text.Data->GetDisplayString());
    }

    void PropertyHelper::SetClassPropertyValueFromJsonValue(void* Data, RC::Unreal::FClassProperty* Property, const nlohmann::json& Value)
    {
        if (Value.is_null())
        {
            if (Property->GetElementSize() != sizeof(UClass*))
            {
                throw std::runtime_error(std::format("Class property {} had an unexpected element size",
                    GetPropertyNameAsUTF8String(Property)));
            }

            UClass* nullClass = nullptr;
            FMemory::Memcpy(Data, &nullClass, sizeof(nullClass));
            return;
        }

        ValidateJsonValueType(Property, Value);

        auto PropertyName = GetPropertyNameAsUTF8String(Property);

        auto classPath = GetReferencePath(Value, true);
        if (classPath.empty())
        {
            throw std::runtime_error(std::format("Property {} was supplied an empty class reference", PropertyName));
        }

        auto* classObject = UECustom::UObjectGlobals::StaticFindObject(
            nullptr, nullptr, classPath.c_str(), false);
        if (!classObject)
        {
            auto softObject = UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(classPath));
            classObject = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(softObject);
        }

        if (!classObject || !classObject->IsA<UClass>())
        {
            throw std::runtime_error(std::format("Property {} was supplied an invalid class of {}",
                PropertyName, RC::to_string(classPath)));
        }

        if (Property->GetElementSize() != sizeof(UClass*))
        {
            throw std::runtime_error(std::format("Class property {} had an unexpected element size", PropertyName));
        }

        auto* resolvedClass = static_cast<UClass*>(classObject);
        FMemory::Memcpy(Data, &resolvedClass, sizeof(resolvedClass));
    }

    void PropertyHelper::SetObjectPropertyValueFromJsonValue(void* Data, RC::Unreal::FObjectProperty* Property, const nlohmann::json& Value)
    {
        if (Value.is_null())
        {
            if (Property->GetElementSize() != sizeof(UObject*))
            {
                throw std::runtime_error(std::format("Object property {} had an unexpected element size",
                    GetPropertyNameAsUTF8String(Property)));
            }

            auto* valuePtr = Property->ContainerPtrToValuePtr<void>(Data);
            UObject* nullObject = nullptr;
            FMemory::Memcpy(valuePtr, &nullObject, sizeof(nullObject));
            return;
        }

        ValidateJsonValueType(Property, Value);

        bool isReference = Value.is_string()
            || (Value.is_object() && (Value.contains("ObjectPath") || Value.contains("ObjectName")));

        if (isReference)
        {
            RC::StringType objectPath;
            RC::StringType objectName;

            if (Value.is_string())
            {
                objectPath = RC::to_generic_string(Value.get<std::string>());
            }
            else
            {
                if (Value.contains("ObjectPath") && Value.at("ObjectPath").is_string())
                {
                    objectPath = RC::to_generic_string(Value.at("ObjectPath").get<std::string>());
                }

                if (Value.contains("ObjectName") && Value.at("ObjectName").is_string())
                {
                    auto exportText = RC::to_generic_string(Value.at("ObjectName").get<std::string>());
                    auto firstQuote = exportText.find(TEXT('\''));
                    auto lastQuote = exportText.find_last_of(TEXT('\''));
                    if (firstQuote != RC::StringType::npos && lastQuote > firstQuote)
                    {
                        objectName = exportText.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                    }
                    else
                    {
                        objectName = exportText;
                    }
                }
            }

            if (!objectPath.empty())
            {
                objectPath = PS::ObjectPath::Normalize(objectPath, objectName);
            }

            UObject* referencedObject = nullptr;
            if (!objectPath.empty())
            {
                referencedObject = UECustom::UObjectGlobals::StaticFindObject(
                    nullptr, nullptr, objectPath.c_str(), false);

                if (!referencedObject)
                {
                    auto softObject = UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(objectPath));
                    referencedObject = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(softObject);
                }
            }

            if (!referencedObject && !objectName.empty())
            {
                UObjectGlobals::ForEachUObject([&](UObject* object, int32_t, int32_t) -> LoopAction {
                    if (object && object->GetName() == objectName)
                    {
                        referencedObject = object;
                        return LoopAction::Break;
                    }
                    return LoopAction::Continue;
                });
            }

            if (!referencedObject)
            {
                throw std::runtime_error(std::format("Failed to resolve object reference for property {}", GetPropertyNameAsUTF8String(Property)));
            }

            if (Property->GetElementSize() != sizeof(UObject*))
            {
                throw std::runtime_error(std::format("Object property {} had an unexpected element size", GetPropertyNameAsUTF8String(Property)));
            }

            auto* valuePtr = Property->ContainerPtrToValuePtr<void>(Data);
            FMemory::Memcpy(valuePtr, &referencedObject, sizeof(referencedObject));
            return;
        }

        auto ObjectValue = *Property->ContainerPtrToValuePtr<UObject*>(Data);
        if (ObjectValue)
        {
            for (auto& [InnerKey, InnerValue] : Value.items())
            {
                auto ObjectValue_PropertyName = RC::to_generic_string(InnerKey);
                auto ObjectValue_Property = ObjectValue->GetPropertyByNameInChain(ObjectValue_PropertyName.c_str());

                if (!ObjectValue_Property)
                {
                    ObjectValue_Property = DragonWilds::PropertyHelper::GetPropertyByName(ObjectValue->GetClassPrivate(), ObjectValue_PropertyName);
                }

                if (ObjectValue_Property)
                {
                    CopyJsonValueToContainer(ObjectValue, ObjectValue_Property, InnerValue);
                }
            }
        }
    }

    void PropertyHelper::SetSoftClassPropertyValueFromJsonValue(void* Data, RC::Unreal::FSoftClassProperty* Property, const nlohmann::json& Value)
    {
        if (Property->GetElementSize() != sizeof(UECustom::TSoftClassPtr<UClass>))
        {
            throw std::runtime_error(std::format("Soft class property {} had an unexpected element size ({} vs {})",
                GetPropertyNameAsUTF8String(Property), Property->GetElementSize(), sizeof(UECustom::TSoftClassPtr<UClass>)));
        }

        auto* Destination = static_cast<UECustom::TSoftClassPtr<UClass>*>(Data);

        if (Value.is_null())
        {
            *Destination = UECustom::TSoftClassPtr<UClass>(UECustom::FSoftObjectPath());
            return;
        }

        ValidateJsonValueType(Property, Value);

        auto String = GetReferencePath(Value, true);
        if (String.empty())
        {
            throw std::runtime_error(std::format("Property {} was supplied an empty soft class reference",
                GetPropertyNameAsUTF8String(Property)));
        }

        *Destination = UECustom::TSoftClassPtr<UClass>(UECustom::FSoftObjectPath(String));
    }

    void PropertyHelper::SetSoftObjectPropertyValueFromJsonValue(void* Data, RC::Unreal::FSoftObjectProperty* Property, const nlohmann::json& Value)
    {
        if (Property->GetElementSize() != sizeof(UECustom::TSoftObjectPtr<UObject>))
        {
            throw std::runtime_error(std::format("Soft object property {} had an unexpected element size ({} vs {})",
                GetPropertyNameAsUTF8String(Property), Property->GetElementSize(), sizeof(UECustom::TSoftObjectPtr<UObject>)));
        }

        auto* Destination = static_cast<UECustom::TSoftObjectPtr<UObject>*>(Data);

        if (Value.is_null())
        {
            *Destination = UECustom::TSoftObjectPtr<UObject>();
            return;
        }

        ValidateJsonValueType(Property, Value);

        auto PackagePath = GetReferencePath(Value, false);

        *Destination = UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(PackagePath));
    }

    void PropertyHelper::SetStructPropertyValueFromJsonValue(void* Data, RC::Unreal::FStructProperty* Property, const nlohmann::json& Value)
    {
        auto Struct = Property->GetStruct();
        if (!Struct)
        {
            throw std::runtime_error(std::format("Failed to get Struct"));
        }

        auto ParsedObject = Value;
        if (Value.is_array())
        {
            auto* gameplayTagsProperty = GetPropertyByName(Struct.Get(), TEXT("GameplayTags"));
            if (!gameplayTagsProperty)
            {
                ValidateJsonValueType(Property, Value);
            }

            auto gameplayTags = nlohmann::json::array();
            for (const auto& tag : Value)
            {
                if (tag.is_string())
                {
                    gameplayTags.push_back({ { "TagName", tag } });
                }
                else
                {
                    gameplayTags.push_back(tag);
                }
            }

            ParsedObject = nlohmann::json::object();
            ParsedObject["GameplayTags"] = std::move(gameplayTags);
        }
        else
        {
            ValidateJsonValueType(Property, Value);
        }

        FField* Field = Struct->GetChildProperties();
        while (Field)
        {
            auto FieldName = GetPropertyNameAsUTF8String(static_cast<FProperty*>(Field));
            if (ParsedObject.contains(FieldName))
            {
                CopyJsonValueToContainer(Data, static_cast<FProperty*>(Field), ParsedObject.at(FieldName));
            }

            Field = GetNextField(Field);
        }
    }

    void PropertyHelper::SetArrayPropertyValueFromJsonValue(void* Data, RC::Unreal::FArrayProperty* Property, const nlohmann::json& Value)
    {
        ValidateJsonValueType(Property, Value);

        auto ParsedValue = Value.get<nlohmann::json>();

        auto ScriptArray = static_cast<FScriptArray*>(Data);
        auto ScriptArrayHelper = UECustom::FScriptArrayHelper(ScriptArray, Property);

        auto InnerProperty = Property->GetInner();

        if (Value.is_object())
        {
            if (Value.contains("Action"))
            {
                auto Action = Value.at("Action").get<std::string>();
                if (Action == "Clear")
                {
                    ScriptArrayHelper.Empty();
                }
            }

            if (Value.contains("Items"))
            {
                if (!Value.at("Items").is_array())
                {
                    throw std::runtime_error(std::format("Field Items must be an array"));
                }

                auto Items = Value.at("Items").get<nlohmann::json::array_t>();
                for (auto& Item : Items)
                {
                    UECustom::FManagedValue ValuePtr;
                    ScriptArrayHelper.InitializeValue(ValuePtr);
                    CopyJsonValueToContainer(ValuePtr.GetData(), InnerProperty, Item);
                    ScriptArrayHelper.Add(ValuePtr);
                }
            }
        }
        else if (Value.is_array())
        {
            ScriptArrayHelper.Empty();

            auto Items = Value.get<nlohmann::json::array_t>();
            for (auto& Item : Items)
            {
                UECustom::FManagedValue ValuePtr;
                ScriptArrayHelper.InitializeValue(ValuePtr);
                CopyJsonValueToContainer(ValuePtr.GetData(), InnerProperty, Item);
                ScriptArrayHelper.Add(ValuePtr);
            }
        }
    }

    void PropertyHelper::SetMapPropertyValueFromJsonValue(void* Data, RC::Unreal::FMapProperty* Property, const nlohmann::json& Value)
    {
        ValidateJsonValueType(Property, Value);

        auto ArrayItems = Value.get<std::vector<nlohmann::json>>();

        auto KeyProperty = Property->GetKeyProp();
        auto ValueProperty = Property->GetValueProp();

        auto MapLayout = FScriptMap::GetScriptLayout(
            KeyProperty->GetSize(),
            KeyProperty->GetMinAlignment(),
            ValueProperty->GetSize(),
            ValueProperty->GetMinAlignment());

        auto ScriptMap = static_cast<Unreal::FScriptMap*>(Data);
        auto ScriptMapHelper = UECustom::FScriptMapHelper(ScriptMap, MapLayout, KeyProperty, ValueProperty);

        for (const auto& Entry : ArrayItems)
        {
            if (!Entry.contains("Key") || !Entry.contains("Value"))
            {
                throw std::runtime_error("Each TMap entry must have a 'Key' and 'Value' property.");
            }

            UECustom::FManagedValue ScopedPair;

            ScriptMapHelper.InitializePair(ScopedPair);

            CopyJsonValueToContainer(ScopedPair.GetData(), KeyProperty, Entry.at("Key"));
            CopyJsonValueToContainer(ScopedPair.GetData(), ValueProperty, Entry.at("Value"));

            ScriptMapHelper.Add(ScopedPair);
        }

        ScriptMap->Rehash(MapLayout,
        [&](const void* Src) -> uint32 {
            return KeyProperty->GetValueTypeHash(Src);
        });
    }

    void PropertyHelper::ValidateJsonValueType(RC::Unreal::FProperty* Property, const nlohmann::json& Value)
    {
        auto PropertyName = GetPropertyNameAsUTF8String(Property);
        auto PropertyClass = Property->GetClass();
        auto PropertyClassName = PropertyClass.GetName();

        if (auto EnumProperty = CastProperty<FEnumProperty>(Property))
        {
            if (!Value.is_string()) throw std::runtime_error(std::format("Property {} must be a string", PropertyName));
        }
        else if (auto NumProperty = CastProperty<FNumericProperty>(Property))
        {
            if (!Value.is_number()) throw std::runtime_error(std::format("Property {} must be a number", PropertyName));
        }
        else if (auto BoolProperty = CastProperty<FBoolProperty>(Property))
        {
            if (!Value.is_boolean()) throw std::runtime_error(std::format("Property {} must be a boolean", PropertyName));
        }
        else if (auto NameProperty = CastProperty<FNameProperty>(Property))
        {
            if (!Value.is_string()) throw std::runtime_error(std::format("Property {} must be a string", PropertyName));
        }
        else if (auto StrProperty = CastProperty<FStrProperty>(Property))
        {
            if (!Value.is_string()) throw std::runtime_error(std::format("Property {} must be a string", PropertyName));
        }
        else if (auto TextProperty = CastProperty<FTextProperty>(Property))
        {
            if (!Value.is_string()) throw std::runtime_error(std::format("Property {} must be a string", PropertyName));
        }
        else if (auto ClassProperty = CastProperty<FClassProperty>(Property))
        {
            if (!Value.is_string() && !Value.is_object()) throw std::runtime_error(std::format("Property {} must be an object or string", PropertyName));
        }
        else if (auto ObjectProperty = CastProperty<FObjectProperty>(Property) && PropertyClassName == STR("ObjectProperty"))
        {
            if (!Value.is_object() && !Value.is_string()) throw std::runtime_error(std::format("Property {} must be an object or string", PropertyName));
        }
        else if (auto SoftObjectProperty = CastProperty<FSoftObjectProperty>(Property) && PropertyClassName == STR("SoftObjectProperty"))
        {
            if (!Value.is_string() && !Value.is_object()) throw std::runtime_error(std::format("Property {} must be an object or string", PropertyName));
        }
        else if (auto SoftClassProperty = CastProperty<FSoftClassProperty>(Property) && PropertyClassName == STR("SoftClassProperty"))
        {
            if (!Value.is_string() && !Value.is_object()) throw std::runtime_error(std::format("Property {} must be an object or string", PropertyName));
        }
        else if (auto StructProperty = CastProperty<FStructProperty>(Property))
        {
            if (!Value.is_object()) throw std::runtime_error(std::format("Property {} must be an object", PropertyName));
        }
        else if (auto ArrayProperty = CastProperty<FArrayProperty>(Property))
        {
            if (!Value.is_object() && !Value.is_array()) throw std::runtime_error(std::format("Property {} must be an object or array", PropertyName));
        }
        else if (auto MapProperty = CastProperty<FMapProperty>(Property))
        {
            if (!Value.is_array()) throw std::runtime_error(std::format("Property {} must be an array of objects", PropertyName));
        }
    }

    std::string PropertyHelper::GetPropertyNameAsUTF8String(FProperty* Property)
    {
        auto PropertyName = Property->GetName();
        auto PropertyNameUTF8 = RC::to_string(PropertyName);
        return PropertyNameUTF8;
    }

    std::string PropertyHelper::GetPropertyTypeAsUTF8String(FProperty* Property)
    {
        auto PropertyType = RC::to_string(*Property->GetCPPType());
        return PropertyType;
    }

    RC::Unreal::FProperty* PropertyHelper::GetPropertyByName(RC::Unreal::UClass* Class, const RC::StringType& PropertyName)
    {
        FProperty* Property = nullptr;
        for (FProperty* It = Class->GetPropertyLink(); It != nullptr; It = It->GetPropertyLinkNext())
        {
            if (It->GetName() == PropertyName)
            {
                Property = It;
            }
        }
        return Property;
    }

    RC::Unreal::FProperty* PropertyHelper::GetPropertyByName(RC::Unreal::UScriptStruct* Struct, const RC::StringType& PropertyName)
    {
        FProperty* Property = nullptr;
        FName PropertyFName = FName(PropertyName, FNAME_Add);
        for (FProperty* It = Struct->GetPropertyLink(); It != nullptr; It = It->GetPropertyLinkNext())
        {
            if (It->GetFName() == PropertyFName)
            {
                Property = It;
            }
        }
        return Property;
    }

    void* PropertyHelper::GetValuePtrByPropertyNameInChain(RC::Unreal::UObject* Instance, const RC::StringType& PropertyName)
    {
        if (!Instance)
        {
            return nullptr;
        }

        RC::Unreal::FProperty* Property = PropertyHelper::GetPropertyByName(Instance->GetClassPrivate(), PropertyName);
        if (!Property)
        {
            return nullptr;
        }

        auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Instance);
        return ValuePtr;
    }

    RC::Unreal::FFieldClass* PropertyHelper::FindFieldClassByName(const RC::Unreal::FName& Name)
    {
        auto NameToFieldClassMap = GetNameToFieldClassMap();
        if (!NameToFieldClassMap)
        {
            return nullptr;
        }

        auto FieldClass = NameToFieldClassMap->Find(Name);
        if (!FieldClass)
        {
            return nullptr;
        }

        return *FieldClass;
    }

    FFieldClass* PropertyHelper::FindFieldClassByName(const RC::StringType& Name)
    {
        auto NewName = FName(Name, FNAME_Add);
        return FindFieldClassByName(NewName);
    }

    FField* PropertyHelper::GetNextField(FField* Field)
    {
        if (!Field)
        {
            return nullptr;
        }

        return Field->GetNextFieldAsProperty();
    }

    TMap<FName, FFieldClass*>* PropertyHelper::GetNameToFieldClassMap()
    {
        using GetNameToFieldClassMap_Signature = TMap<FName, FFieldClass*>*(*)();
        static GetNameToFieldClassMap_Signature GetNameToFieldClassMap_Internal = nullptr;

        if (!GetNameToFieldClassMap_Internal)
        {
            GetNameToFieldClassMap_Internal = reinterpret_cast<GetNameToFieldClassMap_Signature>(
                DragonWilds::SignatureManager::GetSignature("FFieldClass::GetNameToFieldClassMap")
            );
        }

        if (!GetNameToFieldClassMap_Internal)
        {
            PS::Log<LogLevel::Error>(STR("Failed to call FFieldClass::GetNameToFieldClassMap because function address was invalid.\n"));
            return nullptr;
        }

        return GetNameToFieldClassMap_Internal();
    }

    bool PropertyHelper::IsPropertyA(RC::Unreal::FField* Field, RC::Unreal::FFieldClass* FieldClass)
    {
        using IsA_Signature = bool(*)(FField*, FFieldClass*);
        static IsA_Signature IsA_Internal = nullptr;

        if (!IsA_Internal)
        {
            IsA_Internal = reinterpret_cast<IsA_Signature>(
                DragonWilds::SignatureManager::GetSignature("FField::IsA")
            );
        }

        if (!IsA_Internal)
        {
            PS::Log<LogLevel::Error>(STR("Failed to call FField::IsA because function address was invalid.\n"));
            return false;
        }

        return IsA_Internal(Field, FieldClass);
    }
}
