#include "Core/JsonLoadOrderMerge.h"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

namespace {
    std::optional<std::string> ScalarId(const nlohmann::json& o, const char* key) {
        if (!o.contains(key)) return std::nullopt;
        const auto& v = o.at(key);
        if (v.is_string()) return std::string(key) + ":" + v.get<std::string>();
        if (v.is_number_integer()) return std::string(key) + ":" + std::to_string(v.get<std::int64_t>());
        return std::nullopt;
    }
    std::optional<std::string> RefId(const nlohmann::json& o, const char* key) {
        if (!o.contains(key)) return std::nullopt;
        const auto& v = o.at(key);
        if (v.is_string()) return std::string(key) + ":" + v.get<std::string>();
        if (!v.is_object()) return std::nullopt;
        for (const auto* nested : {"ObjectPath", "AssetPathName", "InternalName", "ObjectName"})
            if (auto id = ScalarId(v, nested)) return std::string(key) + ":" + *id;
        return std::nullopt;
    }
    std::optional<std::string> StableId(const nlohmann::json& v) {
        if (!v.is_object()) return std::nullopt;
        for (const auto* key : {"ItemData", "ItemDataClass", "RecipeData", "Asset"})
            if (auto id = RefId(v, key)) return id;
        for (const auto* key : {"Identifier", "InternalName", "ObjectiveId", "StageId", "ChoiceId", "EventId", "QuestId", "SpawnGroupId", "WaveId", "RowName", "Id"})
            if (auto id = ScalarId(v, key)) return id;
        return std::nullopt;
    }
    bool IsKeyedArray(const nlohmann::json& v) {
        return v.is_array() && !v.empty() && std::all_of(v.begin(), v.end(), [](const auto& e) { return StableId(e).has_value(); });
    }
    void Merge(nlohmann::json& earlier, const nlohmann::json& later,
        DragonWilds::JsonLoadOrderMerge::MergeStats& stats) {
        if (earlier.is_object() && later.is_object()) {
            for (const auto& [key, value] : later.items()) {
                if (!earlier.contains(key)) earlier[key] = value;
                else Merge(earlier[key], value, stats);
            }
            return;
        }
        if (earlier.is_array() && later.is_array() && IsKeyedArray(earlier) && IsKeyedArray(later)) {
            for (const auto& value : later) {
                const auto id = StableId(value);
                auto found = std::find_if(earlier.begin(), earlier.end(), [&](const auto& candidate) { return StableId(candidate) == id; });
                if (found == earlier.end()) { earlier.push_back(value); ++stats.ArrayEntriesAppended; }
                else { Merge(*found, value, stats); ++stats.ArrayEntriesMerged; }
            }
            return;
        }
        if (earlier.is_array() && later.is_array()) ++stats.ArraysReplaced;
        if (earlier != later) ++stats.FieldsOverwritten;
        earlier = later;
    }
}

namespace DragonWilds::JsonLoadOrderMerge {
    MergeStats Apply(nlohmann::json& earlier, const nlohmann::json& later, bool mergeLineItems) {
        MergeStats stats{};
        if (!mergeLineItems) {
            if (earlier.is_array() && later.is_array()) ++stats.ArraysReplaced;
            if (earlier != later) ++stats.FieldsOverwritten;
            earlier = later;
        } else Merge(earlier, later, stats);
        return stats;
    }
}
