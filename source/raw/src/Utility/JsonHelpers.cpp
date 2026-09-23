#include "Utility/JsonHelpers.h"
#include "Unreal/NameTypes.hpp"
#include "Unreal/UnrealCoreStructs.hpp"
#include "Unreal/Rotator.hpp"
#include "nlohmann/json.hpp"
#include <format>
#include <stdexcept>

using namespace RC;
using namespace RC::Unreal;

namespace PS::JsonHelpers {
    void ParseRotator(const nlohmann::json& value, const std::string& fieldName, FRotator& outValue)
    {
        auto& field = value.at(fieldName);

        if (!field.is_object() || !field.contains("Pitch") || !field.contains("Yaw") || !field.contains("Roll"))
        {
            throw std::runtime_error(std::format("FRotator '{}' must be an object with fields 'Pitch', 'Yaw' and 'Roll'.", fieldName));
        }

        double pitch, yaw, roll;
        ParseDouble(field, "Pitch", pitch);
        ParseDouble(field, "Yaw", yaw);
        ParseDouble(field, "Roll", roll);

        outValue = FRotator{ pitch, yaw, roll };
    }

    void ParseVector(const nlohmann::json& value, const std::string& fieldName, FVector& outValue)
    {
        auto& field = value.at(fieldName);

        if (!field.is_object() || !field.contains("X") || !field.contains("Y") || !field.contains("Z"))
        {
            throw std::runtime_error(std::format("FVector '{}' must be an object with fields 'X', 'Y' and 'Z'.", fieldName));
        }

        double x, y, z;
        ParseDouble(field, "X", x);
        ParseDouble(field, "Y", y);
        ParseDouble(field, "Z", z);

        outValue = FVector{ x, y, z };
    }

    void ParseFName(const nlohmann::json& value, const std::string& fieldName, FName& outValue)
    {
        std::string parsedString;
        ParseString(value, fieldName, parsedString);

        auto wideString = RC::to_generic_string(parsedString);

        outValue = FName(wideString, FNAME_Add);
    }

}
