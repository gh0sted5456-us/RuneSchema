#include "Loader/AssetConsumablePackSchema.h"

#include <stdexcept>

#include "Loader/SpawnSchemaFields.h"

namespace DragonWilds
{
    std::vector<ConsumablePackDrop> ParseConsumablePackDrops(
        const nlohmann::json& pack)
    {
        if (!pack.is_object())
            throw std::runtime_error("$ConsumablePack must be an object");
        // Accept RuneSchema's concise Drops form and the native cooked
        // emitter form exported by FModel ("Items to Drop").  Keeping both
        // spellings here lets a custom $create/$clone bag mirror a vanilla
        // pack without forcing authors to hand-convert every row.
        const nlohmann::json* entries = nullptr;
        if (pack.contains("Drops")) entries = &pack.at("Drops");
        else if (pack.contains("Items to Drop")) entries = &pack.at("Items to Drop");
        else if (pack.contains("ItemsToDrop")) entries = &pack.at("ItemsToDrop");
        if (!entries || !entries->is_array() || entries->empty())
            throw std::runtime_error(
                "$ConsumablePack requires a non-empty Drops or Items to Drop array");

        std::vector<ConsumablePackDrop> result;
        for (const auto& rawValue : *entries)
        {
            auto value = rawValue;
            if (!value.is_object())
                throw std::runtime_error("each consumable-pack drop must be an object");
            ConsumablePackDrop drop;
            // Native rows wrap the item path in ItemDataClass.AssetPathName.
            // Flatten that wrapper before applying the normal aliases.
            if (value.contains("ItemDataClass") && value.at("ItemDataClass").is_object())
            {
                const auto& item = value.at("ItemDataClass");
                if (item.contains("AssetPathName") && item.at("AssetPathName").is_string())
                    value["AssetPath"] = item.at("AssetPathName");
            }
            const auto identifier = SpawnSchemaFields::ReadAliasedString(value,
                {"Identifier", "identifier", "InternalName", "internal_name",
                    "Name", "name", "AssetPath", "asset_path"},
                "consumable-pack drop identifier");
            if (!identifier)
                throw std::runtime_error(
                    "consumable-pack drop requires Identifier, InternalName, Name, or AssetPath");
            drop.Identifier = *identifier;
            SpawnSchemaFields::ValidateIdentifier(
                drop.Identifier, "consumable-pack drop identifier");

            const bool hasQuantity = value.contains("Quantity");
            const bool hasRange = value.contains("MinQuantity")
                || value.contains("MaxQuantity") || value.contains("MinToDrop")
                || value.contains("MaxToDrop");
            if (hasQuantity && hasRange)
                throw std::runtime_error(
                    "use Quantity or MinQuantity/MaxQuantity, not both");
            const auto readInteger = [&](const char* primary, const char* alias,
                                         int fallback) {
                const auto* field = value.contains(primary) ? primary
                    : (value.contains(alias) ? alias : nullptr);
                if (!field) return fallback;
                if (!value.at(field).is_number_integer())
                    throw std::runtime_error(std::string(field) + " must be an integer");
                return value.at(field).get<int>();
            };
            if (hasQuantity)
            {
                if (!value.at("Quantity").is_number_integer())
                    throw std::runtime_error("Quantity must be an integer");
                drop.MinQuantity = drop.MaxQuantity = value.at("Quantity").get<int>();
            }
            else
            {
                drop.MinQuantity = readInteger("MinQuantity", "MinToDrop", 1);
                drop.MaxQuantity = readInteger(
                    "MaxQuantity", "MaxToDrop", drop.MinQuantity);
            }
            if (drop.MinQuantity < 1 || drop.MinQuantity > 9999
                || drop.MaxQuantity < drop.MinQuantity || drop.MaxQuantity > 9999)
                throw std::runtime_error(
                    "consumable-pack quantities must satisfy 1 <= min <= max <= 9999");

            drop.MaximumGrouping = readInteger(
                "MaximumGrouping", "MaxGrouping", drop.MaxQuantity);
            if (drop.MaximumGrouping < 1 || drop.MaximumGrouping > 9999)
                throw std::runtime_error(
                    "MaximumGrouping must be between 1 and 9999");
            if (value.contains("Probability"))
            {
                if (!value.at("Probability").is_number())
                    throw std::runtime_error("Probability must be a number");
                drop.Probability = value.at("Probability").get<double>();
            }
            else if (value.contains("ProbabilityOfDrop"))
            {
                if (!value.at("ProbabilityOfDrop").is_number())
                    throw std::runtime_error("ProbabilityOfDrop must be a number");
                drop.Probability = value.at("ProbabilityOfDrop").get<double>();
            }
            if (drop.Probability < 0.0 || drop.Probability > 1.0)
                throw std::runtime_error("Probability must be between 0 and 1");
            result.push_back(std::move(drop));
        }
        return result;
    }

    nlohmann::json BuildNativeConsumablePackDrops(
        const std::vector<ConsumablePackDrop>& drops,
        const std::function<std::optional<std::string>(const std::string&)>&
            resolveObjectPath)
    {
        auto result = nlohmann::json::array();
        for (const auto& drop : drops)
        {
            const auto objectPath = resolveObjectPath(drop.Identifier);
            if (!objectPath || objectPath->empty())
                throw std::runtime_error(
                    "consumable-pack item could not be resolved: " + drop.Identifier);
            result.push_back({
                {"ItemDataClass", {
                    {"AssetPathName", *objectPath}, {"SubPathString", ""}}},
                {"MinToDrop", drop.MinQuantity},
                {"MaxToDrop", drop.MaxQuantity},
                {"MaximumGrouping", drop.MaximumGrouping},
                {"ProbabilityOfDrop", drop.Probability},
                {"RequiredTagQuery", {
                    {"TokenStreamVersion", 0},
                    {"TagDictionary", nlohmann::json::array()},
                    {"QueryTokenStream", nlohmann::json::array()},
                    {"UserDescription", ""}, {"AutoDescription", ""}}},
                {"ProbabilityModifierAttribute", nullptr},
                {"ExtraDropPercentageForPlayerAmount", nlohmann::json::array()}
            });
        }
        return result;
    }
}

