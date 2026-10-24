#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "SDK/Helper/PropertyHelper.h"
#include "Loader/WildcardFilter/WildcardEnumFilter.h"

using namespace RC;
using namespace RC::Unreal;
using namespace DragonWilds;

namespace PS {
    WildcardEnumFilter::WildcardEnumFilter(FProperty* sourceProperty, const nlohmann::json& data)
        : WildcardFilter(sourceProperty, data)
    {
        Validate();

        if (m_value.is_array())
        {
            for (const nlohmann::json& value : m_value)
            {
                RC::StringType filterString = RC::to_generic_string(value.get<std::string>());
                FixupEnumString(filterString);
                m_enumFilterStrings.push_back(filterString);
            }
        }
        else
        {
            RC::StringType filterString = RC::to_generic_string(m_value.get<std::string>());
            FixupEnumString(filterString);
            m_enumFilterStrings.push_back(filterString);
        }
    }

    bool WildcardEnumFilter::Compare(void* container)
    {
        FEnumProperty* enumProp = PropertyHelper::CastProperty<FEnumProperty>(m_sourceProperty);
        UEnum* enumObj = enumProp->GetEnum();
        FNumericProperty* underlyingProp = enumProp->GetUnderlyingProperty();

        void* srcData = m_sourceProperty->ContainerPtrToValuePtr<void>(container);
        int64 enumValue = underlyingProp->GetSignedIntPropertyValue(srcData);
        FName enumName = enumObj->GetNameByValue(enumValue);

        for (const RC::StringType& filterString : m_enumFilterStrings)
        {
            if (enumName.ToString() == filterString)
            {
                return true;
            }
        }

        return m_enumFilterStrings.empty();
    }

    void WildcardEnumFilter::Validate()
    {
        if (m_operationType != EWildcardOperationType::Equals)
        {
            throw std::runtime_error(RC::fmt("'%S' is an enum and only supports the equals operation.", m_sourceProperty->GetName().c_str()));
        }

        if (!m_value.is_string() && !m_value.is_array())
        {
            throw std::runtime_error(RC::fmt("Value for '%S' must be a string or an array of strings.", m_sourceProperty->GetName().c_str()));
        }

        if (m_value.is_array())
        {
            int index = 0;
            for (auto& value : m_value.get<nlohmann::json::array_t>())
            {
                if (!value.is_string())
                {
                    throw std::runtime_error(RC::fmt("Array value for '%S' at index %d must be a string.", m_sourceProperty->GetName().c_str(), index));
                }
                index++;
            }
        }
    }

    void WildcardEnumFilter::FixupEnumString(RC::StringType& enumString)
    {
        if (!enumString.contains(TEXT("::")))
        {
            FEnumProperty* enumProp = PropertyHelper::CastProperty<FEnumProperty>(m_sourceProperty);
            UEnum* enumObj = enumProp->GetEnum();

            RC::StringType nameSpace = enumObj->GetName();
            RC::StringType enumName = std::format(TEXT("{}::{}"), nameSpace, enumString);

            for (const auto& enumPair : enumObj->GetEnumNames())
            {
                if (enumPair.Key.ToString() == enumName)
                {
                    enumString = enumName;
                    return;
                }
            }

            throw std::runtime_error(RC::fmt("Invalid enum '%S' supplied to field '%S'.", enumName.c_str(), m_sourceProperty->GetName().c_str()));
        }
    }
}
