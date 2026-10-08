#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include "nlohmann/json.hpp"

namespace DragonWilds::JsonLoadOrderMerge {
    std::optional<std::string> LineItemIdentity(const nlohmann::json& value);
    struct MergeStats {
        std::size_t FieldsOverwritten = 0;
        std::size_t ArrayEntriesMerged = 0;
        std::size_t ArrayEntriesAppended = 0;
        std::size_t ArraysReplaced = 0;
    };

    MergeStats Apply(nlohmann::json& earlier, const nlohmann::json& later,
        bool mergeLineItems = true);
}
