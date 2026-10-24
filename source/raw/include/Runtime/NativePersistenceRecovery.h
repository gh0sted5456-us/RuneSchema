#pragma once
#include <optional>
namespace RC::Unreal {class UObject;}
namespace PS::NativePersistenceRecovery {
std::optional<bool> ReadStrictMode();
void WriteStrictMode(RC::Unreal::UObject* context,bool strict);
void EnsureSessionPolicy(RC::Unreal::UObject* context) noexcept;
void ReleaseSessionPolicy(RC::Unreal::UObject* context) noexcept;
}
