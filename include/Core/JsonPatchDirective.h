#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include "Core/JsonLoadOrderMerge.h"
#include "nlohmann/json.hpp"

namespace DragonWilds::JsonPatchDirective {
    struct Directive {
        std::string Reference;
        nlohmann::json Changes;
    };

    std::optional<Directive> Parse(const nlohmann::json& value,
        std::span<const std::string_view> protectedIdentityFields,
        std::string_view context);
    JsonLoadOrderMerge::MergeStats Apply(nlohmann::json& target,
        const Directive& directive, bool mergeLineItems = true);
}
