#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace RC::Unreal { class UWorld; }

namespace PS::ItemIconThumbnail {
    nlohmann::json Render(RC::Unreal::UWorld* World, const std::string& IconPath);
}
