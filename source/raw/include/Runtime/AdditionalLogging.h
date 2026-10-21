#pragma once
#include <nlohmann/json.hpp>
namespace PS::AdditionalLogging {
void Initialize();
void Shutdown() noexcept;
nlohmann::json Read();
nlohmann::json Save(const nlohmann::json& request);
}
