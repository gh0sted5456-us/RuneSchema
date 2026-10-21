#pragma once
#include <nlohmann/json.hpp>
namespace PS::ModHealthService {
void Initialize();
void Shutdown() noexcept;
nlohmann::json Read();
}
