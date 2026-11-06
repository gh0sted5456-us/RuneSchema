#pragma once
#include <cmath>
#include <algorithm>
namespace PS::HelpyControllerPointer {
struct Delta {float X=0,Y=0;};
inline Delta Motion(int x,int y,float seconds,float speed){
    const float fx=std::clamp(x/32767.f,-1.f,1.f),fy=std::clamp(y/32767.f,-1.f,1.f);
    const float length=std::sqrt(fx*fx+fy*fy),dead=.27f;
    if(!std::isfinite(seconds)||!std::isfinite(speed)||length<=dead)return {};
    const float amount=std::clamp((length-dead)/(1-dead),0.f,1.f);
    const float distance=amount*amount*std::clamp(seconds,0.f,.05f)*std::clamp(speed,200.f,1800.f);
    return {fx/length*distance,-fy/length*distance};
}
}
