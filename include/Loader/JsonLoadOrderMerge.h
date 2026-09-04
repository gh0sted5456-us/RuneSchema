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

    // Applies a later-loaded JSON definition over an earlier one. Objects merge
    // recursively. Arrays of identifiable objects (for example recipe
    // ItemsConsumed rows keyed by ItemData) merge entry-by-entry; other arrays
    // are replaced as a unit. Scalars always use the later value.
    MergeStats Apply(nlohmann::json& earlier, const nlohmann::json& later,
        bool mergeLineItems = true);
}

