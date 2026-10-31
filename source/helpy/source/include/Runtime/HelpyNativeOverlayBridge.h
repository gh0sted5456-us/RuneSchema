#pragma once
#include <nlohmann/json.hpp>
namespace PS::HelpyNativeOverlayBridge {
nlohmann::json Call(const nlohmann::json& request);
void Shutdown() noexcept;
}
