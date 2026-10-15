#pragma once
#include <algorithm>
#include <cmath>

namespace PS::ItemBayLayout {
inline constexpr int DefaultColumns=5,DefaultRows=4,MaxColumns=12,MaxRows=10;
inline int Columns(float available,float minimum,float gap,int preferred) {
    preferred=std::clamp(preferred,1,MaxColumns);
    if(!std::isfinite(available)||!std::isfinite(minimum)||!std::isfinite(gap)
        ||available<=0||minimum<=0||gap<0)return 1;
    const float fit=std::floor((available+gap)/(minimum+gap));
    return fit>=preferred?preferred:std::max(1,static_cast<int>(fit));
}
inline float RowsHeight(int rows,float card,float gap,float padding) {
    rows=std::clamp(rows,1,MaxRows);
    return rows*card+(rows-1)*gap+2*padding;
}
inline float PaneHeight(float desired,float available,float minimum) {
    if(!std::isfinite(available)||available<=0)return 1.0f;
    minimum=std::clamp(minimum,1.0f,std::max(1.0f,available));
    if(!std::isfinite(desired))desired=minimum;
    return std::clamp(desired,minimum,std::max(minimum,available));
}
}
