#pragma once

#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "nlohmann/json.hpp"

namespace DragonWilds::SpawnSchemaFields
{
    inline std::optional<std::string> ReadAliasedString(
        const nlohmann::json& object,
        std::initializer_list<std::string_view> fields,
        std::string_view semanticName)
    {
        std::optional<std::string> result;
        std::string firstField;
        for (const auto field : fields)
        {
            const auto key = std::string(field);
            if (!object.contains(key)) continue;
            if (!object.at(key).is_string())
            {
                throw std::runtime_error(key + " must be a string");
            }

            const auto value = object.at(key).get<std::string>();
            if (result && *result != value)
            {
                throw std::runtime_error(
                    std::string(semanticName) + " aliases '" + firstField
                    + "' and '" + key + "' must not disagree");
            }
            if (!result)
            {
                result = value;
                firstField = key;
            }
        }
        return result;
    }

    inline void ValidateIdentifier(
        const std::string& value, std::string_view semanticName,
        std::size_t maximumBytes = 256)
    {
        if (value.empty() || value.size() > maximumBytes)
        {
            throw std::runtime_error(std::string(semanticName)
                + " must contain 1-" + std::to_string(maximumBytes) + " UTF-8 bytes");
        }
        for (const auto character : value)
        {
            const auto byte = static_cast<unsigned char>(character);
            if (byte < 0x20 || byte == 0x7f)
            {
                throw std::runtime_error(std::string(semanticName)
                    + " must not contain control characters");
            }
        }
    }
}

