#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
namespace PS::PartyCardLayout {
struct Line {size_t First=0,Count=0;float Offset=0;};
inline std::vector<Line> Centered(size_t count,float width,float side,float gap){
    if(!std::isfinite(width)||!std::isfinite(side)||!std::isfinite(gap)||width<=0||side<=0||gap<0||count>32)return {};
    const auto columns=std::max(1,int((width+gap)/(side+gap)));std::vector<Line> lines;
    for(size_t at=0;at<count;at+=columns){const auto n=std::min(size_t(columns),count-at);lines.push_back({at,n,std::max(0.f,(width-(n*side+(n-1)*gap))*.5f)});}return lines;
}
}
