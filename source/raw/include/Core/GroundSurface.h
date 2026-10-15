#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace PS::Grounding {
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
}
