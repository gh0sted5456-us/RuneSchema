#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <nlohmann/json.hpp>

namespace PS::HelpyQueryService {
bool Submit(nlohmann::json request);
void Cancel(int category,std::uint64_t cancelGeneration);
std::optional<nlohmann::json> Take();
bool IsStale(const nlohmann::json& request);
bool HasPending();
std::size_t PendingCount();
void Publish(nlohmann::json response);
void Clear();
}
