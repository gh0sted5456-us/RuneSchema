#pragma once
#include <nlohmann/json.hpp>

namespace PS::SavedLoadout {
inline bool InventoryReference(const nlohmann::json& row) {
    // Native equipped items own their identity; their index can point at unrelated bag contents.
    return row.is_object()&&row.size()==1&&row.contains("PlayerInventoryItemIndex");
}
}
