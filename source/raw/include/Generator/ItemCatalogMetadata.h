#pragma once
#include <nlohmann/json.hpp>
namespace RC::Unreal {class UObject;}
namespace PS::AssetSearch {
// Game-thread use only. Caller must verify ItemData type; no UObject lifetime is retained.
nlohmann::json DescribeItem(RC::Unreal::UObject* object);
}
