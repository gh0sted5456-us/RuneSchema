#pragma once
#include <nlohmann/json_fwd.hpp>
namespace RC::Unreal { class FArrayProperty; }
namespace DragonWilds::VendorCategoryText {
// Only for the native LabeledRecipes array, not arbitrary ItemsProperty schemas.
void WriteGroups(void* row, RC::Unreal::FArrayProperty* groups,
                 const nlohmann::json& expected);
// Successful verification is silent; the merchant loader owns summary logging.
void VerifyGroups(const void* row, RC::Unreal::FArrayProperty* groups,
                  const nlohmann::json& expected);
}
