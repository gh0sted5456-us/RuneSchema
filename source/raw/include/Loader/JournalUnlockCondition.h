#pragma once

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

namespace DragonWilds::JournalUnlockCondition {
    inline bool Automatic(const nlohmann::json& body, bool declared) {
        if (body.contains("Unlock")) {
            if (!body.at("Unlock").is_boolean())
                throw std::runtime_error("Unlock must be a boolean");
            return body.at("Unlock").get<bool>();
        }
        return !declared && !body.value("UnlockOnAcquire", false)
            && !body.contains("UnlockCondition");
    }

    inline void Validate(const nlohmann::json& body) {
        if (!body.contains("UnlockCondition")) return;
        const auto& condition = body.at("UnlockCondition");
        if (!condition.is_object())
            throw std::runtime_error("UnlockCondition must be an object");
        const auto type = condition.value("UnlockType", std::string{});
        if (type.empty())
            throw std::runtime_error("UnlockCondition requires a native UnlockType");
        const auto separator = type.rfind("::");
        const std::string_view shortType(type.data() + (separator == std::string::npos ? 0 : separator + 2),
            type.size() - (separator == std::string::npos ? 0 : separator + 2));
        const auto requirePath = [&](const char* field) {
            const auto found = condition.find(field);
            if (found == condition.end() || !found->is_string()
                || found->get_ref<const std::string&>().empty()
                || found->get_ref<const std::string&>().front() != '/')
                throw std::runtime_error(std::string("UnlockCondition.") + field
                    + " requires a full cooked asset path");
        };
        if (shortType == "ItemFirstPickup") requirePath("UnlockingItemData");
        else if (shortType == "RecipeUnlocked") requirePath("UnlockingRecipe");
        else if (shortType == "Interaction") requirePath("UnlockingInteractionActor");
        else if (shortType == "AIKill") requirePath("AIData");
        // Other native enum values are validated by the live reflected enum.
        // In particular, RegionUnlocked has no confirmed donor in this runtime.
    }
}
