#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace DragonWilds
{
    struct ConsumablePackDrop
    {
        std::string Identifier;
        int MinQuantity = 1;
        int MaxQuantity = 1;
        int MaximumGrouping = 1;
        double Probability = 1.0;
    };

    std::vector<ConsumablePackDrop> ParseConsumablePackDrops(
        const nlohmann::json& pack);

    nlohmann::json BuildNativeConsumablePackDrops(
        const std::vector<ConsumablePackDrop>& drops,
        const std::function<std::optional<std::string>(const std::string&)>&
            resolveObjectPath);
}

