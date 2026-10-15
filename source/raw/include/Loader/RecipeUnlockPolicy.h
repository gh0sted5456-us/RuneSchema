#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace PS::RecipeUnlockPolicy {
    struct LearningPlan {bool AddVisible=false,RemoveExclusion=false;};
    inline LearningPlan LearnOnce(bool visible,bool excludedFromSave){return {!visible,excludedFromSave};}
    inline bool Automatic(const nlohmann::json& body) {
        if (body.contains("Unlock") && !body.at("Unlock").is_boolean())
            throw std::runtime_error("Recipe Unlock must be boolean");
        return body.value("Unlock", false);
    }
}
