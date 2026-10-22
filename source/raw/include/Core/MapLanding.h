#pragma once
#include "Core/MapProjection.h"
#include <array>
#include <vector>
namespace PS::MapLanding {
struct Position {double X=0,Y=0,Z=0;};
// Map destinations have no authored floor height. Always trace the destination
// terrain from the full map range, never from the departing player's altitude.
template<class Query> Position Resolve(const MapProjection::Projection& map,MapProjection::Point point,double radius,double halfHeight,Query&& terrain){
    if(!map.Contains(point)||!std::isfinite(radius)||!std::isfinite(halfHeight)||radius<1||radius>500||halfHeight<radius||halfHeight>1000)
        throw std::runtime_error("Player capsule or map position is invalid; no teleport attempted.");
    const double edge=radius*0.8;double highest=map.MinZ;std::array<double,5> heights{};size_t index=0;
    for(const auto offset:std::array{MapProjection::Point{0,0},MapProjection::Point{edge,0},MapProjection::Point{-edge,0},MapProjection::Point{0,edge},MapProjection::Point{0,-edge}}){
        const MapProjection::Point sample{point.X+offset.X,point.Y+offset.Y};
        if(!map.Contains(sample))throw std::runtime_error("The landing footprint extends outside the map.");
        const auto hit=terrain(sample,map.MaxZ,map.MinZ);
        if(!std::isfinite(hit.X)||!std::isfinite(hit.Y)||!std::isfinite(hit.Z)||hit.Z<map.MinZ||hit.Z>map.MaxZ||std::abs(hit.X-sample.X)>1||std::abs(hit.Y-sample.Y)>1)
            throw std::runtime_error("Destination terrain could not be verified; no teleport attempted.");
        highest=std::max(highest,hit.Z);heights[index++]=hit.Z;
    }
    // A consistent hill is not a ledge. Fit the five-point footprint to a
    // plane, limit its gradient to 40 degrees and reject discontinuities.
    // Native teleport collision checks remain authoritative after this gate.
    const double dx=(heights[1]-heights[2])/(2*edge),dy=(heights[3]-heights[4])/(2*edge);
    const double discontinuity=std::max(std::abs((heights[1]+heights[2])*.5-heights[0]),std::abs((heights[3]+heights[4])*.5-heights[0]));
    if(std::hypot(dx,dy)>.839099631||discontinuity>std::min(12.0,radius*.3))
        throw std::runtime_error("Choose a walkable slope, away from cliffs and ledges.");
    return {point.X,point.Y,highest+halfHeight+8.0};
}
inline std::vector<MapProjection::Point> Candidates(MapProjection::Point point,bool nearby){
    std::vector<MapProjection::Point> points{point};if(nearby)for(const auto offset:std::array{MapProjection::Point{200,0},MapProjection::Point{-200,0},MapProjection::Point{0,200},MapProjection::Point{0,-200},MapProjection::Point{140,140},MapProjection::Point{-140,140},MapProjection::Point{140,-140},MapProjection::Point{-140,-140}})points.push_back({point.X+offset.X,point.Y+offset.Y});return points;
}
template<class Query> Position ResolveNear(const MapProjection::Projection& map,MapProjection::Point point,double radius,double halfHeight,bool nearby,Query&& terrain){
    std::string error;for(const auto candidate:Candidates(point,nearby))try{return Resolve(map,candidate,radius,halfHeight,terrain);}catch(const std::exception& failure){error=failure.what();}
    throw std::runtime_error(error.empty()?"No verified landing beside this marker":error);
}
inline std::vector<MapProjection::Point> TravelCandidates(MapProjection::Point point,bool marker){
    std::vector<MapProjection::Point> points{point};
    for(const double radius:marker?std::array{200.,400.,800.}:std::array{100.,200.,400.})
        for(int i=0;i<8;++i){const double angle=i*0.7853981633974483;points.push_back({point.X+std::cos(angle)*radius,point.Y+std::sin(angle)*radius});}
    return points;
}
inline bool Matches(Position wanted,Position landed){
    return std::isfinite(landed.X)&&std::isfinite(landed.Y)&&std::isfinite(landed.Z)&&std::abs(wanted.X-landed.X)<=2&&std::abs(wanted.Y-landed.Y)<=2&&std::abs(wanted.Z-landed.Z)<=2;
}
}
