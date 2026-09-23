#pragma once
#include <string>
#include <nlohmann/json.hpp>
namespace RC::Unreal { class UObject; }
namespace PS::NiagaraTest {
// Manual tools only; call on the game thread.
std::string Attach(RC::Unreal::UObject* controller, RC::Unreal::UObject* pawn, const nlohmann::json& preset);
nlohmann::json Inspect();
void Remove();
void Reset();
void Unbind();
}
