#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <optional>

namespace PS::PlayerCoordinates {
class Store {
    using Clock=std::chrono::steady_clock;
    std::atomic<bool> Requested{false};std::mutex Mutex;
    std::optional<std::array<double,3>> Point,Rotation;uint64_t Generation=0;Clock::time_point Updated{};
public:
    bool TakeRequest(){return Requested.load(std::memory_order_relaxed)&&Requested.exchange(false,std::memory_order_relaxed);}
    void Publish(uint64_t generation,std::optional<std::array<double,3>> point,std::optional<std::array<double,3>> rotation=std::nullopt){
        if(point)for(const auto value:*point)if(!std::isfinite(value)||std::abs(value)>1e9){point.reset();break;}
        if(rotation)for(const auto value:*rotation)if(!std::isfinite(value)||std::abs(value)>1e9){rotation.reset();break;}
        std::lock_guard lock(Mutex);Point=point;Rotation=rotation;Generation=generation;Updated=Clock::now();
    }
    nlohmann::json Read(uint64_t generation){
        Requested=true;std::lock_guard lock(Mutex);
        if(!Point||generation!=Generation||Clock::now()-Updated>std::chrono::seconds(2))return {{"Available",false}};
        nlohmann::json result={{"Available",true},{"Location",*Point}};
        if(Rotation)result["Rotation"]=*Rotation;
        return result;
    }
};
inline Store Current;
}
