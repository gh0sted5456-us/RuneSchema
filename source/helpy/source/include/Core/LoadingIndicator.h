#pragma once
#include <array>
#include <cmath>
namespace PS::LoadingIndicator {
struct Point {float X,Y;};
inline std::array<Point,4> Quad(Point center,float radius,float angle){
    const float c=std::cos(angle),s=std::sin(angle);
    std::array<Point,4> points{{{-radius,-radius},{radius,-radius},{radius,radius},{-radius,radius}}};
    for(auto& p:points)p={center.X+p.X*c-p.Y*s,center.Y+p.X*s+p.Y*c};
    return points;
}
}
