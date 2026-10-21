#pragma once
#include <nlohmann/json.hpp>
namespace PS::QuickLoad {
void InitializeEarly();
void Initialize();
void Shutdown() noexcept;
nlohmann::json Read();
nlohmann::json Save(const nlohmann::json& request);
}
