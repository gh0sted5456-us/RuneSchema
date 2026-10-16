#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace PS::SpawnFootprint {
struct Capsule {double Radius=0,HalfHeight=0;};
inline Capsule Scaled(Capsule native,double scale){
    Capsule result{native.Radius*scale,native.HalfHeight*scale};
    if(!std::isfinite(scale)||scale<0.01||scale>100||!std::isfinite(result.Radius)||!std::isfinite(result.HalfHeight)
        ||result.Radius<0.1||result.Radius>10000||result.HalfHeight<result.Radius||result.HalfHeight>100000)
        throw std::runtime_error("Event scaled capsule footprint is invalid");
    return result;
}
// Query returns verified blocking-ground height. All five samples must pass
// before an encounter constructs its first actor; partial waves cannot retry.
template<class Query> double OriginZ(Capsule capsule,double authoredFootZ,Query&& query){
    Scaled(capsule,1);if(!std::isfinite(authoredFootZ))throw std::runtime_error("Event ground height is invalid");
    const double center=query(0.0,0.0);double highest=center,lowest=center;
    if(!std::isfinite(center))throw std::runtime_error("Event footprint has no verified center ground");
    const double edge=capsule.Radius*0.8;
    for(const auto offset:std::array{std::array{edge,0.0},std::array{-edge,0.0},std::array{0.0,edge},std::array{0.0,-edge}}){
        const auto height=query(offset[0],offset[1]);if(!std::isfinite(height))throw std::runtime_error("Event footprint ground is invalid");
        highest=std::max(highest,height);lowest=std::min(lowest,height);
    }
    if(highest-lowest>std::min(50.0,capsule.Radius*0.75))throw std::runtime_error("Event footprint crosses a cliff or unsupported step");
    return highest+(authoredFootZ-center)+capsule.HalfHeight;
}
}
