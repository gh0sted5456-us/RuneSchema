#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
namespace PS::AppearanceTrace {
    void Start(uintptr_t equipment, uintptr_t customization, uintptr_t cpd);
    nlohmann::json Stop();
    void Cancel();
    void Tick();
}
