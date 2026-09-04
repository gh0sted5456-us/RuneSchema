#include "Loader/JsonLoadOrderMerge.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

namespace {
    std::optional<std::string> ScalarIdentity(
        const nlohmann::json& object, const char* key)
    {
        if (!object.contains(key)) return std::nullopt;
        const auto& value = object.at(key);
        if (value.is_string()) return std::string(key) + ":" + value.get<std::string>();
        if (value.is_number_integer())
            return std::string(key) + ":" + std::to_string(value.get<std::int64_t>());
        return std::nullopt;
    }

    std::optional<std::string> ObjectReferenceIdentity(
        const nlohmann::json& object, const char* key)
    {
        if (!object.contains(key)) return std::nullopt;
        const auto& reference = object.at(key);
        if (reference.is_string())
            return std::string(key) + ":" + reference.get<std::string>();
        if (!reference.is_object()) return std::nullopt;
        for (const auto* nested : {"ObjectPath", "AssetPathName", "InternalName", "ObjectName"})
            if (const auto identity = ScalarIdentity(reference, nested))
                return std::string(key) + ":" + *identity;
        return std::nullopt;
    }

    std::optional<std::string> StableArrayIdentity(const nlohmann::json& value)
    {
        if (!value.is_object()) return std::nullopt;
        // Recipe ingredient/product rows use ItemData. The remaining keys cover
        // RuneSchema's other explicitly identified JSON rows without treating a
        // display label or arbitrary first field as identity.
        for (const auto* key : {"ItemData", "ItemDataClass", "RecipeData", "Asset"})
            if (const auto identity = ObjectReferenceIdentity(value, key)) return identity;
        for (const auto* key : {"Identifier", "InternalName", "ObjectiveId",
                 "StageId", "ChoiceId", "EventId", "QuestId", "SpawnGroupId",
                 "WaveId", "RowName", "Id"})
            if (const auto identity = ScalarIdentity(value, key)) return identity;
        return std::nullopt;
    }

    bool IsKeyedObjectArray(const nlohmann::json& value)
    {
        if (!value.is_array() || value.empty()) return false;
        for (const auto& entry : value)
            if (!StableArrayIdentity(entry)) return false;
        return true;
    }

    void Merge(nlohmann::json& earlier, const nlohmann::json& later,
        DragonWilds::JsonLoadOrderMerge::MergeStats& stats)
    {
        if (earlier.is_object() && later.is_object())
        {
            for (const auto& [key, laterValue] : later.items())
            {
                if (!earlier.contains(key))
                {
                    earlier[key] = laterValue;
                    continue;
                }
                Merge(earlier[key], laterValue, stats);
            }
            return;
        }

        if (earlier.is_array() && later.is_array()
            && IsKeyedObjectArray(earlier) && IsKeyedObjectArray(later))
        {
            for (const auto& laterEntry : later)
            {
                const auto identity = StableArrayIdentity(laterEntry);
                auto match = std::find_if(earlier.begin(), earlier.end(),
                    [&](const nlohmann::json& candidate) {
                        return StableArrayIdentity(candidate) == identity;
                    });
                if (match == earlier.end())
                {
                    earlier.push_back(laterEntry);
                    ++stats.ArrayEntriesAppended;
                }
                else
                {
                    Merge(*match, laterEntry, stats);
                    ++stats.ArrayEntriesMerged;
                }
            }
            return;
        }

        if (earlier.is_array() && later.is_array()) ++stats.ArraysReplaced;
        if (earlier != later) ++stats.FieldsOverwritten;
        earlier = later;
    }
}

namespace DragonWilds::JsonLoadOrderMerge {
    MergeStats Apply(nlohmann::json& earlier, const nlohmann::json& later,
        bool mergeLineItems)
    {
        MergeStats stats{};
        if (!mergeLineItems)
        {
            if (earlier.is_array() && later.is_array()) ++stats.ArraysReplaced;
            if (earlier != later) ++stats.FieldsOverwritten;
            earlier = later;
            return stats;
        }
        Merge(earlier, later, stats);
        return stats;
    }
}

