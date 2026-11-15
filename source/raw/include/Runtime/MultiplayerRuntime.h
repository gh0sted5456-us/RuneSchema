#pragma once
#include <functional>
#include <nlohmann/json.hpp>

namespace RC::Unreal { class UObject; }
namespace PS::MultiplayerRuntime {
void Declare(const nlohmann::json& entries);
using SnapshotPublisher = std::function<void(const std::string&, const std::string&)>;
void Attach(RC::Unreal::UObject* gameState, SnapshotPublisher publisher);
void ObserveActor(RC::Unreal::UObject* actor, bool entering);
void Tick();
void Reset();
nlohmann::json Control(const nlohmann::json& request);
nlohmann::json Probe(const nlohmann::json& request);
bool PublishSnapshot(const std::string& owner, const std::string& channel, const std::string& payload);
}
