#pragma once

#include <cstddef>
#include "nlohmann/json.hpp"

namespace DragonWilds::JsonLoadOrderMerge {
    struct MergeStats {
        std::size_t FieldsOverwritten = 0;
        std::size_t ArrayEntriesMerged = 0;
        std::size_t ArrayEntriesAppended = 0;
        std::size_t ArraysReplaced = 0;
    };

    MergeStats Apply(nlohmann::json& earlier, const nlohmann::json& later,
        bool mergeLineItems = true);
}
