#pragma once
#include <algorithm>
#include <cmath>

namespace PS::HelpyMapMotion {
struct Position {double X=0,Y=0,Yaw=0;};
class Smoother {
    Position From{},To{};double Started=0,Duration=0;bool Valid=false;
public:
    void Reset(){Valid=false;}
    Position At(double now)const{const auto t=Duration>0?std::clamp((now-Started)/Duration,0.,1.):1.;return {std::lerp(From.X,To.X,t),std::lerp(From.Y,To.Y,t),From.Yaw+std::remainder(To.Yaw-From.Yaw,360.)*t};}
    void Update(Position sample,double now){
        if(!std::isfinite(sample.X)||!std::isfinite(sample.Y)||!std::isfinite(sample.Yaw)||!std::isfinite(now)){Reset();return;}
        const auto elapsed=now-Started;
        const bool snap=!Valid||elapsed<0||elapsed>1||std::hypot(sample.X-To.X,sample.Y-To.Y)>5000;
        From=snap?sample:At(now);To=sample;Duration=snap?0:std::clamp(elapsed,.05,.15);Started=now;Valid=true;
    }
};
}
