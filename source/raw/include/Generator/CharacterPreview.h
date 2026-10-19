#pragma once
#include <nlohmann/json.hpp>
namespace PS::CharacterPreview {
// Game-thread-only. Rendering never receives or mutates a live player pawn.
nlohmann::json Choices();
nlohmann::json InventoryLayout();
nlohmann::json Skills(const nlohmann::json& draft);
std::string LoadoutSlot(int index);
nlohmann::json Render(const nlohmann::json& appearance,const nlohmann::json& request);
void Tick();
void Reset();
}
