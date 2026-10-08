#include "Core/JsonPatchDirective.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_set>

namespace DragonWilds::JsonPatchDirective {
    std::optional<Directive> Parse(const nlohmann::json& value,
        std::span<const std::string_view> protectedFields, std::string_view context) {
        if (!value.is_object()) return std::nullopt;
        const bool hasPatch = value.contains("$Patch");
        const bool hasTarget = value.contains("$Target");
        if (!hasPatch && !hasTarget) return std::nullopt;
        if (!hasPatch || !hasTarget) throw std::runtime_error(std::string(context) + " patch requires both $Patch and $Target");
        if (!value.at("$Patch").is_string()) throw std::runtime_error(std::string(context) + " $Patch must be a target identity string");
        auto reference = value.at("$Patch").get<std::string>();
        if (reference.empty() || std::all_of(reference.begin(), reference.end(), [](unsigned char c) { return std::isspace(c) != 0; }))
            throw std::runtime_error(std::string(context) + " $Patch target cannot be blank");
        if (!value.at("$Target").is_object() || value.at("$Target").empty())
            throw std::runtime_error(std::string(context) + " $Target must be a non-empty object");
        for (const auto& [field, fieldValue] : value.items()) {
            if (field == "$Patch" || field == "$Target" || (field == "$Comment" && fieldValue.is_string())) continue;
            throw std::runtime_error(std::string(context) + " patch contains unsupported envelope field '" + field + "'");
        }
        static const std::unordered_set<std::string> nested{"$Patch", "$Target", "$Clone", "$Create", "$CloneFrom", "$clone"};
        for (const auto& [field, ignored] : value.at("$Target").items()) {
            if (nested.contains(field)) throw std::runtime_error(std::string(context) + " $Target cannot contain directive '" + field + "'");
            if (std::find(protectedFields.begin(), protectedFields.end(), field) != protectedFields.end())
                throw std::runtime_error(std::string(context) + " $Target cannot change identity field '" + field + "'");
        }
        return Directive{std::move(reference), value.at("$Target")};
    }
    JsonLoadOrderMerge::MergeStats Apply(nlohmann::json& target,
        const Directive& directive, bool mergeLineItems) {
        if (!target.is_object()) throw std::runtime_error("$Patch resolved to a non-object target");
        return JsonLoadOrderMerge::Apply(target, directive.Changes, mergeLineItems);
    }
}
