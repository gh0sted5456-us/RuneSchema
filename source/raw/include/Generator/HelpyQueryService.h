#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>

namespace PS::HelpyQueryService {
bool Submit(nlohmann::json request);
void Cancel(int category,std::uint64_t cancelGeneration);
std::optional<nlohmann::json> Take();
bool IsStale(const nlohmann::json& request);
bool HasPending();
std::size_t PendingCount();
void Publish(nlohmann::json response);
// Game-thread metadata reads do not require a world, pawn or game renderer.
// At most one bounded window is dispatched per call; mutations keep priority.
bool PumpOne(bool mutationBusy,const std::function<nlohmann::json(const nlohmann::json&)>& handler);
void Clear();
}
