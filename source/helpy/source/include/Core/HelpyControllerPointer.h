#pragma once
#include <cmath>
#include <algorithm>
namespace PS::HelpyControllerPointer {
struct Delta {float X=0,Y=0;};
// DeadZone is the stick radius ignored as drift. Scale slows the pointer, for example while it is over
// a control so small targets are easier to land on. Both are clamped so bad settings cannot freeze it.
struct Tuning {float DeadZone=.27f,Scale=1.f;};
inline constexpr float MinimumDeadZone=.08f,MaximumDeadZone=.5f,MinimumScale=.25f;
inline Delta Motion(int x,int y,float seconds,float speed,Tuning tuning){
    const float fx=std::clamp(x/32767.f,-1.f,1.f),fy=std::clamp(y/32767.f,-1.f,1.f);
    const float length=std::sqrt(fx*fx+fy*fy);
    const float dead=std::isfinite(tuning.DeadZone)?std::clamp(tuning.DeadZone,MinimumDeadZone,MaximumDeadZone):.27f;
    const float scale=std::isfinite(tuning.Scale)?std::clamp(tuning.Scale,MinimumScale,1.f):1.f;
    if(!std::isfinite(seconds)||!std::isfinite(speed)||length<=dead)return {};
    const float amount=std::clamp((length-dead)/(1-dead),0.f,1.f);
    const float distance=amount*amount*std::clamp(seconds,0.f,.05f)*std::clamp(speed,200.f,1800.f)*scale;
    return {fx/length*distance,-fy/length*distance};
}
inline Delta Motion(int x,int y,float seconds,float speed){return Motion(x,y,seconds,speed,{});}
}
