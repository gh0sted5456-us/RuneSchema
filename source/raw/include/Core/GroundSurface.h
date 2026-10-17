#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <optional>
#include <nlohmann/json.hpp>
namespace PS::Grounding {
struct QueryState {bool Obstructed=false;std::optional<double> FloorReference;};
// Search limits, never a placement fallback. Cell bounds only widen the query;
// the returned position must still come from a blocking physics hit.
struct SearchRange {
    double Top=5000,Bottom=-10000;
    static SearchRange Around(double anchor,double above=5000,double below=10000){
        if(!std::isfinite(anchor)||!std::isfinite(above)||!std::isfinite(below)||above<=0||below<=0)
            throw std::runtime_error("Ground search range is not finite and positive");
        SearchRange r{anchor+above,anchor-below};if(!r.Valid())throw std::runtime_error("Ground search range is invalid");return r;
    }
    bool Valid()const{return std::isfinite(Top)&&std::isfinite(Bottom)&&Top>Bottom;}
    void ObserveCell(double minZ,double maxZ){
        if(!std::isfinite(minZ)||!std::isfinite(maxZ)||maxZ<minZ||maxZ-minZ>200000)return;
        const double top=std::max(Top,maxZ+256),bottom=std::min(Bottom,minZ-256);
        if(top-bottom<=200000){Top=top;Bottom=bottom;}
    }
    bool Contains(double z)const{return Valid()&&std::isfinite(z)&&z<=Top+0.1&&z>=Bottom-0.1;}
};
// Complex collision is a bounded fallback for surfaces without simple collision.
// Binding errors are not a missing surface and must not be retried as one.
template<class Query> bool QuerySurface(Query&& query){
    if(query(false))return true;return query(true);
}
// A small upward allowance admits a stair tread, not an overhead storey.
inline constexpr double StepUp=45.0;
// An intended floor may be below a character's capsule centre or an authored
// approximation, but not dozens of metres down in a cave/under-world shell.
// Bare '$' first obtains the terrain height, so this is a local floor window,
// not a restriction on the initial terrain-discovery ray.
inline constexpr double StepDown=300.0;
inline SearchRange FloorRange(SearchRange bounds,double reference){
    if(!bounds.Valid()||!std::isfinite(reference))throw std::runtime_error("Ground reference must be finite");
    bounds.Top=std::min(bounds.Top,reference+StepUp);
    bounds.Bottom=std::max(bounds.Bottom,reference-StepDown);
    if(!bounds.Valid())throw std::runtime_error("Ground reference lies below the search range");
    return bounds;
}
inline std::optional<double> Reference(const nlohmann::json& entry,std::optional<double> fallback={}){
    if(!entry.contains("GroundReferenceZ"))return fallback;
    const auto& value=entry.at("GroundReferenceZ");
    if(!value.is_number())throw std::runtime_error("GroundReferenceZ must be a finite height in centimetres");
    const auto z=value.get<double>();
    if(!std::isfinite(z)||std::abs(z)>100000000)throw std::runtime_error("GroundReferenceZ is outside the supported world bounds");
    return z;
}
}
