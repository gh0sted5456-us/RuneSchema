#pragma once
#include <nlohmann/json.hpp>
namespace RC::Unreal { class UFunction; }
namespace PS::InspectionTools {
// Copies supported inline values and array counts while the callback owns the
// parameter buffer. Does not traverse containers or references, load assets,
// or retain parameter addresses.
nlohmann::json CaptureEventParameters(RC::Unreal::UFunction* function, void* parameters);
}
