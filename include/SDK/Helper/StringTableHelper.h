#pragma once

#include <functional>
#include <limits>

namespace RC::Unreal {
    class FString;
    class UObject;
}

namespace DragonWilds::StringTableHelper {

    void ForEachEntry(const std::function<void(RC::Unreal::UObject*, RC::Unreal::FString&)>& callback,
        size_t maxTables=std::numeric_limits<size_t>::max(),size_t maxSlots=std::numeric_limits<size_t>::max());
}
