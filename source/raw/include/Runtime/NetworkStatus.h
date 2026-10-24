#pragma once
#include "Runtime/NetworkRole.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <chrono>
#include <mutex>
#include <optional>
#include <nlohmann/json.hpp>
namespace PS::NetworkStatus {
class State {
    using Clock=std::chrono::steady_clock;
    mutable std::mutex Mutex;
    Network::Role Role=Network::Role::Unknown;
    uint64_t Generation=0;
    std::optional<size_t> Players;
    Clock::time_point Observed{};
public:
    void Publish(Network::Role role,uint64_t generation,std::optional<size_t> players={},Clock::time_point now=Clock::now()){
        std::lock_guard lock(Mutex);Role=role;Generation=generation;Players=players&&*players<=256?players:std::nullopt;Observed=now;
    }
    // Copied telemetry only: callers never query engine objects off-thread.
    nlohmann::json Snapshot(uint64_t generation,bool available,bool waiting,Clock::time_point now=Clock::now())const{
        std::lock_guard lock(Mutex);
        const auto age=std::max<int64_t>(0,std::chrono::duration_cast<std::chrono::milliseconds>(now-Observed).count());
        const bool fresh=available&&Generation==generation&&Role!=Network::Role::Unknown&&Observed!=Clock::time_point{}&&age<=12000;
        nlohmann::json result={{"Generation",generation},{"Available",available},{"Waiting",waiting},{"Fresh",fresh},
            {"Role",fresh?std::string(Network::Label(Role)):"unknown"},{"AgeMs",Observed==Clock::time_point{}?nlohmann::json{}:nlohmann::json(age)},
            {"KnownPlayers",fresh&&Players?nlohmann::json(*Players):nlohmann::json{}},{"LatencyMs",nullptr},{"PacketLoss",nullptr}};
        return result;
    }
};
inline State Current;
}
