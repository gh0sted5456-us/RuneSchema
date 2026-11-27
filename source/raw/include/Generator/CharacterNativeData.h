#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace RC::Unreal {class UObject;}
namespace PS::CharacterNativeData {
// Game-thread-only reads of native character data for Helpy's character sheet.
nlohmann::json Choices();
nlohmann::json InventoryLayout();
nlohmann::json Skills(const nlohmann::json& draft);
std::string LoadoutSlot(int index);
// Authority game thread; requests the owning client's native provider save.
nlohmann::json SaveSkillLevel(RC::Unreal::UObject* controller,const nlohmann::json& request);
}
