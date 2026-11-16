#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace PS::WorldEffectRules {
inline double Scalar(double current,double value) {
    if(!std::isfinite(value)||value<0||value>100000)
        throw std::runtime_error("World policy scalar is outside its contract");
    return std::max(current,value);
}
inline int Quantity(int quantity,double factor) {
    const double result=quantity*factor;
    if(!std::isfinite(result)||result<0||result>std::numeric_limits<int>::max())
        throw std::runtime_error("Resource yield overflow");
    return static_cast<int>(std::round(result));
}
}
