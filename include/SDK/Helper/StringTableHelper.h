#pragma once

#include <functional>

namespace RC::Unreal {
    class FString;
    class UObject;
}

namespace DragonWilds::StringTableHelper {

    void ForEachEntry(const std::function<void(RC::Unreal::UObject*, RC::Unreal::FString&)>& callback);
}
