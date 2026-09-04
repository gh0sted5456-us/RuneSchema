#include "Loader/JsonPatchDirective.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_set>

namespace
{
    bool IsBlank(std::string_view value)
    {
        return value.empty() || std::all_of(value.begin(), value.end(),
            [](unsigned char character) { return std::isspace(character) != 0; });
    }
}

namespace DragonWilds::JsonPatchDirective
{
    std::optional<Directive> Parse(
        const nlohmann::json& value,
        std::span<const std::string_view> protectedIdentityFields,
        std::string_view context)
    {
        if (!value.is_object()) return std::nullopt;
        const bool hasPatch = value.contains("$Patch");
        const bool hasTarget = value.contains("$Target");
        if (!hasPatch && !hasTarget) return std::nullopt;
        if (!hasPatch || !hasTarget)
            throw std::runtime_error(std::string(context)
                + " patch requires both $Patch and $Target");
        if (!value.at("$Patch").is_string())
            throw std::runtime_error(std::string(context)
                + " $Patch must be a target identity string");
        auto reference = value.at("$Patch").get<std::string>();
        if (IsBlank(reference))
            throw std::runtime_error(std::string(context)
                + " $Patch target cannot be blank");
        if (!value.at("$Target").is_object()
            || value.at("$Target").empty())
            throw std::runtime_error(std::string(context)
                + " $Target must be a non-empty object");

        for (const auto& [field, fieldValue] : value.items())
        {
            if (field == "$Patch" || field == "$Target") continue;
            if (field == "$Comment" && fieldValue.is_string()) continue;
            throw std::runtime_error(std::string(context)
                + " patch contains unsupported envelope field '" + field + "'");
        }

        const auto& changes = value.at("$Target");
        static const std::unordered_set<std::string> nestedDirectives{
            "$Patch", "$Target", "$Clone", "$Create", "$CloneFrom", "$clone"
        };
        for (const auto& [field, ignored] : changes.items())
        {
            if (nestedDirectives.contains(field))
                throw std::runtime_error(std::string(context)
                    + " $Target cannot contain creation or patch directive '"
                    + field + "'");
            if (std::find(protectedIdentityFields.begin(),
                    protectedIdentityFields.end(), field)
                != protectedIdentityFields.end())
                throw std::runtime_error(std::string(context)
                    + " $Target cannot change identity field '" + field + "'");
        }

        return Directive{std::move(reference), changes};
    }

    JsonLoadOrderMerge::MergeStats Apply(
        nlohmann::json& target,
        const Directive& directive,
        bool mergeLineItems)
    {
        if (!target.is_object())
            throw std::runtime_error("$Patch resolved to a non-object target");
        return JsonLoadOrderMerge::Apply(
            target, directive.Changes, mergeLineItems);
    }
}

