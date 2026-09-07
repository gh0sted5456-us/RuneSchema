#pragma once
#include "Unreal/UObject.hpp"
#include <nlohmann/json.hpp>
namespace PS::PlayerTrace {
void Start(RC::Unreal::UObject* player,RC::Unreal::UObject* controller,const nlohmann::json& options);
nlohmann::json Stop();
void Tick();
void Cancel();
void Marker(const std::string& text);
}
