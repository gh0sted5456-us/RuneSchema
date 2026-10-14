#pragma once

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <array>
#include <algorithm>

namespace DragonWilds::JournalUnlockCondition {
    inline constexpr std::array<std::string_view,8> Types={"PlayerStart","ItemFirstPickup","RecipeUnlocked","Interaction","AIKill","RegionUnlocked","Manual","NeverUnlock"};
    inline std::string Type(const nlohmann::json& body) {
        const auto& condition=body.at("UnlockCondition");
        if(!condition.is_object()||!condition.contains("UnlockType")||!condition.at("UnlockType").is_string())throw std::runtime_error("UnlockCondition requires a native UnlockType");
        auto type=condition.at("UnlockType").get<std::string>();
        constexpr std::string_view prefix="EJournalEntryUnlockType::";
        if(type.starts_with(prefix))type.erase(0,prefix.size());
        if(std::find(Types.begin(),Types.end(),type)==Types.end())throw std::runtime_error("Unknown journal UnlockType; use a native EJournalEntryUnlockType value");
        return type;
    }
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
        const auto shortType=Type(body);
        const auto requirePath = [&](const char* field) {
            const auto found = condition.find(field);
            if (found == condition.end() || !found->is_string()
                || found->get_ref<const std::string&>().empty()
                || found->get_ref<const std::string&>().front() != '/'
                || found->get_ref<const std::string&>().size()>1024
                || found->get_ref<const std::string&>().find("..")!=std::string::npos
                || std::any_of(found->get_ref<const std::string&>().begin(),found->get_ref<const std::string&>().end(),[](unsigned char c){return c<=32||c==127||c=='\\';}))
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
