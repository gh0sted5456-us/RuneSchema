#pragma once
#include <optional>
#include <functional>
#include <vector>
#include "nlohmann/json.hpp"

namespace DragonWilds::JsonArrayPatch {
    struct Edit {
        std::optional<int> Index;
        nlohmann::json Match;
        nlohmann::json Target;
    };
    std::vector<Edit> Parse(const nlohmann::json& value);
    int Select(const Edit& edit, int count, const std::function<bool(int)>& matches);
}
