#pragma once
#include <nlohmann/json.hpp>
#include "Core/MapProjection.h"
namespace RC::Unreal {class UWorld;}
namespace PS::HelpyMapRuntime {
MapProjection::Projection Projection(RC::Unreal::UWorld* world);
nlohmann::json Snapshot(RC::Unreal::UWorld* world,const nlohmann::json& players);
nlohmann::json Buildings(const std::string& query,std::size_t offset);
}
