#pragma once
#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "Core/SpawnFootprint.h"

namespace PS::Grounding {
inline bool SameNearbyFloor(double original,double candidate) {
    return std::isfinite(original)&&std::isfinite(candidate)
        &&candidate<=original+45.0&&candidate>=original-100.0;
}
// Centimetres. Deterministic, exact-first and bounded: 1 + 3*8 probes.
// Query owns native floor/footprint/occupancy validation. A binding failure
// must throw, never masquerade as a blocked candidate.
struct NearbyPolicy {
    double Radius=300;
    static NearbyPolicy Parse(const nlohmann::json& entry) {
        NearbyPolicy result;
        if(!entry.contains("GroundMigrationRadius"))return result;
        const auto& value=entry.at("GroundMigrationRadius");
        if(!value.is_number())throw std::runtime_error("GroundMigrationRadius must be numeric centimetres");
        result.Radius=value.get<double>();result.Validate();return result;
    }
    void Validate()const {
        if(!std::isfinite(Radius)||Radius<0||Radius>1000)
            throw std::runtime_error("GroundMigrationRadius must be finite and between 0 and 1000 centimetres");
    }
};
// Optional author-supplied, unscaled clearance envelope above the placement
// surface, for props/resources that do not expose an owned native capsule.
inline std::optional<PS::SpawnFootprint::Capsule> Clearance(const nlohmann::json& entry) {
    if(!entry.contains("GroundClearance"))return {};
    const auto& shape=entry.at("GroundClearance");
    if(!shape.is_object()||shape.size()!=2||!shape.contains("Radius")||!shape.contains("HalfHeight")
        ||!shape.at("Radius").is_number()||!shape.at("HalfHeight").is_number())
        throw std::runtime_error("GroundClearance requires numeric Radius and HalfHeight in centimetres");
    return PS::SpawnFootprint::Scaled({shape.at("Radius").get<double>(),shape.at("HalfHeight").get<double>()},1);
}
enum class SiteState { Safe, Blocked, Pending };
struct Site { SiteState State=SiteState::Pending;std::array<double,3> Position{}; };
template<class Query> std::optional<Site> Nearby(NearbyPolicy policy,Query&& query) {
    policy.Validate();
    auto exact=query(0.0,0.0);
    if(exact.State==SiteState::Safe){
        for(double coordinate:exact.Position)if(!std::isfinite(coordinate))throw std::runtime_error("Exact placement returned a non-finite location");
        return exact;
    }
    // Missing streamed collision is not evidence of an obstructed location.
    if(exact.State==SiteState::Pending||policy.Radius==0)return {};
    constexpr double diagonal=0.7071067811865475244;
    constexpr std::array directions{std::array{1.0,0.0},std::array{diagonal,diagonal},
        std::array{0.0,1.0},std::array{-diagonal,diagonal},std::array{-1.0,0.0},
        std::array{-diagonal,-diagonal},std::array{0.0,-1.0},std::array{diagonal,-diagonal}};
    for(int ring=1;ring<=3;++ring)for(const auto& direction:directions) {
        const double radius=policy.Radius*ring/3.0;
        auto site=query(direction[0]*radius,direction[1]*radius);
        if(site.State==SiteState::Safe) {
            for(double coordinate:site.Position)if(!std::isfinite(coordinate))throw std::runtime_error("Nearby placement returned a non-finite location");
            return site;
        }
    }
    return {};
}
}
