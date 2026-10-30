#pragma once
#include <algorithm>
#include <cmath>
namespace PS::HelpyUiMotion {
inline constexpr double PageSeconds=.16,HomeExitSeconds=.20;
inline float Ease(double elapsed){const float t=static_cast<float>(std::clamp(elapsed/PageSeconds,0.0,1.0));return 1-(1-t)*(1-t)*(1-t);}
struct PageFade {
    int Page=-1;double Started=0;
    float Alpha(int page,double now,bool animate){
        if(Page!=page){Started=Page<0?now-PageSeconds:now;Page=page;}
        if(!animate){Started=now-PageSeconds;return 1;}
        return .6f+.4f*Ease(now-Started);
    }
};
inline float Hover(float current,bool hovered,float seconds,bool animate){
    const float target=hovered?1.f:0.f;
    if(!animate||!std::isfinite(seconds))return target;
    return target+(current-target)*std::exp(-std::clamp(seconds,0.f,.1f)/.06f);
}
}
