#pragma once

#include <optional>
#include <span>
#include <string>

#include "Loader/JsonLoadOrderMerge.h"
#include "nlohmann/json.hpp"

namespace DragonWilds::JsonPatchDirective
{
    struct Directive
    {
        std::string Reference;
        nlohmann::json Changes;
    };

    // Uniform injection-loader contract:
    // {
    //   "$Patch": "OwningMod:identity",
    //   "$Target": { ...fields to merge... }
    // }
    //
    // $Patch identifies an already-loaded definition/object. $Target is the
    // merge body; it is deliberately an object rather than a path expression,
    // so nested changes use the same shape as a complete definition.
    std::optional<Directive> Parse(
        const nlohmann::json& value,
        std::span<const std::string_view> protectedIdentityFields = {},
        std::string_view context = "definition");

    JsonLoadOrderMerge::MergeStats Apply(
        nlohmann::json& target,
        const Directive& directive,
        bool mergeLineItems = true);
}

