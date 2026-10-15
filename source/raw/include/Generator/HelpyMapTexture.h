#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace RC::Unreal {class UWorld;}
namespace PS::HelpyMapTexture {
nlohmann::json Render(RC::Unreal::UWorld*,const std::string&);
void Reset() noexcept;
}
